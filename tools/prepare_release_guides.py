#!/usr/bin/env python3
"""Prepare the two PDF guides required by every public release."""

from __future__ import annotations

import argparse
import hashlib
import re
import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VERSION_RE = re.compile(r'^#define\s+FW_VERSION\s+"([^"]+)"', re.MULTILINE)
CHECKSUM_RE = re.compile(r"^[0-9a-fA-F]{64}  (.+)$")


def firmware_version() -> str:
    text = (ROOT / "TamaPoke.ino").read_text(encoding="utf-8")
    match = VERSION_RE.search(text)
    if not match:
        raise SystemExit("FW_VERSION was not found in TamaPoke.ino")
    return match.group(1)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_pdf(path: Path) -> None:
    if not path.is_file():
        raise SystemExit(f"Required guide is missing: {path.relative_to(ROOT)}")
    with path.open("rb") as handle:
        header = handle.read(5)
    if header != b"%PDF-":
        raise SystemExit(f"Required guide is not a valid PDF: {path.relative_to(ROOT)}")


def copy_guide(source: Path, destination: Path) -> None:
    validate_pdf(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.resolve() != destination.resolve():
        shutil.copy2(source, destination)


def update_checksums(path: Path, assets: list[Path]) -> None:
    target_names = {asset.name for asset in assets}
    kept: list[str] = []
    if path.exists():
        for line in path.read_text(encoding="utf-8").splitlines():
            match = CHECKSUM_RE.match(line)
            if not match or match.group(1) not in target_names:
                kept.append(line)
    kept.extend(f"{sha256(asset)}  {asset.name}" for asset in assets)
    path.write_text("\n".join(kept).rstrip() + "\n", encoding="utf-8", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Copy the mandatory Watch and play guides into a release directory."
    )
    parser.add_argument("--version", default=firmware_version())
    parser.add_argument(
        "--out",
        type=Path,
        help="release asset directory (default: build/release/<version>)",
    )
    args = parser.parse_args()

    version = args.version
    out = args.out or ROOT / "build" / "release" / version
    if not out.is_absolute():
        out = ROOT / out

    play_source = ROOT / "web" / "guides" / f"TamaPoke-{version}-Play-Guide-KO.pdf"
    watch_source = ROOT / "docs" / "guides" / "TamaPoke-Galaxy-Watch4-9-Install-Guide-KO.pdf"
    play_asset = out / play_source.name
    watch_asset = out / f"TamaPoke-{version}-Galaxy-Watch4-9-Install-Guide-KO.pdf"

    copy_guide(play_source, play_asset)
    copy_guide(watch_source, watch_asset)
    update_checksums(out / "SHA256SUMS.txt", [play_asset, watch_asset])

    print(f"Prepared required release guides for {version}:")
    print(f"- {play_asset.relative_to(ROOT)}")
    print(f"- {watch_asset.relative_to(ROOT)}")
    print(f"- {(out / 'SHA256SUMS.txt').relative_to(ROOT)}")


if __name__ == "__main__":
    main()
