#!/usr/bin/env python3
"""Static Korean translation, format, name and glyph coverage checks."""
from pathlib import Path
from collections import Counter
import json,re,subprocess,sys
R=Path(__file__).resolve().parents[1]
read=lambda p:(R/p).read_text(encoding='utf-8')
ui=json.loads(read('localization/ko.json')); names=json.loads(read('localization/names-ko.json'))
h=read('i18n.h');ids=re.findall(r'\bS_[A-Z0-9_]+\b',re.sub(r'//[^\n]*','',h.split('enum StrId')[1].split('STR_COUNT')[0]))
assert list(ui)==ids and all(ui.values()),'Every StrId needs exactly one Korean translation'
def specs(s):return Counter(re.findall(r'%(?:l)?[sudi]',s.replace('%%','')))
src=read('i18n.cpp'); table=src.split('static const char *const STRINGS')[1].split('};',1)[0]
rows=re.findall(r'\{(.*?)\}',table,re.S); assert len(rows)==7
english=re.findall(r'"((?:\\.|[^"\\])*)"',rows[1]); assert len(english)==len(ids)
for i,key in enumerate(ids):assert specs(english[i])==specs(ui[key]),f'printf mismatch: {key}'
korean=re.findall(r'"((?:\\.|[^"\\])*)"',rows[6])
assert korean==list(ui.values()),'Korean runtime row must match ko.json'
dex_names=re.findall(r'\{ "([^"]+)"',read('dex.h').split('DEX_TBL[DEX_COUNT + 1]')[1].split('\n};')[0])[1:]
move_names=re.findall(r'\{ "([^"]+)"',read('moves.h').split('MOVE_TBL')[1].split('\n};')[0])
missing=[n for n in dex_names+move_names if n not in ('-','?','NONE') and n not in names]
assert not missing,'Missing Korean display names: '+','.join(missing[:10])
subprocess.run([sys.executable,str(R/'tools/gen_korean_font.py'),'--check'],check=True)
subprocess.run([sys.executable,str(R/'tools/gen_korean_names.py'),'--check'],check=True)
assert 'LANG_KO' in h and 'LANG_DEFAULT LANG_KO' in h
print(f'PASS: {len(ui)} UI strings, {len(dex_names)} species, {len(move_names)} moves; formats and glyphs valid')
