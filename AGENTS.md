# Korean fork context

Read `HANDOFF.ko.md` and `README.ko.md` for the current project state before continuing work.

- This repository is Loaram's Korean edition of **DylanPDao/TamaPoke**. Preserve its 809 species and seven regions. It is not the ShadowEnemyx Gen 3 fork.
- `HANDOVER.md` is an older upstream history, including another maintainer's board/save state. It is not evidence about the user's device.
- Korean UI uses `korean_text.h` and the generated font/name tables. Keep canonical IDs, saved nicknames, language indices, and radio record formats compatible.
- Build/emulator/web checks have passed for ko.1.0.5. Hardware tests remain unverified until actual results are recorded. See the hardware test issue template and `docs/HARDWARE_TEST.ko.md`.
- For every numbered release, also build a private full-Pokedex emulator with the same version plus `-dex` (for example `ko.1.0.5-dex`). Never publish or attach the `-dex` executable to GitHub releases or Pages. Build it with `tools/build_emulator.py --full-dex`; it uses `tamapoke-dex.nvs` instead of the normal emulator save.
- Use the commands and pinned board/library settings in `README.ko.md` and `HANDOFF.ko.md`. Local runtime paths and credentials from the previous PC are not dependencies of this repository.
