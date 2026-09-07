"""Read a completed GitHub verification run and preserve its test evidence."""
import argparse, hashlib, io, json, os, re, subprocess, urllib.request, zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--run',type=int,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
assert subprocess.check_output(['git','remote','get-url','origin'],cwd=R,text=True).strip().rstrip('/') in ('https://github.com/Loaram/TamaPoke_ko.git','https://github.com/Loaram/TamaPoke_ko')
env=os.environ.copy();env['GIT_TERMINAL_PROMPT']='0';env['GCM_INTERACTIVE']='never'
credential=subprocess.run(['git','credential','fill'],input='protocol=https\nhost=github.com\n\n',cwd=R,env=env,text=True,capture_output=True,check=True)
fields=dict(s.split('=',1) for s in credential.stdout.splitlines() if '=' in s)
assert fields.get('password'), 'An existing GitHub credential is required'
def get(path,raw=False):
    req=urllib.request.Request('https://api.github.com/repos/Loaram/TamaPoke_ko'+path,
        headers={'Authorization':'Bearer '+fields['password'],'Accept':'application/vnd.github+json','User-Agent':'TamaPoke-CI-evidence'})
    with urllib.request.urlopen(req,timeout=90) as response:data=response.read()
    return data if raw else json.loads(data)
run=get(f'/actions/runs/{a.run}')
print(json.dumps({k:run[k] for k in ('id','head_sha','status','conclusion','html_url')}),flush=True)
if run['status']!='completed':raise SystemExit(3)
out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
archive=get(f'/actions/runs/{a.run}/logs',True)
(out/'logs.zip').write_bytes(archive)
with zipfile.ZipFile(io.BytesIO(archive)) as z:
    for key,needle in [('runtime','Run python tools/test_runtime.py'),('sd-store','Run python tools/test_sd_store.py')]:
        matches=[n for n in z.namelist() if needle.replace('/','_') in n or needle in n]
        assert len(matches)==1, f'Missing or ambiguous {key} step log: {matches}'
        if matches:
            raw=z.read(matches[0]).decode('utf8',errors='replace')
            clean=re.sub(r'^\d{4}-\d\d-\d\dT\S+\s+', '',raw,flags=re.M)
            (out/(key+'.log')).write_text(clean,encoding='utf8')
    if run['conclusion']!='success':
        for name in z.namelist():
            raw=z.read(name).decode('utf8',errors='replace')
            for line in raw.splitlines():
                if re.search(r'\bFAIL\b|error:|Traceback|AssertionError',line):print(line)
        raise SystemExit('CI did not pass; evidence retained without approving release')
assert run['name']=='Verify Korean edition'
names=subprocess.check_output(['git','ls-tree','-r','--name-only',run['head_sha']],cwd=R,text=True).splitlines()
code=[n for n in names if Path(n).suffix in ('.cpp','.h','.ino','.cs') or n in ('tools/test_runtime.py','tools/build_android.py','tools/capture_guide_data.py')]
hashes={n:hashlib.sha256(subprocess.check_output(['git','show',run['head_sha']+':'+n],cwd=R)).hexdigest() for n in code}
report=dict(run_id=a.run,commit=run['head_sha'],conclusion=run['conclusion'],url=run['html_url'],sources=hashes)
(out/'ci.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print('PASS: successful CI evidence and verified source hashes saved')
