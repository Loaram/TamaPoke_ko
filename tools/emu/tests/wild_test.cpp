// Explore release balance and save-compatible energy probes.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "wild.h"
#include "dex.h"
#include "pokedex_progress.h"
#include <cstdio>

uint32_t g_seed = 17;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

int main() {
  int shinyRolls = 0;
  for (uint32_t roll = 0; roll < 100; ++roll) shinyRolls += wildShinyForRoll(roll);
  ck(WILD_SHINY_SCALE == 100 && shinyRolls == 1,
     "wild shiny probability is exactly one of 100 equally likely rolls");
  bool exactRolls = true; int rejected = 0;
  for (uint32_t seed = 1; seed <= 10000; ++seed) {
    g_seed = seed; uint32_t roll;
    do { roll = (uint32_t)random(65536L); if (roll >= 65500) ++rejected; } while (roll >= 65500);
    bool expected = roll % 100 == 0; uint32_t expectedSeed = g_seed;
    g_seed = seed;
    if (wildShinyNow() != expected || g_seed != expectedSeed) exactRolls = false;
  }
  ck(exactRolls && rejected > 0, "live wild shiny lottery rejects the biased tail before applying 1/100");
  ck(wildTierForRoll(0) == R_LEGENDARIO &&
     wildTierForRoll(1) == R_RARO && wildTierForRoll(7) == R_RARO &&
     wildTierForRoll(8) == R_EVO && wildTierForRoll(29) == R_EVO &&
     wildTierForRoll(30) == R_COMUN && wildTierForRoll(99) == R_COMUN,
     "encounter tiers are 1/7/22/70 percent");

  ck(wildLevelMin(60, false) == 55 && wildLevelMax(60, false) == 65,
     "normal encounters stay within player level +/- 5");
  ck(wildLevelMin(60, true) == 1 && wildLevelMax(60, true) == 100,
     "random encounters span levels 1 through 100");

  uint8_t a = 8, d = 19, s = 20, h = 31;
  wildApplyShiny(true, a, d, s, h);
  ck(a == 20 && d == 20 && s == 20 && h == 31,
     "a shiny wild Pokemon has the promised IV floor");

  ck(wildCatchRateForDex(10) == 255 && wildCatchRateForDex(150) == 3,
     "canonical catch rates are loaded for common and legendary species");
  uint16_t shakes[4] = { 0, 0, 0, 0 };
  int caught = 0;
  for (uint16_t r = 0; r < 1000; r++)
    if (wildCaptureCheck(3, r, shakes)) caught++;
  ck(caught == 25, "catch-rate 3 is exactly 25/1000 (2.5 percent)");

  gRegionArt = 0xFFFF;
  bool reachable[DEX_COUNT + 1] = {};
  for (uint8_t region = 0; region < REGION_ALL; ++region)
    for (uint8_t tier = R_EVO; tier <= R_LEGENDARIO; ++tier)
      for (uint32_t roll = 0; roll < DEX_COUNT; ++roll) {
        int16_t dex = wildPickSpecies(region, tier, roll);
        if (dex >= 1 && dex <= DEX_COUNT) reachable[dex] = true;
      }
  uint16_t collectible = 0, regionalTotal = 0;
  bool availabilityOk = true;
  for (int16_t dex = 1; dex <= DEX_COUNT; ++dex) {
    if (reachable[dex]) ++collectible;
    if (reachable[dex] != speciesHasArt(dex) ||
        (reachable[dex] && !wildCatchRateForDex(dex))) availabilityOk = false;
  }
  for (uint8_t region = 0; region < REGION_ALL; ++region) {
    uint16_t regionalReachable = 0;
    const RegionInfo &rg = REGIONS[region];
    for (uint16_t dex = rg.lo; dex <= rg.hi; ++dex)
      if (reachable[dex]) ++regionalReachable;
    if (regionalReachable != pokedexCollectibleCountIn(rg.lo, rg.hi))
      availabilityOk = false;
    regionalTotal += regionalReachable;
  }
  ck(availabilityOk && collectible == pokedexCollectibleCount() &&
     regionalTotal == collectible,
     "national and regional collection targets match all catchable encounters");
  printf("Collectible species: %u/%u\n", collectible, DEX_COUNT);
  gRegionArt = 0;
  ck(pokedexCollectibleCount() == collectible,
     "collection target stays stable when regional packs are not installed");
  ck(pokedexCollectibleCountIn(0, 0) == 0 &&
     pokedexCollectibleCountIn(100, 99) == 0 &&
     pokedexCollectibleCountIn(0, 65535) == collectible,
     "collection count bounds exclude dex zero and out-of-range IDs");
  gRegionArt = 0xFFFF;
  bool pickOk = true;
  for (uint8_t tier = R_EVO; tier <= R_LEGENDARIO; tier++) {
    int16_t picked = wildPickSpecies(REGION_ALL, tier, 123456 + tier);
    if (picked < 1 || picked > DEX_COUNT || DEX_TBL[picked].rarity != tier)
      pickOk = false;
  }
  ck(pickOk, "wild selection respects the selected rarity tier");

  const int16_t formBacked[] = {668, 741, 870, 999, 1008};
  bool allFormBackedReachable = true;
  for (int16_t want : formBacked) {
    bool found = false;
    uint8_t region = regionOfDex(want);
    uint8_t tier = DEX_TBL[want].rarity;
    for (uint32_t roll = 0; roll < DEX_COUNT * 2UL && !found; roll++)
      found = wildPickSpecies(region, tier, roll) == want;
    if (!found) allFormBackedReachable = false;
  }
  ck(allFormBackedReachable,
     "all five alternate-form-backed species are reachable in Explore");

  nvs().clear();
  Pet p; p.begin(); p.dbgHatchAs(9, false);
  p.energy = 30;
  ck(p.spendEnergy(WILD_ENERGY_COST) && p.energy == 0,
     "an encounter spends 30 existing energy");
  ck(!p.spendEnergy(WILD_ENERGY_COST) && p.energy == 0,
     "an encounter cannot start without 30 energy");
  Pet reloaded; reloaded.begin();
  ck(reloaded.energy == 0, "the energy charge is persisted immediately");
  p.registerCaughtSpecies(150, true);
  ck(p.isShinyRegistered(150), "normal live pet correctly registers a wild shiny");
  p.shiny = true;
  p.registerCaughtSpecies(151, false);
  ck(p.isRegistered(151) && !p.isShinyRegistered(151),
     "shiny live pet does not mark a normal catch as shiny");
  p.registerCaughtSpecies(150, false);
  ck(p.isShinyRegistered(150), "later normal catch preserves an existing shiny entry");
  Pet dexReload; dexReload.begin();
  ck(dexReload.isShinyRegistered(150) && !dexReload.isShinyRegistered(151),
     "corrected caught-species flags survive reload");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
