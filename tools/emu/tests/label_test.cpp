#include "Arduino.h"
#include "Preferences.h"
// linked against the same core as every other suite, so it needs the same
// hardware stubs even though it only exercises the string table
uint32_t g_seed = 1;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
bool wasPressed = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
#include "i18n.h"
#include "korean_text.h"
#include "pokedex_progress.h"
#include <cstdio>
#include <cstring>
int main(){
  const StrId ids[] = { S_VIN, S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_VIT, S_STAT_WGT };
  const char *ln[] = {"ES","EN","FR","DE","IT","PT","KO"};
  int worst = 0;
  for (int l=0;l<LANG_COUNT;l++){ setLang((Lang)l);
    for (auto id : ids){ int w = 70 + uiTextWidth(T(id),2);
      if (w > 132) printf("COLLIDES %s \"%s\" ends at x=%d (bar starts 132)\n", ln[l], T(id), w);
      if (w > worst) worst = w; } }
  printf("widest label ends at x=%d\n", worst);
  int bad = 0;
  for (int l = 0; l < LANG_COUNT; ++l) {
    setLang((Lang)l);
    char label[84];
    snprintf(label, sizeof(label), T(S_POKEDEX_FMT),
             DEX_COUNT, DEX_COUNT, pokedexCollectibleCount());
    // Narrowest national tally is the menu button (284px, 10px each side).
    if (uiTextWidth(label, 2) > 264) { printf("National too wide: %s (%d)\n", label, uiTextWidth(label, 2)); ++bad; }
    for (uint8_t region = 0; region < REGION_ALL; ++region) {
      const RegionInfo &rg = REGIONS[region];
      unsigned total = rg.hi - rg.lo + 1;
      snprintf(label, sizeof(label), "%s %u/%u(%u)", localName(rg.name),
               total, total, pokedexCollectibleCountIn(rg.lo, rg.hi));
      // Circular display is only about 248px wide at the gallery title's y=36.
      if (uiTextWidth(label, 2) > 248) { printf("Gallery too wide: %s (%d)\n", label, uiTextWidth(label, 2)); ++bad; }
      snprintf(label, sizeof(label), "%u/%u(%u)", total, total,
               pokedexCollectibleCountIn(rg.lo, rg.hi));
      if (uiTextWidth(localName(rg.name), 3) + uiTextWidth(label, 2) + 12 > 282) {
        printf("Region row too wide: %s %s (%d)\n", localName(rg.name), label,
               uiTextWidth(localName(rg.name), 3) + uiTextWidth(label, 2) + 12);
        ++bad;
      }
    }
  }
  setLang(LANG_KO);
  char label[84], expected[84];
  snprintf(label, sizeof(label), T(S_POKEDEX_FMT), 1, DEX_COUNT, pokedexCollectibleCount());
  snprintf(expected, sizeof(expected), "도감 1/%u(%u)", DEX_COUNT, pokedexCollectibleCount());
  if (strcmp(label, expected)) ++bad;
  printf("%s: Pokedex counts fit national and regional labels in all languages\n",
         bad ? "FAIL" : "PASS");
  return bad ? 1 : 0;
}
