"""Record verified private builds for the active-pet exchange beta."""
from pathlib import Path
import hashlib,json,shutil,zipfile
R=Path(__file__).resolve().parents[1]; B=R/'build/active-swap'; Q=R/'docs/qa/active-swap'
def info(p):
    with p.open('rb') as f:
        return {'bytes':p.stat().st_size,'sha256':hashlib.file_digest(f,'sha256').hexdigest()}
logs={'runtime-tests.log':'38 runtime suites','final-focused-tests.log':'PASS: release,active_swap',
      'android-build.log':'versionCode 3004','wear-build.log':'versionCode 3005',
      'esp-build.log':'Application bytes:', 'emulator-build.log':'3.0.0-beta.3-forms.exe'}
for name,needle in logs.items():
    text=(B/name).read_text(encoding='utf-8',errors='replace')
    assert needle in text and 'Traceback' not in text and '\nFAIL ' not in text,name
artifacts=[B/'emulator/TamaPoke-3.0.0-beta.3-forms.exe',B/'TamaPoke-3.0.0-beta.3-Android.apk',
           B/'TamaPoke-3.0.0-beta.3-WearOS.apk',B/'firmware/TamaPoke.ino.bin']
for apk in B.glob('*.apk'):
    with zipfile.ZipFile(apk) as z:
        packs=[n for n in z.namelist() if n.endswith('forms.pak')]
        assert len(packs)==1
        assert hashlib.sha256(z.read(packs[0])).hexdigest()==info(R/'web/forms.pak')['sha256']
Q.mkdir(parents=True,exist_ok=True)
for name in [*logs,'party-detail.png','box-detail.png']:
    shutil.copy2(B/name,Q/name)
sources={p.relative_to(R).as_posix():info(p) for p in R.iterdir() if p.suffix in ('.h','.cpp','.ino')}
data={'version':'3.0.0-beta.3','published':False,'device_tested':False,'runtime_suites':38,
      'save_version':4,'save_max_bytes':9216,'roster_format':'TFR2','record_stride':72,
      'artifacts':{p.relative_to(R).as_posix():info(p) for p in artifacts},'source_files':sources}
(Q/'build-results.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
print('PASS: private active-swap builds, source hashes, logs and screenshots recorded')
