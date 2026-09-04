#!/usr/bin/env python3
"""Which species actually have sprite art, per region.

    python3 tools/check_sprites.py              # every region
    python3 tools/check_sprites.py --gen 4      # one generation
    python3 tools/check_sprites.py --local      # what is packed here, not upstream

Run this BEFORE adding a generation. SpriteCollab is community art and its
coverage is not uniform: Sinnoh and Kalos are complete, Paldea is 85%. A gap is
not a failure -- the firmware falls back to a numbered placeholder -- but it has
to be a known number rather than something a player discovers, and a species
with no art anywhere should be kept out of the egg pool. Every earlier member
of its evolution family is excluded too, so an egg cannot begin a line that can
lead to a blank sprite.

Upstream is read from the GitHub API rather than by probing each dex number:
one request per hundred species instead of one per species.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
API = ('https://api.github.com/repos/PMDCollab/SpriteCollab/contents/sprite'
       '?per_page=100&page=%d')
GH_API = 'repos/PMDCollab/SpriteCollab/contents/sprite?per_page=100&page=%d'

# (name, first dex, last dex) -- the boundaries are the games', not ours
GENS = [
    ('1 Kanto', 1, 151),    ('2 Johto', 152, 251),  ('3 Hoenn', 252, 386),
    ('4 Sinnoh', 387, 493), ('5 Unova', 494, 649),  ('6 Kalos', 650, 721),
    ('7 Alola', 722, 809),  ('8 Galar', 810, 905),  ('9 Paldea', 906, 1025),
]


RAW = 'https://raw.githubusercontent.com/PMDCollab/SpriteCollab/master/sprite/%04d/AnimData.xml'


def packable(dexes):
    """Of these dex numbers, which have a sprite pack_pmd.py can actually use.

    A DIRECTORY IS NOT COVERAGE. This checked only that sprite/NNNN existed, and
    reported Kalos as 100% -- but 0668 PYROAR holds nothing except the form
    subdirectories 0000/0001, with no AnimData.xml of its own, so pack_pmd.py
    404s on it. The table said complete while the packer failed, which is the
    same "partial fetch presented as fact" this script was already fixed for
    once: it now decides which species can hatch, so it has to mean what it says.

    One HEAD per species against raw.githubusercontent, which is a CDN with no
    meaningful rate limit -- the contents API would need an authenticated call
    each and the git tree API comes back truncated (19 MB, truncated: true), so
    neither can answer this.
    """
    from concurrent.futures import ThreadPoolExecutor

    def has(d):
        # curl, not urllib: this Python's SSL store is not configured on macOS
        # and urllib raises CERTIFICATE_VERIFY_FAILED, which would look exactly
        # like "no art" if it were swallowed. The rest of this script already
        # shells out for the same reason.
        r = subprocess.run(['curl', '-sI', '-o', '/dev/null', '-w', '%{http_code}',
                            RAW % d], capture_output=True, text=True)
        code = r.stdout.strip()
        if code == '200':
            return d, True
        if code == '404':
            return d, False
        raise SystemExit('HEAD for dex %d returned %r -- refusing to report '
                         'partial coverage as fact' % (d, code))

    out = set()
    with ThreadPoolExecutor(max_workers=24) as ex:
        for d, ok in ex.map(has, sorted(x for x in dexes if x > 0)):
            if ok:
                out.add(d)
    return out


def upstream():
    """Every dex number SpriteCollab has a USABLE sprite for.

    A FAILED PAGE IS FATAL, not a break. This used to swallow any error and
    return whatever it had, so a rate-limited run reported every species in a
    generation as having no art -- a partial fetch presented as fact. That was
    survivable while this only printed a table; it is not now that the result
    decides which species are kept out of the egg pool.
    """
    # gh is authenticated and has a far higher rate limit, so prefer it and fall
    # back to curl. Anonymous API access runs out after a few hundred calls,
    # which is easy to hit in a session that is already fetching sprites.
    gh = shutil.which('gh')
    use_gh = bool(gh) and subprocess.run(
        [gh, 'auth', 'status'], capture_output=True).returncode == 0

    have = set()
    for page in range(1, 20):
        if use_gh:
            r = subprocess.run([gh, 'api', GH_API % page], capture_output=True)
        else:
            r = subprocess.run(['curl', '-sfL', API % page], capture_output=True)
        if r.returncode != 0:
            raise SystemExit('page %d failed (rate limited? no network?) -- '
                             'refusing to report partial coverage as fact' % page)
        try:
            rows = json.loads(r.stdout)
        except Exception as e:
            raise SystemExit('page %d is not JSON (%s) -- refusing to guess' % (page, e))
        if not isinstance(rows, list):
            raise SystemExit('page %d: %r -- refusing to guess' % (page, rows))
        if not rows:
            break
        for e in rows:
            n = e.get('name', '')
            if n.isdigit():
                have.add(int(n))
    if len(have) < 900:
        raise SystemExit('only %d sprite dirs found, expected ~980 -- looks '
                         'truncated, refusing to report it' % len(have))
    # A directory is only a candidate; the sprite itself decides.
    return packable(have)


def local():
    """Every dex number packed into tools/sdcard/mons here."""
    have = set()
    d = os.path.join(ROOT, 'tools', 'sdcard', 'mons')
    if not os.path.isdir(d):
        return have
    for f in os.listdir(d):
        m = re.fullmatch(r'p(?:s)?(\d+)\.bin', f)
        if m:
            have.add(int(m.group(1)))
    return have


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--gen', type=int, help='only this generation (1-9)')
    ap.add_argument('--local', action='store_true',
                    help='what is packed here rather than what exists upstream')
    ap.add_argument('--emit', action='store_true',
                    help='write noart.h, the list the egg pool skips')
    args = ap.parse_args()

    have = local() if args.local else upstream()
    if not have:
        print('nothing found -- no network, or no sprites packed yet')
        return 1

    gens = [g for g in GENS if args.gen is None or g[0].startswith(str(args.gen))]
    print('%-12s %5s %5s  %8s  %s' % ('region', 'have', 'of', 'coverage', 'missing'))
    holes = []
    for name, lo, hi in gens:
        miss = [d for d in range(lo, hi + 1) if d not in have]
        n, tot = (hi - lo + 1) - len(miss), hi - lo + 1
        shown = ' '.join(str(d) for d in miss[:8]) + ('...' if len(miss) > 8 else '')
        print('%-12s %5d %5d  %7.1f%%  %s' % (name, n, tot, 100.0 * n / tot, shown))
        holes += miss
    if holes:
        print('\n%d species have no art. They must stay in the dex at their own '
              'numbers -- dropping one renumbers everything after it -- but '
              'should be kept out of the egg pool.' % len(holes))

    if args.emit:
        import re as _re
        from dex_data import DEX, EVOLUTION_BRANCHES
        dexh = open(os.path.join(ROOT, 'dex.h'), encoding='utf-8').read()
        count = int(_re.search(r'#define DEX_COUNT (\d+)', dexh).group(1))
        inrange = sorted(d for d in holes if d <= count)

        # An egg must not start a family that is guaranteed to run into a
        # blank evolution later. Walk the ordinary and branched evolution
        # links backwards until every ancestor of an art-less species is in
        # the no-hatch set too. This does not remove anything from the dex and
        # does not affect creatures already present in a save.
        no_hatch = set(inrange)
        changed = True
        while changed:
            changed = False
            for row in DEX:
                dex, target = row[0], row[4]
                if dex <= count and target in no_hatch and dex not in no_hatch:
                    no_hatch.add(dex)
                    changed = True
            for base, targets in EVOLUTION_BRANCHES.items():
                if (base <= count and any(t in no_hatch for t in targets)
                        and base not in no_hatch):
                    no_hatch.add(base)
                    changed = True
        no_hatch = sorted(no_hatch)

        out = os.path.join(ROOT, 'noart.h')
        with open(out, 'w', encoding='utf-8') as f:
            f.write('// GENERATED by tools/check_sprites.py --emit. Do not edit.\n'
                    '//\n'
                    '// Species SpriteCollab has no art for at all. They KEEP THEIR DEX\n'
                    '// NUMBERS -- removing one would renumber every species after it, and\n'
                    '// dex numbers are positional in saved data (dexReg bits, speciesId,\n'
                    '// every party record) -- but they are kept out of the egg pool, since\n'
                    '// hatching one gives a creature that can only ever draw as a number.\n'
                    '//\n'
                    '// Re-run after any expansion, and again if upstream adds art.\n'
                    '#pragma once\n\n')
            f.write('#define NO_ART_COUNT %d\n' % len(inrange))
            f.write('static const int16_t NO_ART[NO_ART_COUNT ? NO_ART_COUNT : 1] = {\n  ')
            f.write(', '.join(str(d) for d in inrange) if inrange else '0')
            f.write('\n};\n\n')
            f.write('static inline bool speciesHasArt(int16_t d) {\n'
                    '  for (int i = 0; i < NO_ART_COUNT; i++)\n'
                    '    if (NO_ART[i] == d) return false;\n'
                    '  return true;\n'
                    '}\n\n')
            f.write('// Species that cannot hatch: the art-less entries above plus every\n'
                    '// earlier member whose evolution line can reach one of them.\n')
            f.write('#define NO_HATCH_COUNT %d\n' % len(no_hatch))
            f.write('static const int16_t NO_HATCH[NO_HATCH_COUNT ? NO_HATCH_COUNT : 1] = {\n  ')
            f.write(', '.join(str(d) for d in no_hatch) if no_hatch else '0')
            f.write('\n};\n\n')
            f.write('static inline bool speciesCanHatch(int16_t d) {\n'
                    '  for (int i = 0; i < NO_HATCH_COUNT; i++)\n'
                    '    if (NO_HATCH[i] == d) return false;\n'
                    '  return true;\n'
                    '}\n')
        print('\nwrote %s: %d species have no art; %d species cannot hatch in 1..%d' %
              (out, len(inrange), len(no_hatch), count))
    return 0


if __name__ == '__main__':
    sys.exit(main())
