"""Record private beta.4 builds and the exact animation-only form filter."""
from pathlib import Path
import hashlib,json,shutil,zipfile
from audit_animation import pack_contents,animation_info
R=Path(__file__).resolve().parents[1];B=R/'build/animation-audit';Q=R/'docs/qa/animation-filter'
def info(p):
    with p.open('rb') as f:
        return {'bytes':p.stat().st_size,'sha256':hashlib.file_digest(f,'sha256').hexdigest()}
logs={'runtime-tests.log':'38 runtime suites','focused-tests.log':'PASS: forms,forms_ui,active_swap,gyms_new',
      'android-build.log':'versionCode 3006','wear-build.log':'versionCode 3007',
      'esp-build.log':'Application bytes:', 'emulator-build.log':'3.0.0-beta.4-forms.exe'}
for name,needle in logs.items():
    text=(B/name).read_text(encoding='utf-8',errors='replace')
    assert needle in text and 'Traceback' not in text and '\nFAIL ' not in text,name
before=json.loads((B/'forms-before.json').read_text(encoding='utf-8'))
excluded=[e for e in before if not e['animated']]
assert {e['form_id'] for e in excluded}=={10365,10531,10392} and len(excluded)==6
pack=R/'web/forms.pak';contents=pack_contents(pack)
assert len(contents)==344 and all(animation_info(b)['animated'] for b in contents.values())
assert not {e['file'] for e in excluded}.intersection(contents)
old=json.loads((R/'docs/qa/active-swap/build-results.json').read_text())
for name in ('trainers.h','trainers_new.h','gym_art.h'):
    assert info(R/name)['sha256']==old['source_files'][name]['sha256'],f'Gyms changed: {name}'
artifacts=[B/'emulator/TamaPoke-3.0.0-beta.4-forms.exe',B/'TamaPoke-3.0.0-beta.4-Android.apk',
           B/'TamaPoke-3.0.0-beta.4-WearOS.apk',B/'firmware/TamaPoke.ino.bin']
for apk in B.glob('*.apk'):
    with zipfile.ZipFile(apk) as z:
        packs=[n for n in z.namelist() if n.endswith('forms.pak')]
        assert len(packs)==1 and hashlib.sha256(z.read(packs[0])).hexdigest()==info(pack)['sha256']
Q.mkdir(parents=True,exist_ok=True)
for name in [*logs,'charizard.png','chimecho.png','eternamax.png']:
    shutil.copy2(B/name,Q/name)
sources={p.relative_to(R).as_posix():info(p) for p in R.iterdir() if p.suffix in ('.h','.cpp','.ino')}
data={'version':'3.0.0-beta.4','published':False,'device_tested':False,'runtime_suites':38,
      'supported_forms':176,'sprite_files':344,'excluded':excluded,'gyms_unchanged':True,
      'pack':info(pack),'artifacts':{p.relative_to(R).as_posix():info(p) for p in artifacts},'source_files':sources}
(Q/'build-results.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('PASS: 176 animated forms, 344 files, 3 exclusions, unchanged gyms and all private builds recorded')
