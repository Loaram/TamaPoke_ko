"""Record completed local gym beta builds; never publishes or installs them."""
from pathlib import Path
import hashlib,json,shutil,zipfile
R=Path(__file__).resolve().parents[1];B=R/'build/gyms';Q=R/'docs/qa/gyms'
def info(p):return {'bytes':p.stat().st_size,'sha256':hashlib.file_digest(p.open('rb'),'sha256').hexdigest()}
logs={'runtime-tests-final.log':'37 runtime suites','final-focused-tests.log':'PASS: gyms_new,roster,forms_ui','android-build.log':'versionCode 3002','wear-build.log':'versionCode 3003'}
for f,needle in logs.items():assert needle in (B/f).read_text(encoding='utf-8',errors='replace'),f
assert (B/'firmware/compile.log').exists()
artifacts={p.relative_to(R).as_posix():info(p) for p in [B/'emulator/TamaPoke-3.0.0-beta.2-forms.exe',B/'TamaPoke-3.0.0-beta.2-Android.apk',B/'TamaPoke-3.0.0-beta.2-WearOS.apk',B/'firmware/TamaPoke.ino.bin']}
Q.mkdir(parents=True,exist_ok=True)
for f in logs:shutil.copy2(B/f,Q/f)
shutil.copy2(B/'firmware/compile.log',Q/'esp-compile.log')
for f in ('galar.png','paldea.png','still.png','ryme.png','art-preview.png'):shutil.copy2(B/f,Q/f)
for apk in B.glob('*.apk'):
    with zipfile.ZipFile(apk) as z:
        packs=[n for n in z.namelist() if n.endswith('forms.pak')]
        assert len(packs)==1
        assert hashlib.sha256(z.read(packs[0])).hexdigest()==info(R/'web/forms.pak')['sha256']
sources={p.relative_to(R).as_posix():info(p) for p in R.iterdir() if p.suffix in ('.h','.cpp','.ino')}
data={'version':'3.0.0-beta.2','published':False,'device_tested':False,'runtime_suites':37,'new_course_battles':39,'new_course_team_members':160,'artifacts':artifacts,'source_files':sources}
(Q/'build-results.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
print('PASS: final artifacts, source hashes, logs, screenshots and identical form packs recorded')
