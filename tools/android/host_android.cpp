// Android implementations of the hardware-facing modules used by the desktop
// emulator. Sprite bytes come from the indexed TPAK assets in android_main.cpp.
#include "Arduino.h"
#include "sdmon.h"
#include "rtcbat.h"
#include "linknow.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>

uint8_t *androidLoadPackedFile(const char *path, uint32_t *size);

bool sdReady = true;
bool sdDirty = false;
bool sdArtDirty = false;
SdThumbs thumbs;

void emuSetSpriteDir(const char *) {}
void sdScanRegionArt(bool) {}

static uint8_t *slurp(const std::string &path, uint32_t *size) {
  return androidLoadPackedFile(path.c_str(), size);
}

bool PmdMon::load(int16_t dexNum, bool shiny) {
  if (dexNum < 1 || dexNum > DEX_COUNT) return false;
  unload();
  char file[24];
  snprintf(file, sizeof(file), "/p%s%03u.bin", shiny ? "s" : "", (unsigned)dexNum);
  uint32_t size = 0;
  blob = slurp(file, &size);
  if (!blob && shiny) {
    snprintf(file, sizeof(file), "/p%03u.bin", (unsigned)dexNum);
    blob = slurp(file, &size);
  }
  if (!blob) return false;
  if (size < 7 || memcmp(blob, "TPK2", 4) != 0) { unload(); return false; }

  uint8_t nActs = blob[4];
  memcpy(&palCount, blob + 5, 2);
  if (palCount > 256 || (uint32_t)7 + palCount * 2 > size) { unload(); return false; }
  memcpy(pal, blob + 7, palCount * 2);

  const uint8_t *q = blob + 7 + palCount * 2, *end = blob + size;
  for (uint8_t i = 0; i < nActs && q + 4 <= end; i++) {
    uint8_t id = q[0], w = q[1], h = q[2], nf = q[3];
    q += 4;
    if (id >= PMD_NACTS || nf > 24) { unload(); return false; }
    uint32_t bytes = (uint32_t)nf * 2 + (uint32_t)w * h * nf;
    if (w == 0 || h == 0 || nf == 0 || q + bytes > end) { unload(); return false; }
    PmdAct &a = acts[id];
    a.w = w; a.h = h; a.frames = nf;
    for (uint8_t k = 0; k < nf; k++) { a.ms[k] = q[0] | (q[1] << 8); q += 2; }
    a.data = q;
    q += (uint32_t)w * h * nf;
    uint8_t base = 1;
    for (uint8_t f = 0; f < nf; f++) {
      const uint8_t *fr = a.data + (uint32_t)f * w * h;
      for (int r = h - 1; r >= 0; r--) {
        bool any = false;
        for (int c = 0; c < w && !any; c++) if (fr[r * w + c] != 0xFF) any = true;
        if (any) { if (r + 1 > base) base = r + 1; break; }
      }
    }
    a.base = base;
  }
  dex = dexNum;
  loaded = true;
  return true;
}

void PmdMon::unload() {
  if (blob) { free(blob); blob = nullptr; }
  for (auto &a : acts) { a.w = a.h = a.frames = a.base = 0; a.data = nullptr; }
  loaded = false;
}

bool SdThumbs::load() {
  uint32_t size = 0;
  data = slurp("/thumbs.bin", &size);
  if (!data || size < 6 || memcmp(data, "TPTH", 4) != 0) {
    if (data) free(data);
    data = nullptr;
    return false;
  }
  memcpy(&count, data + 4, 2);
  loaded = count > 0 && size >= 6u + (uint32_t)count * 4u;
  if (!loaded) { free(data); data = nullptr; }
  return loaded;
}

const uint8_t *SdThumbs::get(int16_t dex) const {
  if (!loaded || dex < 1 || dex > count) return nullptr;
  uint32_t off;
  memcpy(&off, data + 6 + 4 * (dex - 1), 4);
  return data + off;
}

bool SdMon::load(int16_t, bool) { return false; }
void SdMon::unload() { if (data) { free(data); data = nullptr; } loaded = false; }
bool sdBegin() { return true; }
bool sdSerialCommand(const String &) { return false; }

static int64_t gRtcOffset = 0;
static uint32_t systemEpoch() {
  using namespace std::chrono;
  return (uint32_t)duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}
bool rtcBegin() { return true; }
uint32_t rtcEpoch() { return (uint32_t)((int64_t)systemEpoch() + gRtcOffset); }
void rtcSetEpoch(uint32_t e) { gRtcOffset = (int64_t)e - systemEpoch(); }
bool batBegin() { return true; }
void pmuEnablePanel() {}
int batPercent() { return 87; }
bool batCharging() { return false; }
bool usbPresent() { return true; }
void pwrSetup() {}
bool pwrShortPressed() { return false; }

struct Link;
bool linkNowBegin(Link *) { return false; }
void linkNowEnd() {}
bool linkNowUp() { return false; }
void linkNowPoll() {}
static LinkNowStats gNoStats;
const LinkNowStats &linkNowStats() { return gNoStats; }

static int gResetReason = ESP_RST_POWERON;
esp_reset_reason_t emuResetReason() { return gResetReason; }
void emuSetResetReason(int r) { gResetReason = r; }
