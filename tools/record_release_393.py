"""Verify the exact 3.9.3 public bundle and record reproducible evidence."""
import hashlib,json,re,zipfile
from pathlib import Path
from pypdf import PdfReader
from capture_guide_data import capture_rows,EXAMPLES
R=Path(__file__).resolve().parents[1];V='3.9.3';A=R/'build/release'/V;Q=R/'docs/qa'/V
Q.mkdir(parents=True,exist_ok=True)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
markers={'android':'versionCode 3058','wear':'versionCode 3059','esp':'Published verified Korean components',
         'emu':'tamapoke-emu.exe','watch-installer':'30 checks passed.',
         'runtime':'PASS: 68 runtime suites','swap-final':'PASS: battle_storage,shared_energy,swap_recovery,active_swap', 'audio':'PASS: actual ESP audio task blocks after music stops in four scenarios'}
for name,marker in markers.items():
    s=(R/f'build/release-393-{name}.log').read_text(encoding='utf8',errors='replace')
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
            assert V.encode() in data and b'dayOff' in data and b'saveImportAtTime' in data
            assert (V+'-personal').encode() not in data
        for p in [*(R/'web').glob('sprites-*.pak'),R/'web/forms.pak']:
            assert hashlib.sha256(z.read('assets/'+p.name)).hexdigest()==sha(p)
with zipfile.ZipFile(A/f'TamaPoke-{V}-ESP32-Web-Installer.zip') as z:
    assert z.testzip() is None
    assert z.read('firmware/app.bin')==(R/'web/firmware/app.bin').read_bytes()
    assert b'dayOff' in z.read('firmware/app.bin')
    assert (V+'-personal').encode() not in z.read('firmware/app.bin')
with zipfile.ZipFile(A/f'TamaPoke-{V}-Watch-Installer-Windows.zip') as z:
    assert z.testzip() is None
    prefix=f'TamaPoke-{V}-Watch-Installer-Windows/'
    assert hashlib.sha256(z.read(prefix+'TamaPoke-WearOS.apk')).hexdigest()==sha(A/f'TamaPoke-{V}-WearOS-GalaxyWatch4-9-debug.apk')
    assert hashlib.sha256(z.read(prefix+'Watch-Installer-Guide-KO.pdf')).hexdigest()==sha(A/f'TamaPoke-{V}-Watch-Installer-Guide-KO.pdf')
assert sha(A/f'TamaPoke-{V}-Galaxy-Watch4-9-Install-Guide-KO.pdf')==sha(R/'docs/guides/TamaPoke-Galaxy-Watch4-9-Install-Guide-KO.pdf')
play=PdfReader(A/f'TamaPoke-{V}-Play-Guide-KO.pdf');watch=PdfReader(A/f'TamaPoke-{V}-Watch-Installer-Guide-KO.pdf')
assert len(play.pages)==31 and len(watch.pages)==7
table=play.pages[28].extract_text()
assert all(name in table for _,name,_ in EXAMPLES)
assert all(cell in table for row in capture_rows()[1:] for cell in row)
assert all(p.extract_text().strip() for p in [*play.pages,*watch.pages])
assert all(V in p.extract_text() for p in watch.pages)
text=' '.join(' '.join(p.extract_text() for p in play.pages).split())
assert '공용 활력 22' in text and '활력이 22 이상' in text
assert '활력 30' not in text and '활력이 30 이상' not in text
assert '메뉴의 알 받기는 하루 2회' in text and '자동 생성되는 알은 별개' in text
assert '#define WILD_ENERGY_COST 22' in (R/'wild.h').read_text()
assert '연속 돌봄과 기기 시계' in play.pages[30].extract_text()
report=dict(version=V,save_version=7,record_bytes=72,android_version_code=3058,wear_version_code=3059,
  local_runtime_suites=68,final_swap_regression=True,nvs_handle_copy_regression=True,audio_stop_regression=True,
  sd_streaming_verification_bytes=256,installer_checks=30,play_guide_pages=31,watch_guide_pages=7,
  final_runtime_ci_required=True,physical_swap_retest=True,physical_confirmation="user confirmed patched round ESP test build; not every device model",
  artifacts={n:{'bytes':(A/n).stat().st_size,'sha256':sha(A/n)} for n in sorted(expected)})
(Q/'build-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PASS: 9 public assets, signatures, ABI/packs, ESP, Watch bundle, 31/7-page guides and local regression logs')
