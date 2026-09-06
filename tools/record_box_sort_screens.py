"""Convert synthetic box sort regression screenshots for release documentation."""
from pathlib import Path
from PIL import Image
R=Path(__file__).resolve().parents[1];Q=R/'docs/qa/4.0.0';Q.mkdir(parents=True,exist_ok=True)
for name in ['menu','confirm','result']:
    Image.open(R/f'build/runtime-tests/box-sort-{name}.ppm').save(Q/f'box-sort-{name}.png')
print('Saved box sort menu, confirmation and sorted box screenshots')
