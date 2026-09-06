"""Pinned natural/form-event learnsets from the previous read-only audit.
The committed snapshot is sufficient for offline firmware generation.
"""
import argparse,csv,json,urllib.request
from pathlib import Path
from collections import defaultdict
from dex_moves import MOVES,LEGACY_MOVE_COUNT
from fetch_full_moves import TYPE_NAMES,CLASS_NAMES,AILMENTS,STATS,SELF_TARGETS
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--audit',type=Path,default=R/'build/form-move-audit/results.json');a=p.parse_args()
audit=json.loads(a.audit.read_text(encoding='utf8'));rev=audit['revision'];cache=a.audit.parent
def rows(name):
    path=cache/name
    if not path.exists():
        with urllib.request.urlopen(f'https://raw.githubusercontent.com/PokeAPI/pokeapi/{rev}/data/v2/csv/{name}',timeout=60) as f:path.write_bytes(f.read())
    with path.open(encoding='utf8',newline='') as f:return list(csv.DictReader(f))
def num(v):return int(v or 0)
src={r['identifier']:r for r in rows('moves.csv')}
meta={r['move_id']:r for r in rows('move_meta.csv')}
stats=defaultdict(list)
for r in rows('move_meta_stat_changes.csv'):stats[r['move_id']].append((num(r['stat_id']),num(r['change'])))
names={(r['move_id'],r['local_language_id']):r['name'] for r in rows('move_names.csv')}
machines=defaultdict(set);legacy={m[1] for m in MOVES[:LEGACY_MOVE_COUNT] if m[1]}
slugid={r['id']:s for s,r in src.items()}
for r in rows('pokemon_moves.csv'):
    if num(r['version_group_id'])<=27 and r['pokemon_move_method_id']=='4' and slugid[r['move_id']] in legacy:
        machines[num(r['pokemon_id'])].add(slugid[r['move_id']])
base=json.loads((R/'tools/full_move_data.json').read_text(encoding='utf8'))['learnsets']
forms=[]
for r in audit['rows']:
    event=r.get('form_events',[])
    if r['status']!='different' and not event and r['key'] not in ('zacian-crowned','zamazenta-crowned'):continue
    natural={x['slug']:max(1,x['level']) for x in r['form_natural']}
    tm=machines[r['pokemon_id']].copy()
    if r['status']=='same':natural={s:l for s,l in base[str(r['dex'])] if l};tm={s for s,l in base[str(r['dex'])] if not l}
    for x in event:natural[x['slug']]=1
    if r['key']=='zacian-crowned':natural['behemoth-blade']=1;natural.pop('iron-head',None);tm.discard('iron-head')
    if r['key']=='zamazenta-crowned':natural['behemoth-bash']=1;natural.pop('iron-head',None);tm.discard('iron-head')
    for s in tm:natural.setdefault(s,0)
    forms.append(dict(dex=r['dex'],form_id=r['form_id'],key=r['key'],version=r['version'],
                      learnset=sorted(natural.items(),key=lambda x:(x[1]==0,x[1],x[0]))))
needed={s for f in forms for s,l in f['learnset']}
existing=R/'data/forms/moves.json'
prior=json.loads(existing.read_text(encoding='utf8'))['moves'] if existing.exists() else []
known={m[1] for m in MOVES if m[1]}|{m['slug'] for m in prior};records=prior[:]
for slug in sorted(needed-known,key=lambda s:num(src[s]['id'])):
    r=src[slug];m=meta.get(r['id'],{});cat=CLASS_NAMES[num(r['damage_class_id'])]
    effect='none';param=0;stat='none';stages=0
    changes=[(STATS[s],c) for s,c in stats[r['id']] if s in STATS and c]
    if cat=='status' and changes and len({c for s,c in changes})==1:
        effect='stage';stat='+'.join(s for s,c in changes);stages=changes[0][1]
    elif cat!='status' and num(m.get('drain'))<0:effect='recoil';param=max(2,round(100/-num(m['drain'])))
    elif cat!='status' and num(r['priority']):effect='priority';param=num(r['priority'])
    elif cat!='status' and not r['accuracy']:effect='never_miss'
    if slug in ('freeze-shock','ice-burn'):effect='charge'
    if slug=='surging-strikes':effect='always_crit'
    ail=AILMENTS.get(num(m.get('meta_ailment_id')),'none');chance=num(m.get('ailment_chance'))
    if ail!='none' and cat=='status' and not chance:chance=100
    records.append(dict(source_id=num(r['id']),slug=slug,display=names.get((r['id'],'9'),slug.replace('-',' ')).upper(),
        ko=names.get((r['id'],'3'),slug),type=TYPE_NAMES[num(r['type_id'])],category=cat,
        power=0 if cat=='status' else (num(r['power']) or 60),accuracy=num(r['accuracy']),effect=effect,param=param,
        stat=stat,stages=stages,target='self' if num(r['target_id']) in SELF_TARGETS else 'foe',ailment=ail,ailment_chance=chance))
snapshot=dict(revision=rev,max_version_group=27,policy='Latest comparable natural learnsets; legacy TM subset per form; same-pool forms inherit base; form events explicit.',moves=records,forms=forms)
existing.write_text(json.dumps(snapshot,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
path=R/'localization/names-ko.json';ko=json.loads(path.read_text(encoding='utf8'))
for m in records:ko[m['display']]=m['ko']
path.write_text(json.dumps(ko,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('Forms',len(forms),'appended moves',len(records))
