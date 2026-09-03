#!/usr/bin/env python3
"""Generate the display-only name lookup without modifying canonical game IDs."""
import argparse,json
from pathlib import Path
R=Path(__file__).resolve().parents[1]
data=json.loads((R/'localization/names-ko.json').read_text(encoding='utf-8'))
out='#pragma once\n// Generated from localization/names-ko.json.\nstruct KoreanName { const char *en; const char *ko; };\nstatic const KoreanName KOREAN_NAMES[] = {\n'+''.join('  { '+json.dumps(k)+', '+json.dumps(v,ensure_ascii=False)+' },\n' for k,v in sorted(data.items()))+'};\n'
p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');a=p.parse_args()
target=R/'korean_names.h'
if a.check:assert target.read_text(encoding='utf-8')==out,'Name table is stale'
else:target.write_text(out,encoding='utf-8',newline='\n')
print('PASS:',len(data),'Korean name mappings')
