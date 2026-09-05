"""Moves learned immediately on evolution in the selected latest learnset.

The source rows use level 0. They remain separate from ordinary level gates so
Pet::evolve() can offer them after the species changes, even though the old form
already consumed that level. Declined moves remain available in the move card.
"""
import json
from pathlib import Path

_DATA = json.loads(Path(__file__).with_name("full_move_data.json").read_text(encoding="utf-8"))
EVOLUTION_MOVES = {
    int(dex): tuple(slugs)
    for dex, slugs in _DATA["evolution_moves"].items()
}
del _DATA
