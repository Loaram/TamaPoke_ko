#pragma once
#include "dex.h"
#include "noart.h"

// One release-wide unlock rule for eggs, exploration and Dex rewards.
// A visible base is locked too if any evolution path reaches missing art.
// Installed packs do not change this target; saved IDs/bits stay intact.
static inline bool speciesIsCollectible(int16_t dex) {
  return dex >= 1 && dex <= DEX_COUNT && speciesCanHatch(dex);
}
static inline uint16_t pokedexCollectibleCountIn(uint16_t lo, uint16_t hi) {
  if (lo < 1) lo = 1;
  if (hi > DEX_COUNT) hi = DEX_COUNT;
  if (lo > hi) return 0;
  uint16_t count = hi - lo + 1;
  for (int i = 0; i < NO_HATCH_COUNT; ++i)
    if (NO_HATCH[i] >= lo && NO_HATCH[i] <= hi) --count;
  return count;
}

static inline uint16_t pokedexCollectibleCount() {
  return pokedexCollectibleCountIn(1, DEX_COUNT);
}
