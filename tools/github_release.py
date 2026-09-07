"""Publish verified assets using the existing Git credential, never printing it."""
import argparse, hashlib, json, os, re, subprocess, urllib.error, urllib.parse, urllib.request
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('action',choices=['status','create','upload','publish','verify','workflows','jobs','discard-draft','add-watch-installer'])
p.add_argument('--notes',type=Path);p.add_argument('--commit');p.add_argument('--assets',type=Path);p.add_argument('--run',type=int)
p.add_argument('--discard-version',help='explicit obsolete draft version; local matching assets are required')
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
        with urllib.request.urlopen(req,timeout=120) as r:return None if r.status==204 else json.load(r)
    except urllib.error.HTTPError as e:
        if e.code==404 and method=='GET':return None
        raise SystemExit(f'GitHub {method}: HTTP {e.code} {e.reason}') from None
if a.action=='status':
    info=api('');branch=api('/branches/'+info['default_branch']);release=api('/releases/latest')
    print(json.dumps(dict(repository=info['full_name'],push=info.get('permissions',{}).get('push'),default_branch=info['default_branch'],main_commit=branch['commit']['sha'],latest_release=release and release['tag_name']),indent=2));raise SystemExit(0)
if a.action=='workflows':
    runs=api('/actions/runs?per_page=8')['workflow_runs']
    print(json.dumps([{k:r.get(k) for k in ('id','name','head_sha','status','conclusion','html_url')} for r in runs],indent=2));raise SystemExit(0)
if a.action=='jobs':
    assert a.run and a.run>0
    jobs=api(f'/actions/runs/{a.run}/jobs')['jobs']
    print(json.dumps([dict(name=j['name'],status=j['status'],conclusion=j['conclusion'],steps=[{k:s.get(k) for k in ('name','status','conclusion')} for s in j['steps']]) for j in jobs],indent=2));raise SystemExit(0)
version=re.search(r'^#define FW_VERSION "([^"]+)"',(R/'TamaPoke.ino').read_text(encoding='utf8'),re.M)[1]
assert re.fullmatch(r'\d+\.\d+\.\d+',version),'Only a final numbered version may be published'
if a.action=='discard-draft':
    old=a.discard_version
    assert old and re.fullmatch(r'\d+\.\d+\.\d+',old) and old!=version
    # Never remove a public release or an unrelated/unknown draft artifact.
    current=api('/releases/tags/'+version)
    assert current and not current['draft'],'Publish the replacement before removing its obsolete draft'
    matches=[x for x in api('/releases?per_page=100') if x['tag_name']==old]
    assert len(matches)==1 and matches[0]['draft'],'Only one unpublished draft may be discarded'
    draft=matches[0];local=R/'build/release'/old
    expected={f.name:f for f in local.iterdir() if f.is_file()}
    assert len(expected)==6 and {x['name'] for x in draft['assets']}==set(expected)
    assert all(x['size']==expected[x['name']].stat().st_size and x.get('digest')==digest(expected[x['name']]) for x in draft['assets'])
    api('/releases/'+str(draft['id']),'DELETE')
    print(json.dumps(dict(discarded_draft=old,assets=6,local_backup=str(local)),indent=2));raise SystemExit(0)
release=api('/releases/tags/'+version)
if release is None:
    # GitHub's tag endpoint may omit drafts until their tag is published.
    matches=[x for x in api('/releases?per_page=100') if x['tag_name']==version]
    assert len(matches)<=1,'Multiple releases use this version; inspect before continuing'
    release=matches[0] if matches else None
if a.action=='add-watch-installer':
    # Add only the explicitly requested installer supplement; never replace public assets.
    import zipfile
    assert release and not release['draft'] and a.assets and a.notes
    archive_name=f'TamaPoke-{version}-Watch-Installer-Windows.zip'
    guide_name=f'TamaPoke-{version}-Watch-Installer-Guide-KO.pdf'
    sums_name='WATCH-INSTALLER-SHA256SUMS.txt'
    files={f.name:f for f in a.assets.iterdir()}
    assert set(files)=={archive_name,guide_name,sums_name} and all(f.is_file() for f in files.values())
    expected_sums=''.join(digest(files[n])[7:]+'  '+n+'\n' for n in sorted((archive_name,guide_name)))
    assert files[sums_name].read_text(encoding='ascii')==expected_sums
    existing={v['name']:v for v in release['assets']}
    original={v['name']:(v['id'],v['size'],v.get('digest')) for v in release['assets']}
    wear=existing[f'TamaPoke-{version}-WearOS-GalaxyWatch4-9-debug.apk']
    prefix=archive_name[:-4]+'/'
    with zipfile.ZipFile(files[archive_name]) as z:
        names=z.namelist()
        required={'TamaPoke-Watch-Installer.exe','TamaPoke-WearOS.apk','adb.exe','AdbWinApi.dll','AdbWinUsbApi.dll','Watch-Installer-Guide-KO.pdf','SHA256SUMS.txt','version.txt','NOTICE.txt','LICENSE','CREDITS.md','먼저 읽어주세요.txt'}
        assert len(names)==len(set(names)) and {prefix+n for n in required}<=set(names)
        assert set(names)<={prefix+n for n in required|{'libwinpthread-1.dll','source.properties'}}
        assert z.testzip() is None
        with z.open(prefix+'TamaPoke-WearOS.apk') as stream:
            assert 'sha256:'+hashlib.file_digest(stream,'sha256').hexdigest()==wear['digest'],'Not the published official APK'
        assert 'sha256:'+hashlib.sha256(z.read(prefix+'Watch-Installer-Guide-KO.pdf')).hexdigest()==digest(files[guide_name])
    backup=a.assets.parent/'release-before-watch-installer.json'
    if not backup.exists():backup.write_text(json.dumps(release,ensure_ascii=False,indent=2),encoding='utf8')
    for name,f in sorted(files.items()):
        if name in existing:
            assert existing[name]['size']==f.stat().st_size and existing[name].get('digest')==digest(f),'Never overwrite an existing asset'
            print('Already uploaded: '+name,flush=True);continue
        print('Uploading: '+name,flush=True)
        url=release['upload_url'].split('{')[0]+'?name='+urllib.parse.quote(name)
        with f.open('rb') as stream:
            uploaded=api(url,'POST',stream,{'Content-Type':'application/octet-stream','Content-Length':str(f.stat().st_size)})
        assert uploaded['size']==f.stat().st_size and uploaded.get('digest')==digest(f)
    addition=a.notes.read_text(encoding='utf8').strip()
    marker='<!-- watch-installer-supplement -->'
    body=release.get('body') or ''
    if marker in body:
        assert addition in body,'Existing supplement notes differ; inspect manually'
    else:
        release=api('/releases/'+str(release['id']),'PATCH',{'body':body+'\n\n'+marker+'\n'+addition})
    release=api('/releases/tags/'+version)
    after={v['name']:(v['id'],v['size'],v.get('digest')) for v in release['assets']}
    assert all(after[n]==v for n,v in original.items()),'Original assets changed'
    assert not release['draft'] and set(after)==set(original)|set(files)
elif a.action=='create':
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
