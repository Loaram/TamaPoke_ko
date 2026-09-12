# 3.8.1 verification

- ESP32 Arduino 3.3.8, Android 3050, Wear 3051 and ordinary emulator builds passed.
- Focused current-source suites: save, savetransfer, link, linkudp, lan passed.
  LAN includes sender allocation failure, receiver without staging allocation,
  apply allocation failure, byte-identical saved state after allocation failure,
  invalid received data/rollback, and 20 repeated sender preparations.
- SD store 10 checks and production sprite receiver 30 checks passed. Cases
  include partial USB read, short write, open failure, read-back size/CRC errors,
  invalid paths/sizes, simulated file 47/50 failures and 344 successive files.
- Browser installer tests cover 11 detailed errors, exact file/byte/phase,
  final verification failure, stop-on-error, and bounded logs.
- Watch installer 30 checks passed; bundled APK and guide hashes are verified.
- Public bundle validator verifies 9 exact assets, APK signing identity/ABI/art
  packs, ESP image, safe partition layout, PDF contents and checksums.
- Both new PDFs rendered and visually reviewed: play guide 29 pages and Windows
  installer guide 7 pages. Unchanged Galaxy Watch manual is included separately.
- Final-commit GitHub Verify Korean edition must succeed before publication:
  65 runtime suites, generated data, web, Java LAN, SD and ESP compilation.

## Physical scope

User confirmed ESP test firmware successfully connected to the phone and
completed save transfer/application. Internal free boot memory rose from about
85 KB to 118 KB. Both the memory and MAC fixes were applied together, so the
experiment does not isolate a single root cause. Release disables verbose
diagnostics while retaining the tested fixes and unchanged save/protocol rules.

The user kept the working test firmware on the gift device; it was not reflashed
merely to change the version label. Physical testing of every battle pairing,
all watch models and a complete USB sprite reinstall is not claimed. SD errors
can leave an incomplete sprite file: reconnect after checking media/cable and
reinstall the affected pack; do not format the card or delete save files.
