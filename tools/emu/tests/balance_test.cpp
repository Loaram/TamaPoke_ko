// Pins the current growth, farewell and sleep balance against the real Pet code.
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
  growth.ageMinutes = MINUTES_PER_LEVEL - 1;
  ck(growth.level() == 1, "19 minutes remains level 1");
  growth.ageMinutes = MINUTES_PER_LEVEL;
  ck(growth.level() == 2, "20 minutes reaches level 2");
  growth.ageMinutes = 99UL * MINUTES_PER_LEVEL;
  ck(growth.level() == 100, "1d 9h reaches level 100");
  growth.ageMinutes = 14UL * 24 * 60;
  ck(growth.level() == 100, "level still caps at 100");
  Pet farewell;
  farewell.begin();
  farewell.dbgHatchAs(6, false);      // Charizard is a final form.
  farewell.ageMinutes = FAREWELL_AGE_MIN - 1;
  ck(!farewell.canFarewellNow(), "farewell is unavailable one minute before one day");
  farewell.ageMinutes = FAREWELL_AGE_MIN;
  ck(farewell.level() == 73, "one day reaches level 73 at 20 minutes per level");
  ck(farewell.canFarewellNow(), "farewell is offered at one day");

  Pet live;
  hatch(live);
  live.energy = 20;
  live.sleeping = true;
  live.sleepAuto = SLEEP_PLAYER;
  live.dbgTick();
  ck(live.energy == 35, "live sleep restores 15 energy per minute");

  Pet offline;
  hatch(offline);
  offline.energy = 20;
  offline.sleeping = true;
  offline.sleepAuto = SLEEP_PLAYER;
  offline.dbgSetSeen(100000);
  offline.syncClock(100120);
  ck(offline.energy == 50, "offline sleep restores 15 energy per minute");

  Pet deviceClock;
  hatch(deviceClock);
  deviceClock.ageMinutes = MINUTES_PER_LEVEL - 1;
  deviceClock.setClock(100000);
  deviceClock.updateDeviceClock(0, 100000, 100000);
  deviceClock.updateDeviceClock(59000, 100059, 100059);
  ck(deviceClock.level() == 1, "device clock does not tick before 60 elapsed seconds");
  deviceClock.updateDeviceClock(60000, 100060, 100060);
  ck(deviceClock.level() == 2, "device clock advances growth at the minute boundary");
  uint32_t ageBeforeBackwards = deviceClock.ageMinutes;
  deviceClock.updateDeviceClock(61000, 99000, 99000);
  ck(deviceClock.ageMinutes == ageBeforeBackwards,
     "backward device clock correction cannot trigger a huge unsigned catch-up");
  deviceClock.updateDeviceClock(121000, 99060, 99060);
  ck(deviceClock.ageMinutes == ageBeforeBackwards + 1,
     "growth continues normally after a backward clock correction");
  uint32_t ageBeforeTimezone = deviceClock.ageMinutes;
  deviceClock.updateDeviceClock(122000, 99060 + 9 * 3600, 99061);
  ck(deviceClock.ageMinutes == ageBeforeTimezone,
     "timezone display changes do not become extra growth time");
  ck(deviceClock.lastSeenEpoch == 99060 + 9 * 3600,
     "clock display follows the new device-local timezone");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
