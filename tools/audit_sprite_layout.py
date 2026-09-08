#!/usr/bin/env python3
"""Offline exhaustive asset geometry audit and contact sheets for visual QA.

Uses the same integer fit policy as sprite_layout.h. The C++ runtime test
separately exercises the real renderer and all three loaders share bounds code.
"""
import json,struct
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/size-cap-patch/audit';OUT.mkdir(parents=True,exist_ok=True)
SIZES=json.loads((ROOT/'data/sprite_sizes.json').read_text(encoding='utf-8'))
try:font=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',11)
except OSError:font=ImageFont.load_default()

def load(path):
    blob=path.read_bytes();assert blob[:4]==b'TPK2'
    n,pc=struct.unpack_from('<BH',blob,4);at=7+pc*2
    palette=struct.unpack_from('<'+'H'*pc,blob,7)
    rgb=[((c>>11)*255//31,((c>>5)&63)*255//63,(c&31)*255//31,255) for c in palette]
    actions=[]
    for _ in range(n):
        act,w,h,nf=struct.unpack_from('<4B',blob,at);at+=4
        ms=struct.unpack_from('<'+'H'*nf,blob,at);at+=nf*2
        assert all(ms),path
        frames=[];union=None
        for f in range(nf):
            pixels=blob[at:at+w*h];at+=w*h
            assert len(pixels)==w*h and all(p==255 or p<pc for p in pixels),path
            im=Image.new('RGBA',(w,h));im.putdata([rgb[p] if p!=255 else (0,0,0,0) for p in pixels])
            b=im.getbbox()
            if b:union=b if union is None else (min(union[0],b[0]),min(union[1],b[1]),max(union[2],b[2]),max(union[3],b[3]))
            frames.append(im)
        actions.append((act,union,frames))
    assert at==len(blob),path
    return actions

def body(dm):return 80 if dm<=3 else 92 if dm<=4 else 104 if dm<=7 else 120 if dm<=12 else 144
def sheet(entries,name):
    for p in range(0,len(entries),96):
        page=entries[p:p+96];im=Image.new('RGB',(1440,976),'#eee7df');draw=ImageDraw.Draw(im)
        for i,(row,sprite) in enumerate(page):
            x=(i%12)*120;y=(i//12)*122
            draw.rectangle((x+1,y+1,x+118,y+120),fill='#faf3e9')
            # Half-size home body; wide sprites stay within the cell.
            sprite=sprite.resize((max(1,row['display_w']//2),max(1,row['display_h']//2)),Image.Resampling.NEAREST)
            im.paste(sprite,(x+60-sprite.width//2,y+94-sprite.height),sprite)
            draw.text((x+5,y+3),str(row['dex'])+((' f'+str(row['form'])) if row['form'] else ''),fill='#342f2c',font=font)
            draw.text((x+5,y+103),f"{row['display_w']}x{row['display_h']} / {row['height_dm']/10:g}m",fill='#342f2c',font=font)
        im.save(OUT/f'{name}-{p//96+1:02}.png')

rows=[];normal=[];form_images=[];unusual=[];missing=[];total_frames=0;layout_checks=0
targets=[(dex,0,SIZES['height_dm'][dex]) for dex in range(1,1026)]
targets += [(f['dex'],f['form'],f['height_dm']) for f in SIZES['forms']]
for dex,form,dm in targets:
    for shiny in [False,True]:
        suffix=f'f{form:05}' if form else ''
        path=ROOT/'tools/sdcard/mons'/f'p{"s" if shiny else ""}{dex:03}{suffix}.bin'
        fallback=False
        if not path.exists() and shiny:
            path=path.with_name(f'p{dex:03}{suffix}.bin');fallback=True
        if not path.exists():missing.append({'dex':dex,'form':form,'shiny':shiny});continue
        acts=load(path);total_frames+=sum(len(a[2]) for a in acts)
        idle=next(a for a in acts if a[0]==0);_,b,frames=idle
        assert b,(dex,form,shiny)
        w,h=b[2]-b[0],b[3]-b[1];target=92 if dex==870 and not form else body(dm)
        for _,ab,_ in acts:
            if not ab:continue
            aw,ah=ab[2]-ab[0],ab[3]-ab[1]
            for lane in [3,4,5,6]:
                lane_w,lane_h={3:(120,84),4:(156,112),5:(240,168),6:(270,192)}[lane]
                lw=300 if lane>=5 else lane_w
                lh=204 if lane>=6 else 192 if lane>=5 else lane_h+20
                scale=min(4*65536,(lane_w*target//168)*65536//w,(lane_h*target//168)*65536//h,lw*65536//aw,lh*65536//ah)
                sw=max(1,(aw*scale+32768)//65536);sh=max(1,(ah*scale+32768)//65536)
                assert 0<sw<=lw and 0<sh<=lh,(dex,form,lane,sw,sh)
                assert sw<=aw*4 and sh<=ah*4,(dex,form,lane,'4x cap')
                if lane==5:
                    assert 304-sh>=112
                    dy=max(abs(304-sh-233),71);half=231
                    while half*half+dy*dy>231*231:half-=1
                    for desired in [150,233,326]:
                        cx=max(233-half+sw//2+1,min(desired,233+half-(sw+1)//2-1))
                        assert all((x-233)**2+(y-233)**2<=231**2 for x in [cx-sw//2,cx+(sw+1)//2] for y in [304-sh,304])
                layout_checks+=1
        scale=min(4*65536,(240*target//168)*65536//w,target*65536//h)
        dw=(w*scale+32768)//65536;dh=(h*scale+32768)//65536
        first=frames[0].getbbox();assert first,(dex,form,shiny)
        ratios=[((f.getbbox()[2]-f.getbbox()[0])*(f.getbbox()[3]-f.getbbox()[1]))/(w*h) for f in frames if f.getbbox()]
        row={'dex':dex,'form':form,'shiny':shiny,'fallback':fallback,'height_dm':dm,
             'display_w':dw,'display_h':dh,'idle_union':b,'min_frame_area_ratio':round(min(ratios),3),
             'frames':sum(len(a[2]) for a in acts),'file':path.name}
        rows.append(row)
        crop=frames[0].crop(b)
        if not shiny:
            (form_images if form else normal).append((row,crop))
            if min(ratios)<.55 or max(w/h,h/w)>2.7:unusual.append((row,crop))
assert len(rows)==2316 and len(missing)==86,(len(rows),len(missing))
sheet(normal,'species');sheet(form_images,'forms');sheet(unusual,'review-outliers')
summary={'loaded_variants':len(rows),'missing_base_variants':len(missing),'animation_frames':total_frames,
         'action_lane_checks':layout_checks,'review_outliers':len(unusual),'rows':rows,'missing':missing}
(OUT/'audit.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:v for k,v in summary.items() if k not in ['rows','missing']}))
print('Outliers:',[(r['dex'],r['form'],r['min_frame_area_ratio']) for r,_ in unusual])
