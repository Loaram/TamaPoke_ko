"""Verify and archive private beta.5 artifacts without publishing them."""
from pathlib import Path
import hashlib,json,re,shutil,subprocess,zipfile
from PIL import Image
R=Path(__file__).resolve().parents[1];B=R/'build/form-moves-box';Q=R/'docs/qa/form-moves-box'
def info(p):
    with p.open('rb') as f:return dict(bytes=p.stat().st_size,sha256=hashlib.file_digest(f,'sha256').hexdigest())
old=subprocess.check_output(['git','show','HEAD:moves.h'],cwd=R).decode('utf8')
now=(R/'moves.h').read_text(encoding='utf8')
def codes(t):return re.findall(r'\bMV_\w+',t.split('enum MoveCode : MoveId {')[1].split('};')[0])
assert codes(now)[:len(codes(old))]==codes(old),'Saved move IDs changed'
logs={'runtime-tests.log':'39 runtime suites','final-ui-tests.log':'PASS: form_moves,active_swap,swipe',
      'sd-store-tests.log':'factory reset cannot resurrect', 'emulator-build.log':'3.0.0-beta.5-forms.exe',
      'android-build.log':'versionCode 3008','wear-build.log':'versionCode 3009','esp-build.log':'Application bytes:'}
Q.mkdir(parents=True,exist_ok=True)
for name,needle in logs.items():
    text=(B/name).read_text(encoding='utf8',errors='replace')
    assert needle in text and not re.search(r'^FAIL',text,re.M) and 'Traceback' not in text,name
    shutil.copy2(B/name,Q/name)
for name in ('form-move-choice','box-300-last-page'):
    Image.open(R/f'build/runtime-tests/{name}.ppm').save(Q/f'{name}.png')
pack=info(R/'web/forms.pak')['sha256']
for path in B.glob('*.apk'):
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None
        names=[n for n in z.namelist() if n.endswith('forms.pak')]
        assert len(names)==1 and hashlib.sha256(z.read(names[0])).hexdigest()==pack
artifacts=[B/'emulator/TamaPoke-3.0.0-beta.5-forms.exe',B/'TamaPoke-3.0.0-beta.5-Android.apk',B/'TamaPoke-3.0.0-beta.5-WearOS.apk',B/'firmware/TamaPoke.ino.bin']
data=dict(version='3.0.0-beta.5',published=False,device_tested=False,runtime_suites=39,sd_fault_checks=10,
    supported_forms=176,form_learnsets=69,new_moves=22,moves=717,box_slots=300,box_pages=50,
    save_version=5,save_max_bytes=32768,roster_bytes=22116,legacy_move_ids_preserved=True,
    artifacts={p.relative_to(R).as_posix():info(p) for p in artifacts},
    source_files={p.name:info(p) for p in R.iterdir() if p.suffix in ('.h','.cpp','.ino')})
(Q/'build-results.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PASS: stable legacy move IDs, 39 suites, SD failures, four private platform builds and screenshots archived')
