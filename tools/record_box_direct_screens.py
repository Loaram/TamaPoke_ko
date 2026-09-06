"""Convert synthetic box UI regression screenshots for release documentation."""
from pathlib import Path
from PIL import Image
R=Path(__file__).resolve().parents[1];Q=R/'docs/qa/3.0.1';Q.mkdir(parents=True,exist_ok=True)
for name in ['detail','confirm','swap']:
    Image.open(R/f'build/runtime-tests/box-direct-{name}.ppm').save(Q/f'box-direct-{name}.png')
print('Saved direct box menu, confirmation and explicit exchange screenshots')
