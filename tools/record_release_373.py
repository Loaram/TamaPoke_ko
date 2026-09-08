"""Verify the exact public 3.7.3 bundle and record local evidence."""
import hashlib,json,re,zipfile,sys
from pathlib import Path
from pypdf import PdfReader
from capture_guide_data import probability, capture_rows, EXAMPLES
R=Path(__file__).resolve().parents[1];V='3.7.3';A=R/'build/release'/V;Q=R/'docs/qa'/V
Q.mkdir(parents=True,exist_ok=True)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
markers={'android':'versionCode 3046','wear':'versionCode 3047','esp':'Published verified Korean components',
         'emu':'tamapoke-emu.exe','watch-installer':'30 checks passed.'}
for name,marker in markers.items():
    s=(R/f'build/release-373-{name}.log').read_text(encoding='utf8',errors='replace')
    assert marker in s and not re.search(r'^FAIL|Traceback|error:',s,re.M),name
    if name in ('android','wear'):
        assert '17aaf3ebfaa9b51599e2b0d0970f99651bd8be6700da981b7f240677535b7f47' in s
    (Q/f'{name}.log').write_text('\n'.join(line.rstrip() for line in s.splitlines())+'\n',encoding='utf8')
expected={f'TamaPoke-{V}-{s}' for s in ('Android-Full-debug.apk','WearOS-GalaxyWatch4-9-debug.apk',
    'ESP32-Web-Installer.zip','Play-Guide-KO.pdf','Galaxy-Watch4-9-Install-Guide-KO.pdf',
    'Watch-Installer-Windows.zip','Watch-Installer-Guide-KO.pdf')}|{'SHA256SUMS.txt','WATCH-INSTALLER-SHA256SUMS.txt'}
assert {p.name for p in A.iterdir()}==expected
sums=dict(reversed(s.split('  ',1)) for s in (A/'SHA256SUMS.txt').read_text().splitlines())
assert set(sums)==expected-{'SHA256SUMS.txt'} and all(sha(A/n)==h for n,h in sums.items())
for suffix,abis in [('Android-Full-debug.apk',{'arm64-v8a','x86_64'}),('WearOS-GalaxyWatch4-9-debug.apk',{'armeabi-v7a','arm64-v8a'})]:
    with zipfile.ZipFile(A/f'TamaPoke-{V}-{suffix}') as z:
        assert z.testzip() is None
        assert {n.split('/')[1] for n in z.namelist() if n.startswith('lib/') and n.endswith('.so')}==abis
        for abi in abis:
            data=z.read(f'lib/{abi}/libtamapoke.so')
            assert V.encode() in data and '교체 저장 확인 필요'.encode() in data
        for p in [*(R/'web').glob('sprites-*.pak'),R/'web/forms.pak']:
            assert hashlib.sha256(z.read('assets/'+p.name)).hexdigest()==sha(p)
with zipfile.ZipFile(A/f'TamaPoke-{V}-ESP32-Web-Installer.zip') as z:
    assert z.testzip() is None
    assert z.read('firmware/app.bin')==(R/'web/firmware/app.bin').read_bytes()
    assert '교체 저장 확인 필요'.encode() in z.read('firmware/app.bin')
with zipfile.ZipFile(A/f'TamaPoke-{V}-Watch-Installer-Windows.zip') as z:
    assert z.testzip() is None
    prefix=f'TamaPoke-{V}-Watch-Installer-Windows/'
    assert hashlib.sha256(z.read(prefix+'TamaPoke-WearOS.apk')).hexdigest()==sha(A/f'TamaPoke-{V}-WearOS-GalaxyWatch4-9-debug.apk')
    assert hashlib.sha256(z.read(prefix+'Watch-Installer-Guide-KO.pdf')).hexdigest()==sha(A/f'TamaPoke-{V}-Watch-Installer-Guide-KO.pdf')
assert sha(A/f'TamaPoke-{V}-Galaxy-Watch4-9-Install-Guide-KO.pdf')==sha(R/'docs/guides/TamaPoke-Galaxy-Watch4-9-Install-Guide-KO.pdf')
play=PdfReader(A/f'TamaPoke-{V}-Play-Guide-KO.pdf');watch=PdfReader(A/f'TamaPoke-{V}-Watch-Installer-Guide-KO.pdf')
assert len(play.pages)==29 and len(watch.pages)==7
assert '교체 저장 확인 필요' in play.pages[27].extract_text()
table_text=play.pages[28].extract_text()
assert all(name in table_text for _,name,_ in EXAMPLES)
for row in capture_rows()[1:]:
    assert all(cell in table_text for cell in row),row
rates=[int(n) for n in re.search(r'CATCH_RATE\[DEX_COUNT \+ 1\] = \{(.*?)\};',(R/'catch_rates.h').read_text(),re.S)[1].replace('\n','').split(',') if n.strip()]
assert all(rates[dex]==rate for rate,name,dex in EXAMPLES)
assert all(p.extract_text().strip() for p in [*play.pages,*watch.pages])
assert all('3.5.2' not in p.extract_text() and V in p.extract_text() for p in watch.pages)
print('PASS: 9 public assets, signatures, ABI/packs, ESP, Watch bundle and 29/7-page guides')

text=' '.join(' '.join(p.extract_text() for p in play.pages).split())
assert '공용 활력 22' in text and '활력이 22 이상' in text
assert '활력 30' not in text and '활력이 30 이상' not in text
assert '교체 전 수치를 그대로 이어받습니다' in text
assert '#define WILD_ENERGY_COST 22' in (R/'wild.h').read_text()

report=dict(version=V,save_version=7,record_bytes=72,android_version_code=3046,wear_version_code=3047,
  runtime_ci_url='https://github.com/Loaram/TamaPoke_ko/actions/workflows/verify.yml',
  runtime_ci_note='64 suites must pass before publication; see Actions for live final results.',
  sprite_variants=2316,animation_frames=112549,action_lane_cases=74188,
  approved_preview_variants=1158,maximum_animation_scale=4,
  local_regression_suites=2,local_regression_names=['sprite_layout','bond'],installer_checks=30,device_tested=False,
  artifacts={n:{'bytes':(A/n).stat().st_size,'sha256':sha(A/n)} for n in sorted(expected)})
(Q/'build-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PASS: exact public bundle and local build evidence recorded; CI is a separate publication gate')
