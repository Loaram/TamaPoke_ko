"""Read-only verification of the current numbered release, main and Pages."""
import concurrent.futures
import hashlib
import json
import subprocess
import urllib.request
from pathlib import Path
from prepare_release_guides import ROOT, firmware_version, sha256

version = firmware_version()
commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
base = 'https://api.github.com/repos/Loaram/TamaPoke_ko'

def get(url, head=False):
    req = urllib.request.Request(url, headers={'User-Agent': 'TamaPoke-public-verification'},
                                 method='HEAD' if head else 'GET')
    with urllib.request.urlopen(req, timeout=60) as response:
        assert response.status == 200
        return b'' if head else response.read()

def api(path):
    return json.loads(get(base + path))

local = json.loads((ROOT / f'docs/qa/{version}/build-results.json').read_text(encoding='utf8'))['artifacts']
release = api('/releases/tags/' + version)
assert not release['draft'] and not release['prerelease']
assert api('/releases/latest')['tag_name'] == version
assert api('/branches/main')['commit']['sha'] == commit
assert api('/git/ref/tags/' + version)['object']['sha'] == commit
assert {a['name'] for a in release['assets']} == set(local)

def check(asset):
    name = asset['name']
    assert asset['browser_download_url'].startswith(
        f'https://github.com/Loaram/TamaPoke_ko/releases/download/{version}/')
    assert asset['size'] == local[name]['bytes']
    assert asset['digest'] == 'sha256:' + local[name]['sha256']
    get(asset['browser_download_url'], head=True)
    return name

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    for name in pool.map(check, release['assets']):
        print('PASS public download and SHA-256:', name, flush=True)

site = 'https://loaram.github.io/TamaPoke_ko/'
assert json.loads(get(site + 'manifest.json?verify=' + commit))['version'] == version
html = get(site + '?verify=' + commit).decode('utf8')
guide_name = f'TamaPoke-{version}-Play-Guide-KO.pdf'
assert guide_name in html and '30쪽' in html
assert hashlib.sha256(get(site + 'firmware/app.bin?verify=' + commit)).hexdigest() == sha256(ROOT / 'web/firmware/app.bin')
assert hashlib.sha256(get(site + 'guides/' + guide_name)).hexdigest() == local[guide_name]['sha256']
print('PASS public Pages version, firmware and PDF', flush=True)

runs = api('/actions/runs?head_sha=' + commit + '&per_page=30')['workflow_runs']
passed = {}
for name in ('Verify Korean edition', 'Deploy installer to GitHub Pages'):
    matches = [r for r in runs if r['name'] == name and r['head_sha'] == commit and r['conclusion'] == 'success']
    assert matches, 'Still awaiting successful workflow: ' + name
    passed[name] = matches[0]['html_url']
report = dict(version=version, commit=commit, public_assets=len(local), workflows=passed,
              pages='manifest, HTML, firmware and PDF verified')
out = ROOT / f'build/{version}/published-verification.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
print('PASS main, tag, latest release and successful same-commit CI')
