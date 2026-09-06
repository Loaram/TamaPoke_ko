#!/usr/bin/env python3
"""Check the shipped form pack against its pinned catalogue and generated table."""
import hashlib,json,struct
from pathlib import Path
from gen_forms import display_name,CLASSES,supported,shiny_supported
from audit_animation import animation_info
R=Path(__file__).resolve().parents[1]
data=json.loads((R/'data/forms/catalog.json').read_text(encoding='utf-8'))
art_credits=json.loads((R/'data/forms/art-credits.json').read_text(encoding='utf-8'))['files']
entries=[e for e in data['entries'] if supported(e)]
blob=(R/'web/forms.pak').read_bytes();assert blob[:4]==b'TPAK'
count=struct.unpack_from('<H',blob,4)[0];at=6;directory=[]
for _ in range(count):
    n=blob[at];at+=1;name=blob[at:at+n].decode('ascii');at+=n
    size=struct.unpack_from('<I',blob,at)[0];at+=4
    assert name.startswith('mons/') and '/' not in name[5:] and size>0
    directory.append((name,size))
contents={}
for name,size in directory:
    assert name not in contents and at+size<=len(blob)
    contents[name]=blob[at:at+size];at+=size
assert at==len(blob)
expected=set();credits=[]
for e in entries:
    assert e['classification'] in CLASSES and e['level'] in (60,70,80)
    assert all(0<=x<=255 for x in e['stats']) and e['types'][0]<18
    assert e['types'][1]==255 or e['types'][1]<18
    for shiny in (False,True) if shiny_supported(e) else (False,):
        name=f"mons/p{'s' if shiny else ''}{e['dex']:03}f{e['form_id']:05}.bin"
        expected.add(name);b=contents[name];assert b[:4]==b'TPK2'
        assert animation_info(b)['animated'],f'Non-animated form shipped: {name}'
        sha=hashlib.sha256(b).hexdigest()
        if not shiny: assert sha==e['sha256']
        credits.append({'file':name,'sha256':sha,'dex':e['dex'],'form_id':e['form_id'],
            'name_ko':display_name(e),'shiny':shiny,'credits':art_credits[name],
            'source_path':e['pmd_shiny_path'] if shiny else e['pmd_normal_path']})
assert expected==set(contents)
assert len(entries)==176 and len(contents)==344
manifest={'pmd_revision':data['pmd_revision'],'license':entries[0]['license_source'],
          'pak_sha256':hashlib.sha256(blob).hexdigest(),'files':credits}
out=R/'build/forms-runtime/art-manifest.json';out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(f'PASS: {len(entries)} forms, {len(contents)} exact PMD sprites, types/stats/IDs and hashes; {len(data["entries"])-len(entries)} unavailable candidates excluded')
