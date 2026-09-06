#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "tools/android/game_lifecycle.h"
#include "nvs_file.h"
#include <cstdio>

uint32_t g_seed = 102;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
uint32_t millis() { return 0; }
void FakeESP::restart() {}
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++bad;
}
static void hatch(Pet &p) {
  p.begin(); p.dbgHatchAs(25, false);
  p.fullness = p.joy = p.energy = p.hygiene = 100;
}
int main() {
  const uint32_t epoch = 1700000000;
  nvs().clear();
  Pet sender; sender.begin(); sender.dbgHatchAs(6, true);
  sender.ageMinutes = 59 * MINUTES_PER_LEVEL;
  sender.badges = 7; sender.saveNow();
  party.begin(); PartyMon member; member.dex = 150; member.level = 60;
  party.replaceAt(0, member); party.box[0] = member; party.boxSave();
  uint8_t blob[SAVE_MAX_BYTES]; size_t size = saveExport(blob, sizeof(blob));
  ck(size > 0, "sender snapshot exported");
  Pet receiver; hatch(receiver);
  AndroidGameLifecycle app; app.start();
  ck(saveImport(blob, size), "received save imported");
  const NvsStore imported = nvs();
  app.requestRestart();
  ck(app.restartPending(), "restart flag stops the applying frame before autosave");
  app.suspend(receiver, 0, epoch, epoch);
  app.resume(receiver, 0, epoch + 120, epoch + 120);
  ck(!app.checkpoint(receiver, 0, epoch + 180, epoch + 180) && !app.canRun(),
     "restart blocks pause/stop/resume/exit checkpoints and game ticks");
  ck(nvs() == imported, "every imported key survives the Android shutdown sequence");
  Pet reopened; reopened.begin(); party.begin();
  ck(reopened.level() == 60 && reopened.shiny && reopened.badges == 7 &&
     reopened.isRegistered(6) && party.slots[0].dex == 150 && party.box[0].dex == 150,
     "reopen restores received creature, dex, badges, party and box together");

  nvs().clear(); Pet resumed; hatch(resumed);
  resumed.updateDeviceClock(0, epoch, epoch);
  app.start(); app.suspend(resumed, 0, epoch, epoch);
  app.suspend(resumed, 0, epoch + 300, epoch + 300);
  app.checkpoint(resumed, 0, epoch + 600, epoch + 600);
  ck(resumed.fullness == 100 && resumed.ageMinutes == 0,
     "repeated background callbacks never apply active-care ticks");
  app.resume(resumed, 0, epoch + 7200, epoch + 7200);
  ck(resumed.fullness == 15 && resumed.hygiene == 15 && resumed.careMistakes == 0,
     "two hours in background uses offline floors and adds no neglect");
  ck(resumed.ageMinutes == 120, "background growth advances exactly once");
  app.resume(resumed, 0, epoch + 7200, epoch + 7200);
  ck(resumed.ageMinutes == 120, "duplicate resume/focus events cannot replay time");
  nvs().clear(); Pet cold; hatch(cold); cold.setClock(epoch); cold.syncClock(epoch + 7200);
  ck(cold.fullness == resumed.fullness && cold.hygiene == resumed.hygiene &&
     cold.ageMinutes == resumed.ageMinutes && cold.careMistakes == resumed.careMistakes,
     "cold launch and warm resume agree on offline growth and care");

  nvs().clear(); Pet companion; companion.begin();
  member.dex = 25; member.level = 60;
  companion.reviveFrom(member); companion.setClock(epoch); companion.syncClock(epoch + 3600);
  ck(companion.level() == 63, "active companion gains three levels after an offline hour");
  companion.updateDeviceClock(0, epoch + 3600, epoch + 3600);
  app.start(); app.suspend(companion, 0, epoch + 3600, epoch + 3600);
  app.resume(companion, 0, epoch + 7200, epoch + 7200);
  ck(companion.level() == 66, "active companion gains three more levels after warm resume");
  app.suspend(companion, 0, epoch + 7200, epoch + 7200);
  app.resume(companion, 0, epoch, epoch);
  ck(companion.level() == 66, "backward clock correction never adds offline growth");

  // Exercise the actual APK/PC disk codec, not just the Preferences map.
  const char *disk = "android-lifecycle-sleep.nvs";
  remove(disk);
  NvsFile storage; storage.load(disk, nvs());
  Pet sleeper; hatch(sleeper); party.begin();
  member.dex = 150; member.level = 80;
  for (auto &slot : party.box) slot = member;
  party.boxSave();
  sleeper.energy = 20; sleeper.updateDeviceClock(0, epoch, epoch);
  sleeper.toggleLight();
  Preferences settings; settings.putBool("snd", false); settings.putUChar("vol", 3);
  app.start(); app.suspend(sleeper, 0, epoch, epoch);
  ck(nvs()["rosterF"].size() == 22116 && storage.save(disk,nvs(),true),
     "sleeping app checkpoint writes full 300-box disk snapshot");
  const auto saved = nvs(); nvs().clear(); NvsFile restarted;
  ck(restarted.load(disk,nvs()) && nvs()==saved &&
     !settings.getBool("snd",true) && settings.getUChar("vol",7)==3,
     "process restart restores disabled sound and chosen volume after large roster");
  Pet sleepingCold; sleepingCold.begin(); party.begin();
  ck(sleepingCold.sleeping && sleepingCold.sleepAuto==SLEEP_PLAYER &&
     party.box[0].dex==150 && party.box[299].dex==150,
     "cold startup restores manual sleep and first/last box residents");
  sleepingCold.updateDeviceClock(0,epoch+120,epoch+120,true);
  ck(sleepingCold.sleeping && sleepingCold.energy==36,
     "sleep survives offline minutes and restores exactly 8 energy per minute");
  app.start(); app.suspend(sleepingCold,0,epoch+120,epoch+120);
  app.resume(sleepingCold,0,epoch+240,epoch+240);
  app.resume(sleepingCold,0,epoch+240,epoch+240);
  ck(sleepingCold.sleeping && sleepingCold.energy==52,
     "warm resume preserves sleep without duplicate energy recovery");
  sleepingCold.toggleLight(); sleepingCold.saveNow();
  ck(restarted.save(disk,nvs(),true), "manual wake saved");
  nvs().clear(); storage.load(disk,nvs()); Pet awake; awake.begin();
  ck(!awake.sleeping, "manual wake also persists across process restart");
  remove(disk);
  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
