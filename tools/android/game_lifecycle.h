#pragma once
#include "pet.h"

// Shared with native regression tests: lifecycle callbacks must not overwrite
// imported NVS with the old in-memory Pet, or replay background time as care.
class AndroidGameLifecycle {
 public:
  void start() { ready = true; paused = false; closing = false; }
  bool canRun() const { return ready && !paused && !closing; }
  bool restartPending() const { return closing; }
  void requestRestart() { closing = true; }

  bool checkpoint(Pet &pet, uint32_t ms, uint32_t local, uint32_t utc) {
    if (!ready || closing) return false;
    if (!paused) pet.updateDeviceClock(ms, local, utc);
    pet.saveNow();
    return true;
  }
  void suspend(Pet &pet, uint32_t ms, uint32_t local, uint32_t utc) {
    if (!canRun()) return;
    checkpoint(pet, ms, local, utc);
    paused = true;
  }
  void resume(Pet &pet, uint32_t ms, uint32_t local, uint32_t utc) {
    if (!ready || closing || !paused) return;
    pet.updateDeviceClock(ms, local, utc, true);
    pet.saveNow();
    paused = false;
  }

 private:
  bool ready = false;
  bool paused = false;
  bool closing = false;
};
