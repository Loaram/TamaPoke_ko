// Pins the ko.1.0.2 growth and sleep balance against the real Pet code.
// Sleep recovery has separate live and offline paths, so both must agree.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include <cstdio>

uint32_t g_seed = 102;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
bool wasPressed = false;
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

static void hatch(Pet &p) {
  p.begin();
  p.dbgHatchAs(25, false);
  p.fullness = p.joy = p.hygiene = 100;
}

int main() {
  Pet growth;
  hatch(growth);
  growth.ageMinutes = 44;
  ck(growth.level() == 1, "44 minutes remains level 1");
  growth.ageMinutes = 45;
  ck(growth.level() == 2, "45 minutes reaches level 2");
  growth.ageMinutes = 99UL * 45;
  ck(growth.level() == 100, "3d 2h 15m reaches level 100");
  growth.ageMinutes = 14UL * 24 * 60;
  ck(growth.level() == 100, "level still caps at 100");
  ck(EVO_PENALTY_LEVELS == 32, "early-retire evolution debt remains one day");

  Pet live;
  hatch(live);
  live.energy = 20;
  live.sleeping = true;
  live.sleepAuto = SLEEP_PLAYER;
  live.dbgTick();
  ck(live.energy == 30, "live sleep restores 10 energy per minute");

  Pet offline;
  hatch(offline);
  offline.energy = 20;
  offline.sleeping = true;
  offline.sleepAuto = SLEEP_PLAYER;
  offline.dbgSetSeen(100000);
  offline.syncClock(100120);
  ck(offline.energy == 40, "offline sleep restores 10 energy per minute");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
