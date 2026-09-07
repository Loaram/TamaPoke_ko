"""Guide probability table, cross-checked against capture_dex runtime output."""
from math import isqrt

EXAMPLES = [(255, '캐터피', 10), (190, '피카츄', 25), (120, '단데기', 11),
            (45, '파이리', 4), (3, '뮤츠', 150)]

def probability(rate, registered):
    tenths = 10 + 2 * (min(registered, 982) // 100)
    if rate == 3:
        return (25 * tenths // 10) / 1000
    odds = (rate * tenths // 10) * 14 // 15
    if odds >= 255:
        return 1.0
    if not odds:
        return 0.0
    threshold = 1048560 // isqrt(isqrt(16711680 // odds))
    return (min(65536, threshold) / 65536) ** 4

def capture_rows():
    rows = [['등록 종수', '볼 배율'] + [f'{r}<br/>{name}' for r, name, _ in EXAMPLES]]
    for count in range(0, 901, 100):
        values = [probability(r, count) for r, _, _ in EXAMPLES]
        rows.append([f'{count}~{min(count+99,982)}', f'{1+count//100*0.2:.1f}배'] +
                    [('100%' if value == 1 else f'{100*value:.2f}%') for value in values])
    return rows
