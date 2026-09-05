"""Natural and legacy-compatible machine learnsets for National Dex 1..1025.

Generated data lives in full_move_data.json so the 14,000+ sourced rows are not
duplicated in two committed formats. Run tools/fetch_full_moves.py to refresh
the pinned PokeAPI snapshot, then tools/gen_moves.py to rebuild moves.h.
"""
import json
from pathlib import Path

_DATA = json.loads(Path(__file__).with_name("full_move_data.json").read_text(encoding="utf-8"))
LEARNSETS = {
    int(dex): [(slug, int(level)) for slug, level in rows]
    for dex, rows in _DATA["learnsets"].items()
}
del _DATA
