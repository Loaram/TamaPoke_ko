#!/usr/bin/env python3
"""Build the offline Windows GUI with the current public Wear APK; no game rebuild."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

from build_android import ROOT, firmware_version


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform-tools", type=Path, default=Path(os.environ.get("LOCALAPPDATA", "")) / "Android/Sdk/platform-tools")
    parser.add_argument("--out", type=Path, default=ROOT / "build/watch-installer")
    parser.add_argument("--game-version", default=None, help="Explicit existing APK version; defaults to current firmware version")
    parser.add_argument("--apk", type=Path, help="Explicit Wear APK (requires --variant and --label for clear identification)")
    parser.add_argument("--variant", help="Safe filename suffix for a non-release bundle, e.g. LAN-Test1")
    parser.add_argument("--label", help="Visible description of the bundled game build")
    parser.add_argument("--preview", action="store_true", help="Render a hidden Windows form for visual QA")
    parser.add_argument("--guide", type=Path, required=True, help="Reviewed installer PDF to include")
    args = parser.parse_args()
    version = args.game_version or firmware_version()
    import re
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise SystemExit("Game version must be a.b.c")
    if args.variant and not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9-]{0,39}", args.variant):
        raise SystemExit("Variant must be a short filename-safe label")
    if args.apk and not (args.variant and args.label):
        raise SystemExit("Explicit APK requires --variant and --label to avoid release/test confusion")
    if args.label and ("\n" in args.label or "\r" in args.label or len(args.label) > 70):
        raise SystemExit("Label must be a single short line")
    apk = args.apk or ROOT / "build/android" / f"TamaPoke-{version}-WearOS-GalaxyWatch4-9-debug.apk"
    caption = f"게임 {version}" + (f" · {args.label}" if args.label else "") + " · 설치 도우미 시험판"
    compiler = Path(os.environ.get("WINDIR", "C:/Windows")) / "Microsoft.NET/Framework64/v4.0.30319/csc.exe"
    for path in [apk, compiler, args.guide] + [args.platform_tools / name for name in ("adb.exe", "AdbWinApi.dll", "AdbWinUsbApi.dll", "NOTICE.txt")]:
        if not path.is_file():
            raise SystemExit(f"Required file missing: {path}")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    # A fresh directory avoids ever adding stale APKs, keys or unrelated files to the ZIP.
    suffix = f"-{args.variant}" if args.variant else ""
    bundle = out / f"TamaPoke-{version}{suffix}-Watch-Installer-Windows"
    if bundle.exists():
        raise SystemExit(f"Output already exists; choose another --out directory: {bundle}")
    bundle.mkdir()
    source = ROOT / "tools/watch_installer"
    common = [str(compiler), "/nologo", "/utf8output", "/optimize+", "/reference:System.Windows.Forms.dll", "/reference:System.Drawing.dll"]
    subprocess.run(common + ["/target:winexe", f"/out:{bundle / 'TamaPoke-Watch-Installer.exe'}", str(source / "Installer.cs")], check=True)
    tests = out / "watch-installer-tests.exe"
    subprocess.run(common + ["/target:exe", "/main:Tests", f"/out:{tests}", str(source / "Installer.cs"), str(source / "Tests.cs")], check=True)
    subprocess.run([str(tests)] + ([str(out / "installer-preview.png"), caption] if args.preview else []), check=True)
    shutil.copy2(apk, bundle / "TamaPoke-WearOS.apk")
    for name in ("adb.exe", "AdbWinApi.dll", "AdbWinUsbApi.dll", "libwinpthread-1.dll", "NOTICE.txt", "source.properties"):
        path = args.platform_tools / name
        if path.is_file():
            shutil.copy2(path, bundle / name)
    for name in ("LICENSE", "CREDITS.md"):
        shutil.copy2(ROOT / name, bundle / name)
    readme = (source / "README.ko.md").read_text(encoding="utf-8")
    if args.label:
        readme = (f"동봉 빌드: {caption}\n정식 릴리스와 구분된 별도 설치 묶음입니다.\n"
                  "기존 앱을 삭제하지 말고 설치 / 업데이트를 사용하세요.\n\n" + readme)
    (bundle / "먼저 읽어주세요.txt").write_text(readme, encoding="utf-8-sig")
    shutil.copy2(args.guide, bundle / "Watch-Installer-Guide-KO.pdf")
    (bundle / "version.txt").write_text(caption + "\n", encoding="utf-8-sig")
    files = sorted(bundle.iterdir())
    # Runtime checks only ASCII payload names; include executable and all native dependencies.
    checked = [p for p in files if p.suffix.lower() in (".exe", ".dll", ".apk", ".pdf")]
    (bundle / "SHA256SUMS.txt").write_text("".join(f"{digest(p)}  {p.name}\n" for p in checked), encoding="ascii")
    archive = out / (bundle.name + ".zip")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zip_out:
        for path in sorted(bundle.iterdir()):
            zip_out.write(path, f"{bundle.name}/{path.name}")
    with zipfile.ZipFile(archive) as zip_in:
        if zip_in.testzip() is not None:
            raise SystemExit("ZIP verification failed")
    (out / (archive.name + ".sha256")).write_text(f"{digest(archive)}  {archive.name}\n", encoding="ascii")
    print(f"Built and verified: {archive} ({archive.stat().st_size:,} bytes)")
    print("Physical watch pairing/install remains untested. No upload was performed.")


if __name__ == "__main__":
    main()
