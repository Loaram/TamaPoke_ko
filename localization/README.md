# Korean localization

Based on DylanPDao/TamaPoke commit 6cd0466, firmware 3.11. No ShadowEnemyx source is used.

- `ko.json`: 173 UI translations, in StrId order. The Korean row is in i18n.cpp.
- `names-ko.json`: all 809 current species, move/type/region/trainer/place display names.
- Species and move names came from PokeAPI's `pokemon_species_names.csv` and `move_names.csv`, Korean language ID 3, retrieved 2026-09-03. Internal dex/move IDs and canonical English data remain unchanged. Trainer/place labels are maintained manually.
- `tools/fonts/Galmuri11.ttf`: Galmuri 2.40.3, SIL OFL 1.1; see the accompanying OFL.md.
- Nicknames retain the original ASCII keyboard and storage limits. Unicode species names are display data, never copied into save or radio records.

After changing names, run `python tools/gen_korean_names.py`. After changing text, run `python tools/gen_korean_font.py`, then `python tools/check_korean.py`.
The shared `korean_text.h` decoder, renderer and pixel widths run in both firmware and emulator. It uses no extra heap allocation, no SD font file, and keeps the original ASCII font.
