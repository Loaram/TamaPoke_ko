"""Verify the exact public 3.6.1 bundle and record local evidence."""
import hashlib,json,re,zipfile,sys
from pathlib import Path
from pypdf import PdfReader
from capture_guide_data import probability, capture_rows, EXAMPLES
R=Path(__file__).resolve().parents[1];V='3.6.1';A=R/'build/release'/V;Q=R/'docs/qa'/V
Q.mkdir(parents=True,exist_ok=True)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
markers={'android':'versionCode 3038','wear':'versionCode 3039','esp':'Published verified Korean components',
         'emu':'tamapoke-emu.exe','watch-installer':'30 checks passed.'}
for name,marker in markers.items():
    s=(R/f'build/release-361-{name}.log').read_text(encoding='utf8',errors='replace')
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
if '--artifacts-only' in sys.argv:raise SystemExit(0)
ci_dir=R/'build/3.6.1/ci'
ci=json.loads((ci_dir/'ci.json').read_text());assert ci['conclusion']=='success'
for name,digest in ci['sources'].items():
    assert hashlib.sha256((R/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest,name
runtime=(ci_dir/'runtime.log').read_text(encoding='utf8')
assert 'PASS: 60 runtime suites including evolution-family collection locks, capture bonus and save recovery' in runtime
assert 'MATRIX rate_count_cases=247808' in runtime
assert '967 collectible, 15 additional locks' in runtime
assert 'historical registrations preserved while collectible numerator excludes all locks' in runtime
assert not re.search(r'^FAIL\b|Traceback|AssertionError',runtime,re.M)
samples=re.findall(r'SIM dex=(\d+) rate=(\d+) expected=([\d.]+)% observed=([\d.]+)%',runtime)
assert len(samples)==50
for count,rate,expected_percent,observed in samples:
    assert abs(float(expected_percent)-100*probability(int(rate),int(count)))<0.00000002
sd=(ci_dir/'sd-store.log').read_text(encoding='utf8');assert len(re.findall(r'^PASS ',sd,re.M))==10
for name,data in [('runtime.log',runtime),('sd-store.log',sd)]:
    (Q/name).write_text('\n'.join(line.rstrip() for line in data.splitlines())+'\n',encoding='utf8')
(Q/'ci-evidence.json').write_text(json.dumps(ci,indent=2)+'\n',encoding='utf8')
report=dict(version=V,runtime_suite_total=60,runtime_environment='GitHub Ubuntu',windows_full_run='recovery_clock executable blocked by WinError 4551; no security changes',
            collectible_species=967,locked_species=58,new_family_locks=15,dex_half_goal=484,
            rate_count_cases=247808,capture_simulation_draws=5000000,simulation_source_equivalence=True,ci_url=ci['url'],
            sd_failure_checks=10,installer_checks=30,device_tested=False,save_version=7,record_bytes=72,battle_protocol=5,trade_protocol=1,
            android_version_code=3038,wear_version_code=3039,artifacts={n:{'bytes':(A/n).stat().st_size,'sha256':sha(A/n)} for n in sorted(expected)})
(Q/'build-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PASS: 60 CI suites, 247808 rate/count cases, 5000000 captures, guide table and unchanged verified code')
