# Korean fork context

Read `HANDOFF.ko.md` and `README.ko.md` for the current project state before continuing work.

- This repository is Loaram's Korean edition of **DylanPDao/TamaPoke**. It now covers National Dex 1–1025 across nine Pokédex regions; gyms remain the original seven regions. It is not the ShadowEnemyx Gen 3 fork.
- `HANDOVER.md` is an older upstream history, including another maintainer's board/save state. It is not evidence about the user's device.
- Korean UI uses `korean_text.h` and the generated font/name tables. Keep canonical IDs, saved nicknames, language indices, and radio record formats compatible.
- Build/emulator/web checks have passed for ko.1.1.4. Hardware, Android device-time behavior, the 24-hour farewell boundary and Android-to-hardware LAN tests remain unverified until actual results are recorded. See the hardware test issue template and `docs/HARDWARE_TEST.ko.md`.
- For every numbered release, also build two private emulator variants: full normal Pokedex with `-dex` and full shiny Pokedex with `-shiny` (for example `ko.1.1.1-dex` and `ko.1.1.1-shiny`). Never publish or attach either executable to GitHub releases or Pages. Build them with `tools/build_emulator.py --full-dex` and `tools/build_emulator.py --full-shiny`; they use `tamapoke-dex.nvs` and `tamapoke-shiny.nvs` instead of the normal emulator save.
- Use the commands and pinned board/library settings in `README.ko.md` and `HANDOFF.ko.md`. Local runtime paths and credentials from the previous PC are not dependencies of this repository.
