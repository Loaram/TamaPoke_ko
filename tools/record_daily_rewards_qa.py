"""Archive private daily-reward build evidence; never publish or touch live saves."""
from pathlib import Path
import argparse, hashlib, json, re, shutil, zipfile
from PIL import Image
R = Path(__file__).resolve().parents[1]
B = R / 'build/daily-rewards'
Q = R / 'docs/qa/daily-rewards'
p = argparse.ArgumentParser()
p.add_argument('--screens-only', action='store_true')
a = p.parse_args()
Q.mkdir(parents=True, exist_ok=True)
for name in ('player', 'menu', 'confirm', 'exhausted'):
    Image.open(R / f'build/runtime-tests/daily-rewards-{name}.ppm').save(Q / f'{name}.png')
for name in ('first', 'last', 'picker'):
    Image.open(R / f'build/runtime-tests/box-pages-{name}.ppm').save(Q / f'box-{name}.png')
if a.screens_only:
    raise SystemExit(0)

def info(path):
    with path.open('rb') as f:
        return dict(bytes=path.stat().st_size, sha256=hashlib.file_digest(f, 'sha256').hexdigest())

for log, marker in {
    'runtime-tests.log': '42 runtime suites',
    'emulator-build.log': '3.0.0-beta.6-forms.exe',
    'android-build.log': 'versionCode 3010',
    'wear-build.log': 'versionCode 3011',
    'esp-build.log': 'Application bytes:',
}.items():
    content = (B / log).read_text(encoding='utf8', errors='replace')
    assert marker in content and not re.search(r'^FAIL', content, re.M) and 'Traceback' not in content, log
    shutil.copy2(B / log, Q / log)
artifacts = [B / 'emulator/TamaPoke-3.0.0-beta.6-forms.exe',
             B / 'TamaPoke-3.0.0-beta.6-Android.apk',
             B / 'TamaPoke-3.0.0-beta.6-WearOS.apk',
             B / 'firmware/TamaPoke.ino.bin']
for path in artifacts:
    assert path.is_file(), path
    if path.suffix == '.apk':
        with zipfile.ZipFile(path) as z:
            assert z.testzip() is None, path
report = dict(version='3.0.0-beta.6', published=False, device_tested=False,
              runtime_suites=42, daily_cap=3, reward_cap_day=10,
              box_pages=50, box_page_buttons=True, box_page_picker=True,
              max_shiny_percent=10, max_legendary_tier_percent=14,
              save_version=6, save_max_bytes=32768,
              artifacts={f.relative_to(R).as_posix(): info(f) for f in artifacts},
              source_files={f.name: info(f) for f in R.iterdir() if f.suffix in ('.h','.cpp','.ino')})
(Q / 'build-results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
print('PASS: 42 suites, four private builds and daily-reward / box navigation screenshots archived')
