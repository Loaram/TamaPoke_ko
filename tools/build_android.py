#!/usr/bin/env python3
"""Build Android and universal Galaxy Watch Wear OS debug APKs without Gradle."""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ANDROID = ROOT / "tools" / "android"
ALL_ABIS = {
    "armeabi-v7a": "armv7a-linux-androideabi26",
    "arm64-v8a": "aarch64-linux-android26",
    "x86_64": "x86_64-linux-android26",
}


def run(command: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=cwd, env=env, check=True)


def bat(path: Path, *args: str) -> list[str]:
    if path.suffix.lower() in {".bat", ".cmd"}:
        return [os.environ.get("COMSPEC", "cmd.exe"), "/d", "/s", "/c", str(path), *args]
    return [str(path), *args]


def newest_dir(parent: Path) -> Path:
    found = [p for p in parent.iterdir() if p.is_dir()]
    if not found:
        raise SystemExit(f"No installed version under {parent}")
    return max(found, key=lambda p: tuple(int(x) for x in re.findall(r"\d+", p.name)))


def add_stored(apk: Path, source: Path, archive_name: str) -> None:
    info = zipfile.ZipInfo.from_file(source, archive_name)
    info.compress_type = zipfile.ZIP_STORED
    with zipfile.ZipFile(apk, "a", allowZip64=True) as zf:
        with source.open("rb") as src, zf.open(info, "w") as dst:
            shutil.copyfileobj(src, dst, 1024 * 1024)


