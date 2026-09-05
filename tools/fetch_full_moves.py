#!/usr/bin/env python3
"""Build the National Dex natural-learnset snapshot used by TamaPoke.

The compact table predating this script deliberately contained only a small
selection of moves.  That made a move disappear completely whenever a species'
level-1, evolution or later level-up move was not one of those selections.

This fetcher reads PokeAPI's CSV data at a pinned commit, chooses the newest
main-series learnset up to The Indigo Disk for each default National Dex species
1..1025, and writes one committed JSON snapshot.  Level 0 is preserved
separately as an evolution move and exposed at level 1 in the relearn list.
Existing compact-table machine compatibility is retained, but new machine,
tutor and egg moves are outside this natural-learnset expansion.
"""
from __future__ import annotations

import csv
import io
import json
import sys
import urllib.request
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HERE = ROOT / "tools"
sys.path.insert(0, str(HERE))

POKEAPI_COMMIT = "d4f9a4af58ade123fbc0558f68b1c69daa97d9e4"
RAW = f"https://raw.githubusercontent.com/PokeAPI/pokeapi/{POKEAPI_COMMIT}/data/v2/csv"
MAX_DEX = 1025
MAX_VERSION_GROUP = 27  # The Indigo Disk; later rows are outside this dex snapshot.
LEVEL_UP = 1
MACHINE = 4

TYPE_NAMES = {
    1: "normal", 2: "fighting", 3: "flying", 4: "poison", 5: "ground",
    6: "rock", 7: "bug", 8: "ghost", 9: "steel", 10: "fire",
    11: "water", 12: "grass", 13: "electric", 14: "psychic", 15: "ice",
    16: "dragon", 17: "dark", 18: "fairy",
}
CLASS_NAMES = {1: "status", 2: "physical", 3: "special"}
AILMENTS = {1: "para", 2: "sleep", 3: "freeze", 4: "burn", 5: "poison", 6: "confuse"}
STATS = {2: "atk", 3: "def", 4: "spa", 5: "spd", 6: "spe"}
SELF_TARGETS = {4, 5, 7, 13, 15}


def fetch_csv(name: str) -> list[dict[str, str]]:
    req = urllib.request.Request(f"{RAW}/{name}", headers={"User-Agent": "TamaPoke-ko learnset builder"})
    with urllib.request.urlopen(req, timeout=120) as response:
        text = io.TextIOWrapper(response, encoding="utf-8", newline="")
        return list(csv.DictReader(text))


def number(value: str, default: int = 0) -> int:
    return int(value) if value not in ("", None) else default


