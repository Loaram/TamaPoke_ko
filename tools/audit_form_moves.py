"""Read-only gameplay audit: download pinned reference CSVs and write findings here."""
import csv,json,sys,urllib.request
from pathlib import Path
from collections import defaultdict,Counter
from concurrent.futures import ThreadPoolExecutor
R=Path(__file__).resolve().parents[1];O=R/'build/form-move-audit';O.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(R/'tools'))
from gen_forms import supported,display_name
from dex_moves import MOVES
REV='d4f9a4af58ade123fbc0558f68b1c69daa97d9e4'
ROOT=f'https://raw.githubusercontent.com/PokeAPI/pokeapi/{REV}/data/v2/csv'
def rows(name):
    path=O/name
    existing=R/'build/forms/sources'/REV/name
    if existing.exists():path=existing
    if not path.exists():
        req=urllib.request.Request(ROOT+'/'+name,headers={'User-Agent':'TamaPoke form move audit'})
        with urllib.request.urlopen(req,timeout=60) as response:body=response.read()
        path.write_bytes(body)
    with path.open(encoding='utf8',newline='') as f:return list(csv.DictReader(f))
names=['pokemon_moves.csv','pokemon_forms.csv','moves.csv','move_names.csv','version_groups.csv']
with ThreadPoolExecutor(max_workers=5) as pool:data=dict(zip(names,pool.map(rows,names)))
forms={int(x['id']):x for x in data['pokemon_forms.csv']}
move={int(x['id']):x['identifier'] for x in data['moves.csv']}
ko={int(x['move_id']):x['name'] for x in data['move_names.csv'] if x['local_language_id']=='3'}
vg={int(x['id']):x for x in data['version_groups.csv'] if int(x['id'])<=27}
catalog=json.loads((R/'data/forms/catalog.json').read_text(encoding='utf8'))
entries=[e for e in catalog['entries'] if supported(e)]
needed={e['dex'] for e in entries}|{int(forms[e['form_id']]['pokemon_id']) for e in entries}
natural=defaultdict(dict);allmethods=defaultdict(list)
for x in data['pokemon_moves.csv']:
    p,v,m,method,lev=map(int,[x['pokemon_id'],x['version_group_id'],x['move_id'],x['pokemon_move_method_id'],x['level']])
    if p not in needed or v not in vg:continue
    allmethods[p,v].append((m,method,lev))
    if method==1:natural[p,v][m]=min(lev,natural[p,v].get(m,999))
def latest(versions):return max(versions,key=lambda v:int(vg[v]['order']))
known={x[1] for x in MOVES if x[1]}
def describe(ids):return [{'slug':move[m],'ko':ko.get(m,move[m]),'in_move_table':move[m] in known} for m in sorted(ids)]
report=[]
for e in entries:
    p=int(forms[e['form_id']]['pokemon_id']);dex=e['dex']
    basevs={v for pp,v in natural if pp==dex};formvs={v for pp,v in natural if pp==p}
    common=basevs&formvs
    row={k:e[k] for k in ['dex','form_id','key','classification','species_ko']}
    row['name_ko']=display_name(e);row['pokemon_id']=p
    if not common:
        row['status']='no comparable natural learnset in selected editions';report.append(row);continue
    v=latest(common);base=natural[dex,v];alt=natural[p,v]
    row.update(version=vg[v]['identifier'],version_id=v,
      added=describe(alt.keys()-base.keys()),removed=describe(base.keys()-alt.keys()),
      level_changes=[{'slug':move[m],'ko':ko.get(m,move[m]),'base':base[m],'form':alt[m]} for m in sorted(base.keys()&alt.keys()) if base[m]!=alt[m]],
      form_events=describe({m for m,method,lev in allmethods[p,v] if method==10}),
      form_natural=[{'slug':move[m],'ko':ko.get(m,move[m]),'level':lev,'in_move_table':move[m] in known} for m,lev in sorted(alt.items())])
    row['status']='different' if row['added'] or row['removed'] or row['level_changes'] else 'same'
    report.append(row)
(O/'results.json').write_text(json.dumps({'revision':REV,'max_version_group':27,'rows':report},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('SUMMARY',dict(Counter(r['status'] for r in report)))
print('BY CLASS',dict(Counter(r['classification'] for r in report if r['status']=='different')))
for r in report:
    if r['status']=='different':
        print(r['key'],r['version'],'+',','.join(x['slug'] for x in r['added']),'-',','.join(x['slug'] for x in r['removed']),'levels',len(r['level_changes']))
missing={x['slug']:x['ko'] for r in report for x in r.get('form_natural',[]) if not x['in_move_table']}
print('ABSENT FROM TABLE',json.dumps(missing,ensure_ascii=False))
