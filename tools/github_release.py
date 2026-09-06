"""Publish verified assets using the existing Git credential, never printing it."""
import argparse, hashlib, json, os, re, subprocess, urllib.error, urllib.parse, urllib.request
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('action',choices=['status','create','upload','publish','verify','workflows'])
p.add_argument('--notes',type=Path);p.add_argument('--commit');p.add_argument('--assets',type=Path)
a=p.parse_args()
def digest(path):
    with path.open('rb') as stream:return 'sha256:'+hashlib.file_digest(stream,'sha256').hexdigest()
remote=subprocess.check_output(['git','remote','get-url','origin'],cwd=R,text=True).strip()
assert remote.rstrip('/') in ('https://github.com/Loaram/TamaPoke_ko.git','https://github.com/Loaram/TamaPoke_ko'),remote
repo='Loaram/TamaPoke_ko';base='https://api.github.com/repos/'+repo
env=os.environ.copy();env['GIT_TERMINAL_PROMPT']='0';env['GCM_INTERACTIVE']='never'
result=subprocess.run(['git','credential','fill'],input='protocol=https\nhost=github.com\n\n',cwd=R,env=env,text=True,capture_output=True,timeout=30)
cred=dict(line.split('=',1) for line in result.stdout.splitlines() if '=' in line)
if not cred.get('password'):raise SystemExit('No existing GitHub Git credential available; sign in before publishing.')
headers={'Authorization':'Bearer '+cred['password'],'Accept':'application/vnd.github+json','User-Agent':'TamaPoke-release-audit','X-GitHub-Api-Version':'2022-11-28'}
def api(path,method='GET',data=None,extra=None):
    h=dict(headers);h.update(extra or {})
    if isinstance(data,dict):data=json.dumps(data).encode();h['Content-Type']='application/json'
    req=urllib.request.Request(path if path.startswith('https://') else base+path,data=data,headers=h,method=method)
    try:
        with urllib.request.urlopen(req,timeout=120) as r:return json.load(r)
    except urllib.error.HTTPError as e:
        if e.code==404 and method=='GET':return None
        raise SystemExit(f'GitHub {method}: HTTP {e.code} {e.reason}') from None
if a.action=='status':
    info=api('');branch=api('/branches/'+info['default_branch']);release=api('/releases/latest')
    print(json.dumps(dict(repository=info['full_name'],push=info.get('permissions',{}).get('push'),default_branch=info['default_branch'],main_commit=branch['commit']['sha'],latest_release=release and release['tag_name']),indent=2));raise SystemExit(0)
if a.action=='workflows':
    runs=api('/actions/runs?per_page=8')['workflow_runs']
    print(json.dumps([{k:r.get(k) for k in ('id','name','head_sha','status','conclusion','html_url')} for r in runs],indent=2));raise SystemExit(0)
version=re.search(r'^#define FW_VERSION "([^"]+)"',(R/'TamaPoke.ino').read_text(encoding='utf8'),re.M)[1]
assert re.fullmatch(r'\d+\.\d+\.\d+',version),'Only a final numbered version may be published'
release=api('/releases/tags/'+version)
if a.action=='create':
    assert a.notes and a.commit and re.fullmatch(r'[0-9a-f]{40}',a.commit)
    assert api('/commits/'+a.commit),'Commit has not been pushed'
    payload=dict(tag_name=version,target_commitish=a.commit,name='TamaPoke '+version,body=a.notes.read_text(encoding='utf8'),draft=True,prerelease=False)
    if release:
        assert release['draft'],'Refusing to overwrite a published release'
        release=api('/releases/'+str(release['id']),'PATCH',payload)
    else:release=api('/releases','POST',payload)
elif a.action=='upload':
    assert release and release['draft'] and a.assets
    files=sorted(a.assets.iterdir());assert all(f.is_file() for f in files)
    assert all(f.suffix in ('.apk','.zip','.pdf','.txt') for f in files),'Unexpected public asset type'
    assert not any('-dex' in f.name or '-shiny' in f.name for f in files),'Private emulator in public assets'
    existing={v['name']:v for v in release['assets']}
    for f in files:
        if f.name in existing:
            assert existing[f.name]['size']==f.stat().st_size and existing[f.name].get('digest')==digest(f),'Existing draft asset differs; inspect before replacing'
            print('Already uploaded: '+f.name,flush=True);continue
        url=release['upload_url'].split('{')[0]+'?name='+urllib.parse.quote(f.name)
        print('Uploading: '+f.name,flush=True)
        with f.open('rb') as stream:
            uploaded=api(url,'POST',stream,{'Content-Type':'application/octet-stream','Content-Length':str(f.stat().st_size)})
        assert uploaded['size']==f.stat().st_size and uploaded.get('digest')==digest(f)
        print('Uploaded: '+f.name,flush=True)
elif a.action=='publish':
    assert release and a.assets
    local={f.name:f.stat().st_size for f in a.assets.iterdir() if f.is_file()}
    hosted={f['name']:f['size'] for f in release['assets']}
    assert local==hosted and len(local)>=6,'Release asset set is incomplete'
    assert all(x.get('digest')==digest(a.assets/x['name']) for x in release['assets']),'Hosted SHA-256 mismatch'
    release=api('/releases/'+str(release['id']),'PATCH',dict(draft=False,prerelease=False,make_latest='true'))
elif a.action=='verify':
    assert release
print(json.dumps(dict(url=release['html_url'],tag=release['tag_name'],draft=release['draft'],assets=[{k:x.get(k) for k in ('name','size','digest','browser_download_url')} for x in release['assets']]),indent=2))
