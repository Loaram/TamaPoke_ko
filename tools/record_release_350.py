"""Validate the public 3.5.0 artifacts; never upload files."""
import hashlib,json,re,shutil,subprocess,zipfile
from pathlib import Path
from pypdf import PdfReader
R=Path(__file__).resolve().parents[1];Q=R/'docs/qa/3.5.0';A=R/'build/release/3.5.0'
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def info(p):return dict(bytes=p.stat().st_size,sha256=sha(p))
Q.mkdir(parents=True,exist_ok=True)
logs={name:R/f'build/release-350-{name}.log' for name in ['runtime','runtime-tail','trade','sd-store','android','wear','esp','emu','dex','shiny']}
markers={'runtime-tail':'PASS: wild_result,shiny_eggs,battle_reserves','trade':'PASS: individual transfer transaction tests',
         'sd-store':'PASS factory reset','android':'versionCode 3028','wear':'versionCode 3029',
         'esp':'Application bytes:','emu':'tamapoke-emu.exe','dex':'TamaPoke-3.5.0-dex.exe','shiny':'TamaPoke-3.5.0-shiny.exe'}
for name,path in logs.items():
    content=path.read_text(encoding='utf8',errors='replace')
    if name=='runtime':assert '[WinError 4551]' in content,'Inspect the initial local runtime result'
    else:assert markers[name] in content and not re.search(r'^FAIL|Traceback|error:',content,re.M),name
    shutil.copy2(path,Q/(name+'.log'))
files=sorted(p for p in A.iterdir() if p.is_file());assert len(files)==6
checks={line.split('  ',1)[1]:line.split('  ',1)[0] for line in (A/'SHA256SUMS.txt').read_text().splitlines()}
assert set(checks)=={p.name for p in files if p.name!='SHA256SUMS.txt'}
for n,h in checks.items():assert sha(A/n)==h,n
for suffix,abis in [('Android-Full-debug',{'arm64-v8a','x86_64'}),('WearOS-GalaxyWatch4-9-debug',{'arm64-v8a','armeabi-v7a'})]:
    with zipfile.ZipFile(A/f'TamaPoke-3.5.0-{suffix}.apk') as z:
        assert z.testzip() is None
        assert {n.split('/')[1] for n in z.namelist() if n.startswith('lib/') and n.endswith('.so')}==abis
        for pack in [*(R/'web').glob('sprites-*.pak'),R/'web/forms.pak']:
            assert hashlib.sha256(z.read('assets/'+pack.name)).hexdigest()==sha(pack)
        for abi in abis:assert b'3.5.0' in z.read(f'lib/{abi}/libtamapoke.so')
with zipfile.ZipFile(A/'TamaPoke-3.5.0-ESP32-Web-Installer.zip') as z:
    assert z.testzip() is None
    assert z.read('firmware/app.bin')==(R/'web/firmware/app.bin').read_bytes()
assert sha(A/'TamaPoke-3.5.0-Galaxy-Watch4-9-Install-Guide-KO.pdf')==sha(R/'docs/guides/TamaPoke-Galaxy-Watch4-9-Install-Guide-KO.pdf')
pdf=PdfReader(str(A/'TamaPoke-3.5.0-Play-Guide-KO.pdf'));assert len(pdf.pages)==27
assert all(p.extract_text().strip() for p in pdf.pages)
assert '한 마리' in pdf.pages[25].extract_text() and '재연결' in pdf.pages[26].extract_text()
for p in (R/'build/3.5.0').glob('guide-contact-*.png'):shutil.copy2(p,Q/p.name)
report=dict(version='3.5.0',runtime_suites_defined=55,local_full_runtime_blocked_by_windows=True,
    ci_full_runtime_required_before_publication=True,device_tested=False,individual_trade_protocol=1,
    durable_checkpoint=True,transaction_power_cut_points=5,save_version=7,party_record_bytes=72,box_slots=300,
    battle_protocol=5,android_version_code=3028,wear_version_code=3029,guide_pages=27,watch_guide_unchanged=True,
    artifacts={p.name:info(p) for p in files},source_files={p.name:info(p) for p in R.iterdir() if p.suffix in ('.h','.cpp','.ino')})
(Q/'build-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PASS: 6 public assets, APK ABI/sprite contents, hashes, ESP image, 27-page play guide and unchanged Watch guide')