def firmware_version() -> str:
    text = (ROOT / "TamaPoke.ino").read_text(encoding="utf-8")
    match = re.search(r'^#define\s+FW_VERSION\s+"([^"]+)"', text, re.MULTILINE)
    if not match:
        raise SystemExit("FW_VERSION was not found in TamaPoke.ino")
    return match.group(1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk", type=Path, default=Path(os.environ.get(
        "ANDROID_SDK_ROOT", Path(os.environ.get("LOCALAPPDATA", "")) / "Android" / "Sdk")))
    parser.add_argument("--jdk", type=Path, default=Path(os.environ.get(
        "JAVA_HOME", r"C:\Program Files\Android\Android Studio\jbr")))
    parser.add_argument("--wear", action="store_true",
                        help="build a standalone ARM32/ARM64 Wear OS APK for Galaxy Watch4-9")
    parser.add_argument("--version-code", type=int,
                        help="defaults to 2000 for Android or 2001 for Wear OS")
    parser.add_argument("--android-revision", type=int,
                        help="defaults to 1")
    parser.add_argument("--output", type=Path,
                        help="defaults to build/android/TamaPoke-<FW_VERSION>-Android-Full-debug.apk")
    args = parser.parse_args()

    sdk = args.sdk.resolve()
    jdk = args.jdk.resolve()
    build_tools = newest_dir(sdk / "build-tools")
    ndk = newest_dir(sdk / "ndk")
    platforms = [p for p in (sdk / "platforms").iterdir() if p.is_dir()]
    platform = max(platforms, key=lambda p: tuple(int(x) for x in re.findall(r"\d+", p.name)))
    android_jar = platform / "android.jar"
    llvm = ndk / "toolchains" / "llvm" / "prebuilt" / "windows-x86_64" / "bin"
    clang = llvm / "clang.exe"
    clangxx = llvm / "clang++.exe"
    llvm_strip = llvm / "llvm-strip.exe"
    glue_dir = ndk / "sources" / "android" / "native_app_glue"
    required = [android_jar, clang, clangxx, llvm_strip, glue_dir / "android_native_app_glue.c",
                jdk / "bin" / "javac.exe", build_tools / "aapt2.exe",
                build_tools / "aapt.exe",
                build_tools / "d8.bat", build_tools / "zipalign.exe",
                build_tools / "apksigner.bat"]
    missing = [str(p) for p in required if not p.exists()]
    if missing:
        raise SystemExit("Missing Android build tools:\n" + "\n".join(missing))

    version = firmware_version()
    flavor = "wear" if args.wear else "android"
    version_code = args.version_code if args.version_code is not None else (2001 if args.wear else 2000)
    revision = args.android_revision if args.android_revision is not None else 1
    version_name = f"{version}-{flavor}.{revision}"
    default_name = (f"TamaPoke-{version}-WearOS-GalaxyWatch4-9-debug.apk" if args.wear
                    else f"TamaPoke-{version}-Android-Full-debug.apk")
    apk_output = args.output or ROOT / "build" / "android" / default_name
    manifest_source = ANDROID / ("WearManifest.xml" if args.wear else "AndroidManifest.xml")
    min_sdk = 30 if args.wear else 26
    abis = ({abi: ALL_ABIS[abi] for abi in ("armeabi-v7a", "arm64-v8a")}
            if args.wear else
            {abi: ALL_ABIS[abi] for abi in ("arm64-v8a", "x86_64")})
    # aapt2 on Windows still opens resource paths through a narrow-character
    # code path. Keep intermediates under an ASCII-only directory so this also
    # builds from Korean workspace paths.
    user_profile = Path(os.environ.get("USERPROFILE", ""))
    local_app_data = user_profile / "AppData" / "Local"
    work = local_app_data / "TamaPokeAndroidBuild"
    if (not user_profile.is_absolute() or work.parent != local_app_data or
            work.name != "TamaPokeAndroidBuild"):
        raise SystemExit(f"Refusing unsafe Android work directory: {work}")
    if work.exists():
        shutil.rmtree(work)
    (work / "native").mkdir(parents=True)
    (work / "classes").mkdir()
    (work / "dex").mkdir()
    (work / "generated-java").mkdir()
    apk_output.parent.mkdir(parents=True, exist_ok=True)
    staged_android = work / "android-project"
    shutil.copytree(ANDROID / "res", staged_android / "res")
    shutil.copy2(manifest_source, staged_android / "AndroidManifest.xml")
    shutil.copytree(ANDROID / "java", staged_android / "java")

    run([sys.executable, str(ROOT / "tools" / "emu" / "genproto.py"), str(ROOT / "TamaPoke.ino")],
        cwd=ROOT / "tools" / "emu")
    sketch = work / "native" / "sketch.cpp"
    sketch.write_text('#include "proto.h"\n' + (ROOT / "TamaPoke.ino").read_text(encoding="utf-8"),
                      encoding="utf-8")

    cpp_sources = [
        sketch,
        ROOT / "gbsynth.cpp", ROOT / "pet.cpp", ROOT / "i18n.cpp",
        ROOT / "party.cpp", ROOT / "battle.cpp", ROOT / "wild.cpp",
        ROOT / "link.cpp", ROOT / "save.cpp",
        ROOT / "tools" / "emu" / "font.cpp", ROOT / "tools" / "emu" / "clock.cpp",
        ANDROID / "host_android.cpp", ANDROID / "android_audio.cpp",
        ANDROID / "link_udp.cpp", ANDROID / "android_main.cpp",
    ]
    native_outputs: dict[str, Path] = {}
    common = [
        "-std=c++17", "-O2", "-fPIC", "-ffunction-sections", "-fdata-sections", "-w",
        "-DANDROID", "-D__ANDROID__", "-I" + str(ROOT / "tools" / "emu"),
        "-I" + str(ROOT), "-I" + str(ANDROID), "-I" + str(glue_dir),
    ]
    for abi, target in abis.items():
        abi_dir = work / "native" / abi
        abi_dir.mkdir()
        glue_obj = abi_dir / "android_native_app_glue.o"
        run([str(clang), f"--target={target}", "-O2", "-fPIC", "-I" + str(glue_dir),
             "-c", str(glue_dir / "android_native_app_glue.c"), "-o", str(glue_obj)])
        lib_output = abi_dir / "libtamapoke.so"
        run([str(clangxx), f"--target={target}", *common, "-shared",
             "-Wl,--gc-sections", "-Wl,--no-undefined", "-Wl,--build-id=sha1",
             "-Wl,-soname,libtamapoke.so",
             *map(str, cpp_sources), str(glue_obj), "-static-libstdc++",
             "-landroid", "-laaudio", "-llog", "-latomic", "-lm", "-o", str(lib_output)])
        run([str(llvm_strip), "--strip-unneeded", str(lib_output)])
        native_outputs[abi] = lib_output

    compiled_res = work / "compiled-res.zip"
    base_apk = work / "base.apk"
    run([str(build_tools / "aapt2.exe"), "compile", "--dir", str(staged_android / "res"),
         "-o", str(compiled_res)])
    run([str(build_tools / "aapt2.exe"), "link", "-o", str(base_apk),
         "-I", str(android_jar), "--manifest", str(staged_android / "AndroidManifest.xml"),
         "--java", str(work / "generated-java"), "--min-sdk-version", str(min_sdk),
         "--target-sdk-version", "37", "--version-code", str(version_code),
         "--version-name", version_name, str(compiled_res)])

    java_sources = [str(staged_android / "java" / "com" / "loaram" / "tamapoke" / "TamaPokeActivity.java")]
    java_sources.extend(str(p) for p in (work / "generated-java").rglob("*.java"))
    run([str(jdk / "bin" / "javac.exe"), "-source", "8", "-target", "8",
         "-bootclasspath", str(android_jar), "-encoding", "UTF-8",
         "-d", str(work / "classes"), *java_sources])
    classes_jar = work / "classes.jar"
    run([str(jdk / "bin" / "jar.exe"), "cf", str(classes_jar), "-C", str(work / "classes"), "."])
    env = os.environ.copy()
    env["JAVA_HOME"] = str(jdk)
    run(bat(build_tools / "d8.bat", "--min-api", str(min_sdk), "--output", str(work / "dex"),
            str(classes_jar)), env=env)

    unaligned = work / "unsigned-unaligned.apk"
    aligned = work / "unsigned-aligned.apk"
    shutil.copy2(base_apk, unaligned)
    add_stored(unaligned, work / "dex" / "classes.dex", "classes.dex")
    packs = sorted((ROOT / "web").glob("sprites-*.pak"))
    if len(packs) != 9:
        raise SystemExit(f"Expected 9 sprite packs, found {len(packs)}")
    for pack in packs:
        add_stored(unaligned, pack, f"assets/{pack.name}")
    for abi, lib in native_outputs.items():
        add_stored(unaligned, lib, f"lib/{abi}/libtamapoke.so")
    run([str(build_tools / "zipalign.exe"), "-P", "16", "-f", "4",
         str(unaligned), str(aligned)])

    keystore = ROOT / "build" / "android" / "debug.keystore"
    if not keystore.exists():
        run([str(jdk / "bin" / "keytool.exe"), "-genkeypair", "-keystore", str(keystore),
             "-storepass", "android", "-alias", "androiddebugkey", "-keypass", "android",
             "-dname", "CN=Android Debug,O=Android,C=US", "-keyalg", "RSA", "-keysize", "2048",
             "-validity", "10000"])
    signed = work / "signed.apk"
    run(bat(build_tools / "apksigner.bat", "sign", "--ks", str(keystore),
            "--ks-key-alias", "androiddebugkey", "--ks-pass", "pass:android",
            "--key-pass", "pass:android", "--out", str(signed), str(aligned)), env=env)
    run(bat(build_tools / "apksigner.bat", "verify", "--verbose", "--print-certs",
            str(signed)), env=env)
    run([str(build_tools / "zipalign.exe"), "-c", "-P", "16", "4", str(signed)])
    run([str(build_tools / "aapt.exe"), "dump", "badging", str(signed)])
    if apk_output.exists():
        apk_output.unlink()
    shutil.copy2(signed, apk_output)
    print(f"Built {apk_output} ({apk_output.stat().st_size:,} bytes)")
    print(f"Version {version_name}, versionCode {version_code}, ABIs {', '.join(abis)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
