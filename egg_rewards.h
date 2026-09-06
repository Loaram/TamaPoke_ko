// Shared integer policy for new-egg odds, independent of save format/platform.
#pragma once
#include <stdint.h>

namespace EggRewards {
constexpr uint16_t SHINY_ROLL = 21600, SHINY_BASE = 450, SHINY_SOLO = 2160, SHINY_MAX = 3240;
constexpr uint16_t LEGEND_ROLL = 900, LEGEND_BASE = 27, LEGEND_SOLO = 126, LEGEND_MAX = 189;

// Bilinear interpolation between four corners:
// neither -> base; either goal alone -> solo; BOTH goals -> combined.
// Care is unchanged: day 0/1 = base, days 2..10 advance in nine equal steps.
// Dex progress uses exact species counts, not a rounded display percentage.
inline uint16_t weight(uint16_t base, uint16_t solo, uint16_t combined,
                       uint8_t careDays, uint16_t collected, uint16_t total) {
  uint32_t step = careDays > 10 ? 9 : careDays ? careDays - 1 : 0;
  uint32_t delta = solo - base;
  uint32_t overlap = 2u * solo - base - combined;
  uint32_t care = base + delta * step / 9;
  if (!total) return (uint16_t)care;
  uint32_t dex = (uint32_t)collected * 2;
  if (dex > total) dex = total; // 50% of collectible species completes this goal.
  return (uint16_t)(care + (uint64_t)(9 * delta - overlap * step) * dex / (9u * total));
}
}
