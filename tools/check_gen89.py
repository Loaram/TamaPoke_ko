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

data = json.loads((R / 'tools/gen89_data.json').read_text(encoding='utf-8'))
names = json.loads((R / 'localization/names-ko.json').read_text(encoding='utf-8'))
species = data['species']

assert [s['dex'] for s in species] == list(range(810, 1026))
assert len(DEX) == len(BASE_STATS) == len(TYPES) == len(LEARNSETS) == 1025
assert REGIONS[-2:] == [('GALAR', 810, 905, [810, 813, 816]),
                        ('PALDEA', 906, 1025, [906, 909, 912])]

move_by_slug = {m[1]: m for m in MOVES if m[1]}
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
print('PASS: 216 sourced species, 45 sourced moves, 9 dex regions, 7 gym regions, '
      f'{len(na)} art gaps and {len(nh)} no-hatch species')
