"""Create review contact sheets from already rendered PDF pages."""
from pathlib import Path
from PIL import Image,ImageDraw
R=Path(__file__).resolve().parents[1]
pages=sorted((R/'build/3.0.0/guide-pages').glob('page-*.png'))
assert len(pages)==22
for start in range(0,len(pages),6):
    sheet=Image.new('RGB',(1056,1050),'#d8d8d8');draw=ImageDraw.Draw(sheet)
    for i,path in enumerate(pages[start:start+6]):
        im=Image.open(path).convert('RGB');im.thumbnail((342,493))
        x=(i%3)*352+5;y=(i//3)*525+23
        sheet.paste(im,(x,y));draw.text((x,y-18),path.stem,fill='black')
    out=R/f'build/3.0.0/guide-contact-{start//6+1}.png';sheet.save(out);print(out)
