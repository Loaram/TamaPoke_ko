#include "wild.h"
#include "catch_rates.h"
#include "dex.h"
#include "noart.h"
#include "pet.h"

static uint32_t isqrt32(uint32_t value) {
  uint32_t result = 0;
  uint32_t bit = 1UL << 30;
  while (bit > value) bit >>= 2;
  while (bit) {
    if (value >= result + bit) {
      value -= result + bit;
      result = (result >> 1) + bit;
    } else {
      result >>= 1;
    }
    bit >>= 2;
  }
  return result;
}

uint8_t wildTierForRoll(uint8_t roll) {
  roll %= 100;
  if (roll < 1) return R_LEGENDARIO;
  if (roll < 8) return R_RARO;
  if (roll < 30) return R_EVO;
  return R_COMUN;
}

int16_t wildPickSpecies(uint8_t region, uint8_t tier, uint32_t roll) {
  if (region >= REGION_COUNT || !regionAvailable(region)) return 0;
  uint16_t lo = REGIONS[region].lo;
  uint16_t hi = REGIONS[region].hi;
  if (hi > DEX_COUNT) hi = DEX_COUNT;

  // Two passes avoid a 1025-entry candidate array on the eventual ESP build.
  uint16_t count = 0;
  for (uint16_t dex = lo; dex <= hi; dex++) {
    if (DEX_TBL[dex].rarity != tier || !speciesHasArt((int16_t)dex)) continue;
    if (!regionAvailable(regionOfDex((int16_t)dex))) continue;
    count++;
  }
  if (!count) return 0;
  uint16_t wanted = (uint16_t)(roll % count);
  for (uint16_t dex = lo; dex <= hi; dex++) {
    if (DEX_TBL[dex].rarity != tier || !speciesHasArt((int16_t)dex)) continue;
    if (!regionAvailable(regionOfDex((int16_t)dex))) continue;
    if (!wanted--) return (int16_t)dex;
  }
  return 0;
}

uint8_t wildLevelMin(uint8_t playerLevel, bool hard) {
  if (hard) return 1;
  return playerLevel > WILD_LEVEL_SPREAD
             ? (uint8_t)(playerLevel - WILD_LEVEL_SPREAD) : 1;
}

uint8_t wildLevelMax(uint8_t playerLevel, bool hard) {
  if (hard) return MAX_LEVEL;
  uint16_t top = (uint16_t)playerLevel + WILD_LEVEL_SPREAD;
  return top > MAX_LEVEL ? MAX_LEVEL : (uint8_t)top;
}

bool wildShinyForRoll(uint32_t roll) {
  return (roll % WILD_SHINY_SCALE) == 0;
}

void wildApplyShiny(bool shiny, uint8_t &ivAtk, uint8_t &ivDef,
                    uint8_t &ivSpe, uint8_t &ivHp) {
  if (!shiny) return;
  uint8_t *ivs[] = { &ivAtk, &ivDef, &ivSpe, &ivHp };
  for (uint8_t *iv : ivs)
    if (*iv < WILD_SHINY_IV_FLOOR) *iv = WILD_SHINY_IV_FLOOR;
}

uint8_t wildCatchRateForDex(int16_t dex) {
  return catchRateForDex(dex);
}

uint32_t wildCaptureShakeThreshold(uint8_t catchRate) {
  if (!catchRate) return 0;
  // Emerald: a = catchRate * (3*maxHP - 2*HP) / (3*maxHP).
  // With a Pokeball (x1), HP fixed to exactly 10%, and no status this is 14/15.
  uint32_t odds = (uint32_t)catchRate * 14U / 15U;
  if (odds > 254) return 65536;
  if (!odds) return 0;
  uint32_t root = isqrt32(isqrt32(16711680UL / odds));
  return root ? 1048560UL / root : 65536;
}

bool wildCaptureCheck(uint8_t catchRate, uint16_t rareRoll,
                      const uint16_t shakeRolls[4]) {
  // Canonical rate 3 is about 0.8% here. The beta deliberately raises only
  // that lowest tier to exactly 25/1000 = 2.5% per defeated encounter.
  if (catchRate == 3) return (rareRoll % 1000U) < 25U;
  uint32_t threshold = wildCaptureShakeThreshold(catchRate);
  if (threshold >= 65536) return true;
  for (uint8_t i = 0; i < 4; i++)
    if (shakeRolls[i] >= threshold) return false;
  return true;
}

bool wildCaptureNow(uint8_t catchRate) {
  uint16_t shakes[4];
  for (uint8_t i = 0; i < 4; i++) shakes[i] = (uint16_t)random(65536);
  return wildCaptureCheck(catchRate, (uint16_t)random(1000), shakes);
}
