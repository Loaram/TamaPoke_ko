// Integration proof for the release Explore screen and battle entry.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "wild.h"
#include <cstdio>

uint32_t g_seed = 0xB37A;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
extern Pet pet;
extern bool exploreOpen, battleOpen, btlWild;
extern uint8_t exploreRegion, exploreNotice, wildLevel, btlSquadN;
extern int16_t wildDex;
bool startWildBattle(bool hard);

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

static PartyMon level60(int16_t dex) {
  Pet t; t.dbgHatchAs(dex, false);
  t.ageMinutes = 59UL * MINUTES_PER_LEVEL;
  t.ivAtk = t.ivDef = t.ivSpe = t.ivHp = 25;
  t.relearnFromLevel();
  PartyMon m;
  m.dex = dex; m.level = 60;
  m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 25;
  for (uint8_t k = 0; k < MOVE_SLOTS; k++) m.moves[k] = t.moves[k];
  return m;
}

int main() {
  nvs().clear();
  setup();
  pet.dbgHatchAs(9, false);
  pet.ageMinutes = 59UL * MINUTES_PER_LEVEL;
  pet.energy = 100;
  static const int16_t team[5] = { 6, 25, 65, 94, 149 };
  for (uint8_t i = 0; i < 5; i++) party.replaceAt(i, level60(team[i]));

  exploreOpen = true;
  exploreRegion = REGION_ALL;
  ck(startWildBattle(false), "normal Explore starts a wild battle");
  ck(battleOpen && btlWild && wildDex >= 1 && wildDex <= DEX_COUNT,
     "the encounter is marked wild and has a valid species");
  ck(wildLevel >= 55 && wildLevel <= 65,
     "a level-60 team gets the requested normal level range");
  ck(btlSquadN == 6, "the live Pokemon plus five party members form a six-Pokemon squad");
  ck(pet.energy == 70, "starting that battle charges exactly 30 energy");

  battleOpen = false; btlWild = false;
  pet.energy = 29;
  ck(!startWildBattle(false) && exploreNotice == 1 && pet.energy == 29,
     "insufficient energy blocks the attempt without charging");

  party.replaceAt(5, level60(143));
  for (uint8_t i = 0; i < BOX_SLOTS; i++) party.box[i] = level60((int16_t)(1 + i));
  party.boxSave();
  pet.energy = 100;
  ck(!startWildBattle(false) && exploreNotice == 2 && pet.energy == 100,
     "a full party and box block the attempt before energy is spent");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