def main() -> None:
    from dex_moves import MOVES, LEGACY_MOVE_COUNT
    from dex_data import DEX, EVOLUTION_BRANCHES

    version_groups = {
        number(row["id"]): number(row["order"])
        for row in fetch_csv("version_groups.csv")
        if number(row["id"]) <= MAX_VERSION_GROUP
    }
    move_rows = {number(row["id"]): row for row in fetch_csv("moves.csv")}
    meta_rows = {number(row["move_id"]): row for row in fetch_csv("move_meta.csv")}
    stat_rows: dict[int, list[tuple[int, int]]] = defaultdict(list)
    for row in fetch_csv("move_meta_stat_changes.csv"):
        stat_rows[number(row["move_id"])].append((number(row["stat_id"]), number(row["change"])))

    english: dict[int, str] = {}
    korean: dict[int, str] = {}
    for row in fetch_csv("move_names.csv"):
        move_id = number(row["move_id"])
        lang = number(row["local_language_id"])
        if lang == 9:
            english[move_id] = row["name"]
        elif lang == 3:
            korean[move_id] = row["name"]

    pokemon_rows = []
    for row in fetch_csv("pokemon_moves.csv"):
        dex = number(row["pokemon_id"])
        vg = number(row["version_group_id"])
        if 1 <= dex <= MAX_DEX and vg in version_groups:
            pokemon_rows.append({
                "dex": dex,
                "vg": vg,
                "move": number(row["move_id"]),
                "method": number(row["pokemon_move_method_id"]),
                "level": number(row["level"]),
            })

    latest: dict[int, int] = {}
    for row in pokemon_rows:
        if row["method"] != LEVEL_UP:
            continue
        dex, vg = row["dex"], row["vg"]
        if dex not in latest or version_groups[vg] > version_groups[latest[dex]]:
            latest[dex] = vg
    missing_species = sorted(set(range(1, MAX_DEX + 1)) - set(latest))
    if missing_species:
        raise SystemExit(f"No main-series level-up learnset for species: {missing_species}")

    natural: dict[int, dict[int, int]] = {dex: {} for dex in range(1, MAX_DEX + 1)}
    evolution: dict[int, set[int]] = defaultdict(set)
    for row in pokemon_rows:
        dex = row["dex"]
        if row["method"] != LEVEL_UP or row["vg"] != latest[dex]:
            continue
        move_id, level = row["move"], row["level"]
        # A duplicate within one version group is harmless; keep the earliest gate.
        shown_level = 1 if level == 0 else max(1, min(level, 100))
        natural[dex][move_id] = min(natural[dex].get(move_id, 255), shown_level)
        if level == 0:
            evolution[dex].add(move_id)

    legacy_slugs = {slug for _name, slug, *_ in MOVES[:LEGACY_MOVE_COUNT] if slug}
    move_id_by_slug = {row["identifier"]: move_id for move_id, row in move_rows.items()}
    legacy_ids = {move_id_by_slug[slug] for slug in legacy_slugs if slug in move_id_by_slug}
    machines: dict[int, set[int]] = defaultdict(set)
    for row in pokemon_rows:
        if row["method"] == MACHINE and row["move"] in legacy_ids:
            machines[row["dex"]].add(row["move"])

    natural_ids = {move_id for rows in natural.values() for move_id in rows}
    known_slugs = {slug for _name, slug, *_ in MOVES[:LEGACY_MOVE_COUNT] if slug}
    new_ids = sorted(move_id for move_id in natural_ids if move_rows[move_id]["identifier"] not in known_slugs)

    def move_record(move_id: int) -> dict[str, object]:
        src = move_rows[move_id]
        meta = meta_rows.get(move_id, {})
        slug = src["identifier"]
        category = CLASS_NAMES[number(src["damage_class_id"])]
        source_power = number(src["power"])
        power = 0 if category == "status" else (source_power or 60)
        accuracy = number(src["accuracy"])
        target = "self" if number(src["target_id"]) in SELF_TARGETS else "foe"
        effect, param, stat, stages = "none", 0, "none", 0

        healing = number(meta.get("healing", ""))
        drain = number(meta.get("drain", ""))
        changes = [(STATS[s], c) for s, c in stat_rows.get(move_id, []) if s in STATS and c]
        same_change = bool(changes) and len({c for _s, c in changes}) == 1
        if category == "status" and healing > 0:
            effect, param = "heal", min(100, healing)
        elif category == "status" and same_change:
            effect = "stage"
            stat = "+".join(s for s, _c in changes)
            stages = max(-2, min(2, changes[0][1]))
        elif category != "status" and number(meta.get("min_hits", "")) >= 2:
            effect = "multi"
        elif category != "status" and drain > 0:
            effect, param = "drain", min(100, drain)
        elif category != "status" and drain < 0:
            effect, param = "recoil", max(2, round(100 / min(100, abs(drain))))
        elif category != "status" and number(src["priority"]) != 0:
            effect, param = "priority", max(-7, min(7, number(src["priority"])))
        elif category != "status" and not src["accuracy"]:
            effect = "never_miss"

        ailment = AILMENTS.get(number(meta.get("meta_ailment_id", "")), "none")
        ailment_chance = number(meta.get("ailment_chance", ""))
        if ailment != "none" and category == "status" and ailment_chance == 0:
            ailment_chance = 100

        en = english.get(move_id, slug.replace("-", " ").title())
        return {
            "source_id": move_id,
            "slug": slug,
            "display": en.upper(),
            "ko": korean.get(move_id, en),
            "type": TYPE_NAMES[number(src["type_id"])],
            "category": category,
            "power": power,
            "source_power": source_power,
            "accuracy": accuracy,
            "effect": effect,
            "param": param,
            "stat": stat,
            "stages": stages,
            "target": target,
            "ailment": ailment,
            "ailment_chance": max(0, min(100, ailment_chance)),
        }

    records = [move_record(move_id) for move_id in new_ids]
    slug_for = {move_id: row["identifier"] for move_id, row in move_rows.items()}
    learnsets: dict[str, list[list[object]]] = {}
    max_rows = 0
    max_dex = 0
    for dex in range(1, MAX_DEX + 1):
        rows = {slug_for[mid]: level for mid, level in natural[dex].items()}
        for mid in machines[dex]:
            rows.setdefault(slug_for[mid], 0)
        ordered = sorted(rows.items(), key=lambda item: (item[1] == 0, item[1], item[0]))
        learnsets[str(dex)] = [[slug, level] for slug, level in ordered]
        if len(ordered) > max_rows:
            max_rows, max_dex = len(ordered), dex

    evolution_targets = {row[4] for row in DEX if row[4]}
    evolution_targets.update(target for targets in EVOLUTION_BRANCHES.values() for target in targets)
    evolution_out = {
        str(dex): [slug_for[mid] for mid in sorted(move_ids)]
        for dex, move_ids in sorted(evolution.items()) if move_ids and dex in evolution_targets
    }
    snapshot = {
        "provenance": {
            "source": "PokeAPI CSV database",
            "commit": POKEAPI_COMMIT,
            "base_url": RAW,
            "national_dex": [1, MAX_DEX],
            "latest_version_group": "the-indigo-disk",
            "max_version_group_id": MAX_VERSION_GROUP,
            "methods": ["level-up", "machine compatibility for the pre-expansion table"],
            "note": "Level 0 source rows are evolution moves; relearn rows expose them at level 1.",
        },
        "counts": {
            "species": MAX_DEX,
            "natural_rows": sum(len(rows) for rows in natural.values()),
            "natural_unique_moves": len(natural_ids),
            "new_moves": len(records),
            "evolution_species": len(evolution_out),
            "evolution_rows": sum(len(rows) for rows in evolution_out.values()),
            "max_learnset_rows": max_rows,
            "max_learnset_dex": max_dex,
        },
        "moves": records,
        "learnsets": learnsets,
        "evolution_moves": evolution_out,
    }
    target = HERE / "full_move_data.json"
    target.write_text(json.dumps(snapshot, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    names_path = ROOT / "localization" / "names-ko.json"
    names = json.loads(names_path.read_text(encoding="utf-8"))
    for record in records:
        names[record["display"]] = record["ko"]
    names_path.write_text(json.dumps(names, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print(f"source {POKEAPI_COMMIT}")
    print(f"{MAX_DEX} species, {snapshot['counts']['natural_rows']} natural rows")
    print(f"{len(natural_ids)} natural moves; {len(records)} appended after {LEGACY_MOVE_COUNT}")
    print(f"{snapshot['counts']['evolution_rows']} evolution rows for {len(evolution_out)} species")
    print(f"largest merged learnset: {max_rows} rows (dex {max_dex})")
    print(f"wrote {target}")


if __name__ == "__main__":
    main()
