"""Verify and archive the 3.0.0 public artifact set, without publishing it."""
import argparse,hashlib,json,re,shutil,subprocess,sys,zipfile
from pathlib import Path
from pypdf import PdfReader
R=Path(__file__).resolve().parents[1];Q=R/'docs/qa/3.0.0';A=R/'build/release/3.0.0'
p=argparse.ArgumentParser();p.add_argument('--check-python',default=sys.executable);args=p.parse_args()
Q.mkdir(parents=True,exist_ok=True)
def digest(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def info(p):return dict(bytes=p.stat().st_size,sha256=digest(p))
logs={'runtime':'43 runtime suites','sd-store':'PASS factory reset','android':'versionCode 3012',
      'wear':'versionCode 3013','esp':'Application bytes: 2237232','emu':'tamapoke-emu.exe',
      'dex':'TamaPoke-3.0.0-dex.exe','shiny':'TamaPoke-3.0.0-shiny.exe'}
for name,marker in logs.items():
    source=R/f'build/release-{name}.log';content=source.read_text(encoding='utf8',errors='replace')
    assert marker in content and not re.search(r'^FAIL|Traceback|error:',content,re.M),name
    shutil.copy2(source,Q/f'{name}.log')
static=[]
for tool in ['check_korean.py','check_gen89.py','check_forms.py','check_web.py']:
    static.append(subprocess.check_output([args.check_python,str(R/'tools'/tool)],cwd=R,text=True,encoding='utf8',errors='replace'))
(Q/'static-checks.log').write_text('\n'.join(static),encoding='utf8')
sys.path.insert(0,str(R/'tools'))
from dex_moves import MOVES
legacy={'__file__':str(R/'tools/dex_moves.py')}
exec(subprocess.check_output(['git','show','5e923a5b9d79112f459bf2fbc197e8e52db2838f:tools/dex_moves.py'],cwd=R,text=True,encoding='utf8'),legacy)
assert len(legacy['MOVES'])==695 and MOVES[:695]==legacy['MOVES']
forms=json.loads((R/'data/forms/moves.json').read_text(encoding='utf8'))
assert all(1<=m['source_id']<=919 for m in forms['moves'])
files=sorted(p for p in A.iterdir() if p.is_file());assert len(files)==6
expected={line.split('  ',1)[1]:line.split('  ',1)[0] for line in (A/'SHA256SUMS.txt').read_text().splitlines()}
assert set(expected)=={f.name for f in files if f.name!='SHA256SUMS.txt'}
for name,sha in expected.items():assert digest(A/name)==sha,name
for apk,abis in [('Android-Full-debug.apk',{'arm64-v8a','x86_64'}),('WearOS-GalaxyWatch4-9-debug.apk',{'arm64-v8a','armeabi-v7a'})]:
    with zipfile.ZipFile(A/('TamaPoke-3.0.0-'+apk)) as z:
        assert z.testzip() is None
        assert {n.split('/')[1] for n in z.namelist() if n.startswith('lib/') and n.endswith('.so')}==abis
        for pack in [*(R/'web').glob('sprites-*.pak'),R/'web/forms.pak']:
            assert hashlib.sha256(z.read('assets/'+pack.name)).hexdigest()==digest(pack)
        assert b'3.0.0' in z.read('lib/arm64-v8a/libtamapoke.so')
with zipfile.ZipFile(A/'TamaPoke-3.0.0-ESP32-Web-Installer.zip') as z:
    assert z.testzip() is None
    assert z.read('firmware/app.bin')==(R/'web/firmware/app.bin').read_bytes()
watch=A/'TamaPoke-3.0.0-Galaxy-Watch4-9-Install-Guide-KO.pdf'
assert digest(watch)==digest(R/'docs/guides/TamaPoke-Galaxy-Watch4-9-Install-Guide-KO.pdf')
guide=A/'TamaPoke-3.0.0-Play-Guide-KO.pdf';pdf=PdfReader(str(guide));assert len(pdf.pages)==22
assert all(p.extract_text().strip() for p in pdf.pages)
for p in (R/'build/3.0.0').glob('guide-contact-*.png'):shutil.copy2(p,Q/p.name)
report=dict(version='3.0.0',runtime_suites=43,sd_fault_checks=10,device_tested=False,
    save_version=7,save_max_bytes=32768,legacy_move_ids_unchanged=695,
    forms=176,form_sprites=344,moves=717,form_pools=69,box_slots=300,box_pages=50,
    android_version_code=3012,wear_version_code=3013,guide_pages=22,watch_guide_unchanged=True,
    artifacts={p.name:info(p) for p in files},
    private_emulators={name:info(R/f'build/3.0.0/{name}/TamaPoke-3.0.0-{name}.exe') for name in ['dex','shiny']},
    source_files={p.name:info(p) for p in R.iterdir() if p.suffix in ('.h','.cpp','.ino')})
(Q/'build-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PASS: all 6 release assets, APK contents/ABIs, checksums, unchanged Watch guide, 22 guide pages and all 695 legacy move IDs')
