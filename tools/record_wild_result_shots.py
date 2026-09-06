"""Convert actual result-dialog framebuffers for visual regression evidence."""
from pathlib import Path
from PIL import Image
R=Path(__file__).resolve().parents[1]
Q=R/'docs/qa/3.3.0';Q.mkdir(parents=True,exist_ok=True)
for name in ['party','box','failed','failed-en','lost','storage']:
    with Image.open(R/f'build/runtime-tests/wild-result-{name}.ppm') as image:
        assert image.size==(466,466)
        image.save(Q/f'wild-result-{name}.png')
        print(Q/f'wild-result-{name}.png')
