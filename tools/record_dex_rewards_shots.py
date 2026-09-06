"""Convert the real runtime-test framebuffers into release evidence images."""
from pathlib import Path
from PIL import Image
R=Path(__file__).resolve().parents[1]
Q=R/'docs/qa/3.2.0';Q.mkdir(parents=True,exist_ok=True)
for name in ['egg-bonus-max','egg-bonus-dex-only','egg-bonus-max-en']:
    with Image.open(R/f'build/runtime-tests/{name}.ppm') as image:
        assert image.size==(466,466)
        image.save(Q/f'{name}.png')
        print(Q/f'{name}.png')
