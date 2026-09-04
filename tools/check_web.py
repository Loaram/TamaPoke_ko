#!/usr/bin/env python3
"""Local installer links, bundle structures, manifest and Korean markup checks."""
from pathlib import Path
import hashlib,json,re,struct,subprocess,sys
R=Path(__file__).resolve().parents[1];W=R/'web'
html=(W/'index.html').read_text(encoding='utf-8');js=(W/'installer.js').read_text(encoding='utf-8')
assert '<html lang="ko">' in html and 'TamaPoke 한국어판' in html
assert 'Erase device' in html and '선택하지 않고 Next' in html
assert 'sprites-${region}.pak' in js and 'releases/latest' not in js
manifest=json.loads((W/'manifest.json').read_text(encoding='utf-8'))
info=json.loads((W/'firmware/build-info.json').read_text(encoding='utf-8'))
version=re.search(r'#define FW_VERSION "([^"]+)"',(R/'TamaPoke.ino').read_text(encoding='utf-8'))[1]
assert manifest['version']==info['version']==version and re.fullmatch(r'(?:ko\.\d+\.\d+\.\d+[a-z]?|\d+\.\d+-ko\.\d+)',version)
for filename,expected in info['files'].items():
    data=(W/'firmware'/filename).read_bytes()
    assert len(data)==expected['size'] and hashlib.sha256(data).hexdigest()==expected['sha256'],filename
assert version.encode() in (W/'firmware/app.bin').read_bytes(),'Compiled version marker is missing'
packs=json.loads((W/'packs.json').read_text(encoding='utf-8'));assert len(packs)==9
for pack in packs:
    p=W/pack['path'];assert p.stat().st_size==pack['bytes'];b=p.read_bytes();assert b[:4]==b'TPAK'
    count=struct.unpack_from('<H',b,4)[0];off=6;total=0
    for _ in range(count):
        n=b[off];off+=1;name=b[off:off+n].decode('ascii');off+=n;size=struct.unpack_from('<I',b,off)[0];off+=4;total+=size
        assert re.fullmatch(r'mons/[A-Za-z0-9_.-]+\.bin',name)
    assert off+total==len(b)
subprocess.run([sys.executable,str(R/'tools/check_installer.py')],check=True)
print('PASS: Korean HTML/JS, 9 valid same-origin TPAK bundles, NVS-safe manifest')
