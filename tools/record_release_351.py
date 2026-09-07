"""Audit 3.5.1 release files, signatures, contents and focused regression logs."""
import hashlib
import json
import re
import shutil
import zipfile
from pathlib import Path
from pypdf import PdfReader

ROOT = Path(__file__).resolve().parents[1]
VERSION = '3.5.1'
assets = ROOT / 'build/release' / VERSION
qa = ROOT / 'docs/qa' / VERSION
qa.mkdir(parents=True, exist_ok=True)

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

markers = {'runtime': 'PASS: korean,i18n,label,lan,trade_ui,savetransfer,upgrade',
           'android': 'versionCode 3030', 'wear': 'versionCode 3031',
           'esp': 'Published verified Korean components', 'emu': 'tamapoke-emu.exe',
           'dex': 'TamaPoke-3.5.1-dex.exe', 'shiny': 'TamaPoke-3.5.1-shiny.exe',
           'watch-installer': '30 checks passed.', 'sd-store': 'PASS factory reset'}
for name, marker in markers.items():
    log = ROOT / f'build/release-351-{name}.log'
    content = log.read_text(encoding='utf8', errors='replace')
    assert marker in content and not re.search(r'^FAIL|Traceback|error:', content, re.M), name
    if name in ('android', 'wear'):
        assert '17aaf3ebfaa9b51599e2b0d0970f99651bd8be6700da981b7f240677535b7f47' in content
    (qa / (name + '.log')).write_text('\n'.join(line.rstrip() for line in content.splitlines()) + '\n', encoding='utf8')

expected = {f'TamaPoke-{VERSION}-{s}' for s in (
    'Android-Full-debug.apk', 'WearOS-GalaxyWatch4-9-debug.apk', 'ESP32-Web-Installer.zip',
    'Play-Guide-KO.pdf', 'Galaxy-Watch4-9-Install-Guide-KO.pdf',
    'Watch-Installer-Windows.zip', 'Watch-Installer-Guide-KO.pdf')}
expected |= {'SHA256SUMS.txt', 'WATCH-INSTALLER-SHA256SUMS.txt'}
assert {p.name for p in assets.iterdir()} == expected
sums = dict(reversed(line.split('  ', 1)) for line in (assets / 'SHA256SUMS.txt').read_text().splitlines())
assert set(sums) == expected - {'SHA256SUMS.txt'}
assert all(sha(assets / n) == h for n, h in sums.items())

for suffix, abis in [('Android-Full-debug.apk', {'arm64-v8a', 'x86_64'}),
                     ('WearOS-GalaxyWatch4-9-debug.apk', {'arm64-v8a', 'armeabi-v7a'})]:
    with zipfile.ZipFile(assets / f'TamaPoke-{VERSION}-{suffix}') as z:
        assert z.testzip() is None
        assert {n.split('/')[1] for n in z.namelist() if n.startswith('lib/') and n.endswith('.so')} == abis
        for abi in abis:
            lib = z.read(f'lib/{abi}/libtamapoke.so')
            assert VERSION.encode() in lib and '통신 메뉴'.encode() in lib
        for pack in [*(ROOT / 'web').glob('sprites-*.pak'), ROOT / 'web/forms.pak']:
            assert hashlib.sha256(z.read('assets/' + pack.name)).hexdigest() == sha(pack)

with zipfile.ZipFile(assets / f'TamaPoke-{VERSION}-ESP32-Web-Installer.zip') as z:
    assert z.testzip() is None
    assert z.read('firmware/app.bin') == (ROOT / 'web/firmware/app.bin').read_bytes()
    assert '통신 메뉴'.encode() in z.read('firmware/app.bin')

watch_source = ROOT / 'docs/guides/TamaPoke-Galaxy-Watch4-9-Install-Guide-KO.pdf'
assert sha(assets / f'TamaPoke-{VERSION}-Galaxy-Watch4-9-Install-Guide-KO.pdf') == sha(watch_source)
play = PdfReader(assets / f'TamaPoke-{VERSION}-Play-Guide-KO.pdf')
guide = PdfReader(assets / f'TamaPoke-{VERSION}-Watch-Installer-Guide-KO.pdf')
assert len(play.pages) == 27 and len(guide.pages) == 7
assert '통신 메뉴' in play.pages[13].extract_text()
assert '통신 메뉴' in play.pages[25].extract_text()
assert all(p.extract_text().strip() for p in [*play.pages, *guide.pages])
report = dict(version=VERSION, local_runtime_suites=7, sd_failure_checks=10, installer_checks=30,
              save_version=7, battle_protocol=5, trade_protocol=1, device_tested=False,
              android_version_code=3030, wear_version_code=3031,
              artifacts={n: {'bytes': (assets/n).stat().st_size, 'sha256': sha(assets/n)} for n in sorted(expected)})
(qa / 'build-results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print('PASS: 9 public assets, original signing key, APK ABI/sprites/menu label, ESP, guides and hashes')
