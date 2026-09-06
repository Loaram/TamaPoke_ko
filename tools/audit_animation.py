"""Audit real, distinct frames in the local TPK2 assets (no network)."""
import argparse,json,struct
from pathlib import Path
R=Path(__file__).resolve().parents[1]

def animation_info(blob):
    assert blob[:4]==b'TPK2'
    count,pal=struct.unpack_from('<BH',blob,4);at=7+pal*2;actions=[]
    palette=struct.unpack_from('<'+'H'*pal,blob,7)
    for _ in range(count):
        action,w,h,n=struct.unpack_from('<BBBB',blob,at);at+=4
        durations=struct.unpack_from('<'+'H'*n,blob,at);at+=2*n
        frames=[]
        for i in range(n):
            frame=blob[at:at+w*h];assert len(frame)==w*h;at+=w*h
            # Equal rendered pixels count as equal even if palette indices differ.
            frames.append(tuple(-1 if x==255 else palette[x] for x in frame))
        unique=len(set(frames))
        actions.append({'id':action,'frames':n,'distinct_frames':unique,'animated':unique>1 and all(durations)})
    assert at==len(blob)
    return {'animated':any(a['animated'] for a in actions),
            'idle_animated':any(a['id']==0 and a['animated'] for a in actions),'actions':actions}

def pack_contents(path):
    blob=path.read_bytes();assert blob[:4]==b'TPAK'
    count=struct.unpack_from('<H',blob,4)[0];at=6;directory=[]
    for _ in range(count):
        n=blob[at];at+=1;name=blob[at:at+n].decode('ascii');at+=n
        size=struct.unpack_from('<I',blob,at)[0];at+=4;directory.append((name,size))
    result={}
    for name,size in directory:
        assert name not in result and at+size<=len(blob)
        result[name]=blob[at:at+size];at+=size
    assert at==len(blob)
    return result

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--update-catalog',action='store_true');args=parser.parse_args()
    data=json.loads((R/'data/forms/catalog.json').read_text(encoding='utf-8'))
    blobs=pack_contents(R/'web/forms.pak');rows=[]
    for e in data['entries']:
        if not e['normal_body_verified']:continue
        for shiny in [False,True] if e['shiny_body_verified'] else [False]:
            name=f"mons/p{'s' if shiny else ''}{e['dex']:03}f{e['form_id']:05}.bin"
            if name not in blobs:
                assert not args.update_catalog, f'Cannot update from a partial pack: {name}'
                continue
            rows.append({'key':e['key'],'species_ko':e['species_ko'],'form_name_ko':e['form_name_ko'],
                         'form_id':e['form_id'],'shiny':shiny,'file':name,**animation_info(blobs[name])})
    out=R/'build/animation-audit';out.mkdir(parents=True,exist_ok=True)
    (out/'forms.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    if args.update_catalog:
        lookup={(x['form_id'],x['shiny']):x for x in rows}
        for e in data['entries']:
            if not e['normal_body_verified']:continue
            e['normal_animated']=lookup[e['form_id'],False]['animated']
            e['shiny_animated']=e['shiny_body_verified'] and lookup[e['form_id'],True]['animated']
            e['animation_exclusion']=None if e['normal_animated'] else 'single still frame; no animated action'
        (R/'data/forms/catalog.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f"Audited {len(rows)} assets; no animation: {sum(not x['animated'] for x in rows)}; idle static: {sum(not x['idle_animated'] for x in rows)}")
    for x in rows:
        if not x['animated']:print(json.dumps(x,ensure_ascii=False))
if __name__=='__main__':main()
