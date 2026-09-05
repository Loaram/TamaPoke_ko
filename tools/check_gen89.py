#!/usr/bin/env python3
"""Validate the sourced Galar/Paldea snapshot and generated game tables."""
import json, re, sys
from pathlib import Path

R = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(R / 'tools'))
from dex_data import DEX, REGIONS, EVOLUTION_BRANCHES
from dex_stats import BASE_STATS
from dex_types import TYPES
from dex_moves import MOVES
from dex_learnsets import LEARNSETS
from evolution_moves import EVOLUTION_MOVES

data = json.loads((R / 'tools/gen89_data.json').read_text(encoding='utf-8'))
full = json.loads((R / 'tools/full_move_data.json').read_text(encoding='utf-8'))
names = json.loads((R / 'localization/names-ko.json').read_text(encoding='utf-8'))
species = data['species']

assert [s['dex'] for s in species] == list(range(810, 1026))
assert len(DEX) == len(BASE_STATS) == len(TYPES) == len(LEARNSETS) == 1025
assert REGIONS[-2:] == [('GALAR', 810, 905, [810, 813, 816]),
                        ('PALDEA', 906, 1025, [906, 909, 912])]

move_by_slug = {m[1]: m for m in MOVES if m[1]}
move_ids = {m[1]: i + 1 for i, m in enumerate(MOVES) if m[1]}
assert move_ids['slam'] == 135  # old save IDs must remain append-only
assert [move_ids[s] for s in ('drum-beating', 'pyro-ball', 'snipe-shot',
                              'flower-trick', 'torch-song', 'aqua-step')] == list(range(136, 142))
for s in species:
    d = s['dex']
    row = DEX[d - 1]
    assert row[:4] == (d, s['slug'], s['display'], s['accent_type'])
    assert BASE_STATS[d] == tuple(s['stats'])
    assert TYPES[d] == (s['type1'], s['type2'])
    assert names[s['display']] == s['ko']
    assert LEARNSETS[d] and all(slug in move_by_slug for slug, _ in LEARNSETS[d])

for m in data['moves']:
    row = move_by_slug[m['slug']]
    assert row[0] == m['display'] and row[2] == m['type']
    assert row[4] == m['power'] and row[5] == m['accuracy']
    assert names[m['display']] == m['ko']

assert full['provenance']['commit'] == 'd4f9a4af58ade123fbc0558f68b1c69daa97d9e4'
assert full['provenance']['latest_version_group'] == 'the-indigo-disk'
assert full['counts'] == {
    'species': 1025,
    'natural_rows': 14542,
    'natural_unique_moves': 689,
    'new_moves': 554,
    'evolution_species': 242,
    'evolution_rows': 253,
    'max_learnset_rows': 91,
    'max_learnset_dex': 151,
}
assert len(MOVES) == 695
assert set(map(int, full['learnsets'])) == set(range(1, 1026))
for m in full['moves']:
    row = move_by_slug[m['slug']]
    assert row[0] == m['display'] and row[2] == m['type']
    assert row[4] == m['power'] and row[5] == m['accuracy']
    assert names[m['display']] == m['ko']
for dex, rows in full['learnsets'].items():
    expected = [(slug, int(level)) for slug, level in rows]
    assert LEARNSETS[int(dex)] == expected
    assert len(expected) == len({slug for slug, _level in expected})
    assert all(slug in move_by_slug and 0 <= level <= 100 for slug, level in expected)

starter_evolution_moves = {
    812: ('drum-beating',), 815: ('pyro-ball',), 818: ('snipe-shot',),
    908: ('flower-trick',), 911: ('torch-song',), 914: ('aqua-step',),
}
assert all(EVOLUTION_MOVES[dex] == slugs for dex, slugs in starter_evolution_moves.items())
assert len(EVOLUTION_MOVES) == full['counts']['evolution_species']
assert sum(len(slugs) for slugs in EVOLUTION_MOVES.values()) == full['counts']['evolution_rows']
evolution_targets = {row[4] for row in DEX if row[4]}
evolution_targets.update(target for targets in EVOLUTION_BRANCHES.values() for target in targets)
for dex, slugs in EVOLUTION_MOVES.items():
    assert dex in evolution_targets
    assert all(slug in move_by_slug for slug in slugs)
    assert all((slug, 1) in LEARNSETS[dex] for slug in slugs)
for dex, slugs in starter_evolution_moves.items():
    assert all((slug, 1) in LEARNSETS[dex] for slug in slugs)  # move picker/relearn

for base, targets in data['branches'].items():
    assert EVOLUTION_BRANCHES[int(base)] == targets

noart = (R / 'noart.h').read_text(encoding='utf-8')
na = set(map(int, re.search(r'NO_ART\[.*?\] = \{\s*(.*?)\s*\};', noart, re.S)[1].split(',')))
nh = set(map(int, re.search(r'NO_HATCH\[.*?\] = \{\s*(.*?)\s*\};', noart, re.S)[1].split(',')))
assert na <= nh
assert not ({810, 813, 816, 906, 909, 912} & nh)
assert int(re.search(r'#define NO_ART_COUNT (\d+)', noart)[1]) == len(na)
assert int(re.search(r'#define NO_HATCH_COUNT (\d+)', noart)[1]) == len(nh)

trainers = (R / 'trainers.h').read_text(encoding='utf-8')
assert '#define GYM_REGIONS 7' in trainers
print(f'PASS: 216 sourced species, 689 natural moves / 14,542 rows, '
      f'{len(data["moves"])} previous sourced moves, '
      '9 dex regions, 7 gym regions, '
      f'{len(na)} art gaps and {len(nh)} no-hatch species')
