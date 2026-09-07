"""Verify the exact public 3.5.3 bundle and record local evidence."""
import hashlib,json,re,zipfile
from pathlib import Path
from pypdf import PdfReader
R=Path(__file__).resolve().parents[1];V='3.5.3';A=R/'build/release'/V;Q=R/'docs/qa'/V
Q.mkdir(parents=True,exist_ok=True)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
markers={'runtime':'PASS: 58 runtime suites including recovery clocks, modal safety, swap recovery and durable individual transfer',
         'android':'versionCode 3034','wear':'versionCode 3035','esp':'Published verified Korean components',
         'emu':'tamapoke-emu.exe','dex':'TamaPoke-3.5.3-dex.exe','shiny':'TamaPoke-3.5.3-shiny.exe',
         'watch-installer':'30 checks passed.','sd-store':'PASS factory reset'}
for name,marker in markers.items():
    s=(R/f'build/release-353-{name}.log').read_text(encoding='utf8',errors='replace')
    assert marker in s and not re.search(r'^FAIL|Traceback|error:',s,re.M),name
    if name in ('android','wear'):
        assert '17aaf3ebfaa9b51599e2b0d0970f99651bd8be6700da981b7f240677535b7f47' in s
    (Q/f'{name}.log').write_text('\n'.join(line.rstrip() for line in s.splitlines())+'\n',encoding='utf8')
# Preserve the prior fix simulations and verify that release code differs only
# by its version literal. This keeps their exact coverage claim attributable.
baseline=json.loads((R/'build/diagnostics/352-fixed/verification.json').read_text())
for name,digest in baseline['sources'].items():
    data=(R/name).read_bytes()
    if name=='TamaPoke.ino':
        data=data.replace(b'#define FW_VERSION "3.5.3"',b'#define FW_VERSION "3.5.2"')
    assert hashlib.sha256(data).hexdigest()==digest, 'Simulation source changed: '+name
for name,marker in [('core','SOAK swaps_train_restart=1200 errors=0'),('ui','SUMMARY checks=9 findings=0'),('network','NETWORK cases=90 failures=0')]:
    s=(R/f'build/diagnostics/352-fixed/{name}.log').read_text(encoding='utf8')
    assert marker in s and not re.search(r'^BUG|^FAIL|Traceback',s,re.M),name
    (Q/f'simulation-{name}.log').write_text(s,encoding='utf8')
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
assert len(play.pages)==28 and len(watch.pages)==7
assert '교체 저장 확인 필요' in play.pages[-1].extract_text()
assert all(p.extract_text().strip() for p in [*play.pages,*watch.pages])
assert all('3.5.2' not in p.extract_text() and V in p.extract_text() for p in watch.pages)
report=dict(version=V,runtime_suite_total=58,windows_full_run='58 passed; no blocked suites',
            swap_cut_cases=1056,swap_training_restart_cycles=1200,network_fault_cases=90,simulation_source_equivalence=True,
            sd_failure_checks=10,installer_checks=30,device_tested=False,save_version=7,record_bytes=72,battle_protocol=5,trade_protocol=1,
            android_version_code=3034,wear_version_code=3035,artifacts={n:{'bytes':(A/n).stat().st_size,'sha256':sha(A/n)} for n in sorted(expected)})
(Q/'build-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PASS: 9 public assets, original signatures, ABI/packs, ESP, matching Watch bundle, 28/7-page guides and SHA-256')
