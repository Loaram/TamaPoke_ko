#!/bin/bash
# Build the Korean edition using the cross-platform, NVS-safe packager.
set -e
cd "$(dirname "$0")/.."
python3 tools/check_korean.py
python3 tools/build_firmware.py --publish "$@"
python3 tools/check_web.py
# Existing regional packs are unchanged. Regenerate only after sprite changes:
# python3 tools/pack_bundle.py
