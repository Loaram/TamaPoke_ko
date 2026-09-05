#!/usr/bin/env python3
"""Assemble public release assets, mandatory guides and SHA-256 checksums."""
import argparse
import json
import shutil
import zipfile
from pathlib import Path
from prepare_release_guides import ROOT, firmware_version, sha256, copy_guide


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--guide-images', type=Path, required=True)
    args = parser.parse_args()
    version = firmware_version()
    out = ROOT / 'build' / 'release' / version
    out.mkdir(parents=True, exist_ok=True)
    guides = ROOT / 'web' / 'guides'
    pdf_name = f'TamaPoke-{version}-Play-Guide-KO.pdf'
    copy_guide(ROOT / 'output' / 'pdf' / pdf_name, guides / pdf_name)
    images = sorted(args.guide_images.glob('page-*.png'))
    if len(images) != 18:
        raise SystemExit('Expected 18 rendered play guide pages')
    with zipfile.ZipFile(guides / f'TamaPoke-{version}-Play-Guide-KO-Images.zip', 'w',
                         zipfile.ZIP_DEFLATED) as archive:
        for image in images:
            archive.write(image, image.name)

    info = json.loads((ROOT / 'web/firmware/build-info.json').read_text())
    if info['version'] != version:
        raise SystemExit('Firmware version does not match the release')
    for name, details in info['files'].items():
        if sha256(ROOT / 'web/firmware' / name) != details['sha256']:
            raise SystemExit(f'Firmware checksum mismatch: {name}')
    esp_name = f'TamaPoke-{version}-ESP32-Web-Installer.zip'
    readme = f'''TamaPoke {version} ESP32-S3 한국어판

권장 설치: https://loaram.github.io/TamaPoke_ko/
지원 보드: Waveshare ESP32-S3-Touch-AMOLED-1.75
기존 세이브 유지: Erase device를 선택하지 마세요.
manifest.json은 NVS와 FFat를 피하는 분할 펌웨어를 사용합니다.
firmware/tamapoke.bin은 빈 기기용 통합 이미지입니다.
기존 저장을 유지하는 업데이트에는 통합 이미지를 쓰지 마세요.
스프라이트 9개 지방 팩은 설치 페이지에서 microSD에 설치하세요.
플레이 가이드와 Galaxy Watch 설치 가이드는 같은 릴리스에 첨부되어 있습니다.
'''
    with zipfile.ZipFile(out / esp_name, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.write(ROOT / 'web/manifest.json', 'manifest.json')
        for name in [*info['files'], 'tamapoke.bin', 'build-info.json']:
            archive.write(ROOT / 'web/firmware' / name, f'firmware/{name}')
        archive.writestr('README-KO.txt', readme)
    names = [esp_name]
    for suffix in ['Android-Full-debug.apk', 'WearOS-GalaxyWatch4-9-debug.apk']:
        name = f'TamaPoke-{version}-{suffix}'
        shutil.copy2(ROOT / 'build/android' / name, out / name)
        names.append(name)
    copy_guide(guides / pdf_name, out / pdf_name)
    names.append(pdf_name)
    watch_name = f'TamaPoke-{version}-Galaxy-Watch4-9-Install-Guide-KO.pdf'
    copy_guide(ROOT / 'docs/guides/TamaPoke-Galaxy-Watch4-9-Install-Guide-KO.pdf', out / watch_name)
    names.append(watch_name)
    (out / 'SHA256SUMS.txt').write_text(''.join(
        f'{sha256(out / name)}  {name}\n' for name in sorted(names)), encoding='utf-8')
    for name in sorted(names):
        print(f'{name}: {(out / name).stat().st_size:,} bytes')


if __name__ == '__main__':
    main()
