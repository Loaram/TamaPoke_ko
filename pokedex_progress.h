#pragma once
#include "dex.h"
#include "noart.h"

// Build-wide collection target: Explore can encounter every species with art,
// including species excluded from eggs. Missing installed regional packs do
// not change this target. National IDs and saved registration bits stay intact.
static inline uint16_t pokedexCollectibleCountIn(uint16_t lo, uint16_t hi) {
  if (lo < 1) lo = 1;
  if (hi > DEX_COUNT) hi = DEX_COUNT;
  if (lo > hi) return 0;
  uint16_t count = hi - lo + 1;
  for (int i = 0; i < NO_ART_COUNT; ++i)
    if (NO_ART[i] >= lo && NO_ART[i] <= hi) --count;
  return count;
}

static inline uint16_t pokedexCollectibleCount() {
  return pokedexCollectibleCountIn(1, DEX_COUNT);
}
