"""Create review contact sheets from already rendered PDF pages."""
from pathlib import Path
import re
from PIL import Image,ImageDraw
R=Path(__file__).resolve().parents[1]
version=re.search(r'^#define FW_VERSION "([^"]+)"',(R/'TamaPoke.ino').read_text(encoding='utf8'),re.M)[1]
pages=sorted((R/f'build/{version}/guide-pages').glob('page-*.png'))
assert pages, 'Render every PDF page before creating contact sheets'
for start in range(0,len(pages),6):
    sheet=Image.new('RGB',(1056,1050),'#d8d8d8');draw=ImageDraw.Draw(sheet)
    for i,path in enumerate(pages[start:start+6]):
        im=Image.open(path).convert('RGB');im.thumbnail((342,493))
        x=(i%3)*352+5;y=(i//3)*525+23
        sheet.paste(im,(x,y));draw.text((x,y-18),path.stem,fill='black')
    out=R/f'build/{version}/guide-contact-{start//6+1}.png';sheet.save(out);print(out)
