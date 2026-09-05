#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/window.h>
#include <android_native_app_glue.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "korean_text.h"
#include "pet.h"
#include "game_lifecycle.h"

#define LOG_TAG "TamaPoke"
#define PANEL 466

uint32_t g_seed = 0xC0FFEE;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;

static android_app *gApp = nullptr;
static ANativeWindow *gWindow = nullptr;
static AAssetManager *gAssets = nullptr;
static bool gActive = false;
static std::string gSavePath;
static NvsStore gLastSaved;
static uint32_t gLastSaveCheck = 0;
static AndroidGameLifecycle gGameLifecycle;
extern Pet pet;
uint32_t rtcEpoch();
uint32_t androidUtcEpoch();

struct PackedFile {
  std::string pack;
  off_t offset = 0;
  uint32_t size = 0;
};
static std::unordered_map<std::string, PackedFile> gPackedFiles;

void setup();
void loop();
extern KoreanCanvas *gfx;
void androidAudioSetActive(bool active);

static bool callActivityBoolean(const char *method) {
  if (!gApp || !gApp->activity || !gApp->activity->vm || !gApp->activity->clazz)
    return false;
  JNIEnv *env = nullptr;
  bool attached = false;
  if (gApp->activity->vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    if (gApp->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
    attached = true;
  }
  jclass activityClass = env->GetObjectClass(gApp->activity->clazz);
  jmethodID call = activityClass
      ? env->GetMethodID(activityClass, method, "()Z")
      : nullptr;
  bool result = call && env->CallBooleanMethod(gApp->activity->clazz, call) == JNI_TRUE;
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    result = false;
  }
  if (activityClass) env->DeleteLocalRef(activityClass);
  if (attached) gApp->activity->vm->DetachCurrentThread();
  return result;
}

static int callActivityInt(const char *method, int fallback) {
  if (!gApp || !gApp->activity || !gApp->activity->vm || !gApp->activity->clazz)
    return fallback;
  JNIEnv *env = nullptr;
  bool attached = false;
  if (gApp->activity->vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    if (gApp->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return fallback;
    attached = true;
  }
  jclass activityClass = env->GetObjectClass(gApp->activity->clazz);
  jmethodID call = activityClass
      ? env->GetMethodID(activityClass, method, "()I")
      : nullptr;
  int result = call ? env->CallIntMethod(gApp->activity->clazz, call) : fallback;
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    result = fallback;
  }
  if (activityClass) env->DeleteLocalRef(activityClass);
  if (attached) gApp->activity->vm->DetachCurrentThread();
  return result;
}

bool androidEnsureLocalNetworkPermission() {
  return callActivityBoolean("ensureLocalNetworkPermission");
}

bool androidHasLocalNetworkPermission() {
  return callActivityBoolean("hasLocalNetworkPermission");
}

int androidBatteryPercent() {
  return callActivityInt("getBatteryPercent", -1);
}

bool androidBatteryCharging() {
  return callActivityBoolean("isBatteryCharging");
}

int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

static bool readAll(AAsset *asset, void *out, size_t size) {
  uint8_t *p = static_cast<uint8_t *>(out);
  while (size) {
    int got = AAsset_read(asset, p, size);
    if (got <= 0) return false;
    p += got;
    size -= (size_t)got;
  }
  return true;
}

static uint32_t little32(const uint8_t b[4]) {
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static bool indexPack(const char *packName) {
  AAsset *asset = AAssetManager_open(gAssets, packName, AASSET_MODE_RANDOM);
  if (!asset) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Missing Android asset: %s", packName);
    return false;
  }
  char magic[4];
  uint8_t countBytes[2];
  if (!readAll(asset, magic, 4) || memcmp(magic, "TPAK", 4) != 0 ||
      !readAll(asset, countBytes, 2)) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Invalid sprite pack header: %s", packName);
    AAsset_close(asset);
    return false;
  }
  uint16_t count = (uint16_t)(countBytes[0] | (countBytes[1] << 8));
  if (!count || count > 4096) {
    AAsset_close(asset);
    return false;
  }
  struct Pending { std::string name; uint32_t size; };
  std::vector<Pending> pending;
  pending.reserve(count);
  uint64_t dataBytes = 0;
  for (uint16_t i = 0; i < count; ++i) {
    uint8_t nameLen = 0, sizeBytes[4];
    if (!readAll(asset, &nameLen, 1) || !nameLen || nameLen > 100) {
      AAsset_close(asset); return false;
    }
    std::string name(nameLen, '\0');
    if (!readAll(asset, name.data(), nameLen) || !readAll(asset, sizeBytes, 4) ||
        name.rfind("mons/", 0) != 0 || name.find("..") != std::string::npos) {
      __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Invalid sprite pack directory: %s", packName);
      AAsset_close(asset); return false;
    }
    uint32_t size = little32(sizeBytes);
    if (!size || size > 16u * 1024u * 1024u) { AAsset_close(asset); return false; }
    pending.push_back({name, size});
    dataBytes += size;
  }
  off_t dataAt = AAsset_seek(asset, 0, SEEK_CUR);
  int64_t length = AAsset_getLength64(asset);
  if (dataAt < 0 || (uint64_t)dataAt + dataBytes != (uint64_t)length) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Unexpected sprite pack length: %s", packName);
    AAsset_close(asset); return false;
  }
  off_t offset = dataAt;
  for (const Pending &p : pending) {
    if (!gPackedFiles.count(p.name)) gPackedFiles.emplace(p.name, PackedFile{packName, offset, p.size});
    offset += p.size;
  }
  AAsset_close(asset);
  return true;
}

uint8_t *androidLoadPackedFile(const char *path, uint32_t *size) {
  if (size) *size = 0;
  if (!path || !gAssets) return nullptr;
  std::string name(path);
  while (!name.empty() && name.front() == '/') name.erase(name.begin());
  if (name.rfind("mons/", 0) != 0) name = "mons/" + name;
  auto it = gPackedFiles.find(name);
  if (it == gPackedFiles.end()) return nullptr;
  const PackedFile &entry = it->second;
  AAsset *asset = AAssetManager_open(gAssets, entry.pack.c_str(), AASSET_MODE_RANDOM);
  if (!asset || AAsset_seek(asset, entry.offset, SEEK_SET) != entry.offset) {
    if (asset) AAsset_close(asset);
    return nullptr;
  }
  uint8_t *data = static_cast<uint8_t *>(malloc(entry.size));
  if (!data || !readAll(asset, data, entry.size)) {
    free(data); data = nullptr;
  }
  AAsset_close(asset);
  if (data && size) *size = entry.size;
  return data;
}

void nvsLoad(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) { gLastSaved = nvs(); return; }
  uint32_t count = 0;
  if (fread(&count, 4, 1, f) != 1 || count > 1024) { fclose(f); return; }
  NvsStore loaded;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t keySize = 0, valueSize = 0;
    if (fread(&keySize, 4, 1, f) != 1 || !keySize || keySize > 64) break;
    std::string key(keySize, '\0');
    if (fread(key.data(), 1, keySize, f) != keySize ||
        fread(&valueSize, 4, 1, f) != 1 || valueSize > 4096) break;
    std::vector<uint8_t> value(valueSize);
    if (valueSize && fread(value.data(), 1, valueSize, f) != valueSize) break;
    loaded[key] = std::move(value);
  }
  fclose(f);
  nvs() = std::move(loaded);
  gLastSaved = nvs();
}

void nvsSave(const char *path) {
  std::string temp = std::string(path) + ".tmp";
  FILE *f = fopen(temp.c_str(), "wb");
  if (!f) return;
  uint32_t count = (uint32_t)nvs().size();
  bool ok = fwrite(&count, 4, 1, f) == 1;
  for (const auto &entry : nvs()) {
    uint32_t keySize = (uint32_t)entry.first.size(), valueSize = (uint32_t)entry.second.size();
    ok = ok && fwrite(&keySize, 4, 1, f) == 1;
    ok = ok && fwrite(entry.first.data(), 1, keySize, f) == keySize;
    ok = ok && fwrite(&valueSize, 4, 1, f) == 1;
    ok = ok && (!valueSize || fwrite(entry.second.data(), 1, valueSize, f) == valueSize);
    if (!ok) break;
  }
  ok = ok && fflush(f) == 0;
  fclose(f);
  if (!ok || rename(temp.c_str(), path) != 0) remove(temp.c_str());
}

static void saveIfDirty(bool force) {
  uint32_t now = millis();
  if (!force && now - gLastSaveCheck < 1000) return;
  gLastSaveCheck = now;
  if (force || nvs() != gLastSaved) {
    nvsSave(gSavePath.c_str());
    gLastSaved = nvs();
  }
}

static void checkpointGame() {
  if (gGameLifecycle.checkpoint(pet, millis(), rtcEpoch(), androidUtcEpoch()))
    saveIfDirty(true);
}

void FakeESP::restart() {
  // Import already replaced NVS. Flush it before closing and forbid all later
  // game ticks/checkpoints from saving the receiver's stale in-memory creature.
  gGameLifecycle.requestRestart();
  saveIfDirty(true);
  if (gApp && gApp->activity) ANativeActivity_finish(gApp->activity);
}

bool androidRestartPending() { return gGameLifecycle.restartPending(); }

static void suspendGame() {
  gGameLifecycle.suspend(pet, millis(), rtcEpoch(), androidUtcEpoch());
  checkpointGame();
}

static bool mapTouch(float x, float y, int *outX, int *outY) {
  if (!gWindow) return false;
  int width = ANativeWindow_getWidth(gWindow), height = ANativeWindow_getHeight(gWindow);
  int side = std::min(width, height);
  int left = (width - side) / 2, top = (height - side) / 2;
  if (x < left || y < top || x >= left + side || y >= top + side) return false;
  *outX = std::clamp((int)((x - left) * PANEL / side), 0, PANEL - 1);
  *outY = std::clamp((int)((y - top) * PANEL / side), 0, PANEL - 1);
  return true;
}

static int32_t onInput(android_app *, AInputEvent *event) {
  if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
  int action = AMotionEvent_getAction(event);
  int masked = action & AMOTION_EVENT_ACTION_MASK;
  size_t pointer = (size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                            AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
  if (masked == AMOTION_EVENT_ACTION_MOVE) pointer = 0;
  int x = 0, y = 0;
  bool inside = mapTouch(AMotionEvent_getX(event, pointer), AMotionEvent_getY(event, pointer), &x, &y);
  if (inside) { g_touchX = x; g_touchY = y; }
  if (masked == AMOTION_EVENT_ACTION_DOWN || masked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
    g_touchDown = inside;
    emuFireInterrupt();
    return 1;
  }
  if (masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_POINTER_UP ||
      masked == AMOTION_EVENT_ACTION_CANCEL) {
    g_touchDown = false;
    emuFireInterrupt();
    return 1;
  }
  return masked == AMOTION_EVENT_ACTION_MOVE ? 1 : 0;
}

static void configureWindow() {
  if (gWindow) ANativeWindow_setBuffersGeometry(gWindow, 0, 0, WINDOW_FORMAT_RGBA_8888);
}

static void onCommand(android_app *app, int32_t command) {
  switch (command) {
    case APP_CMD_INIT_WINDOW:
      gWindow = app->window;
      configureWindow();
      if (gActive) gGameLifecycle.resume(pet, millis(), rtcEpoch(), androidUtcEpoch());
      break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
      configureWindow();
      break;
    case APP_CMD_TERM_WINDOW:
      suspendGame();
      gWindow = nullptr;
      break;
    case APP_CMD_GAINED_FOCUS:
    case APP_CMD_RESUME:
      gActive = true;
      gGameLifecycle.resume(pet, millis(), rtcEpoch(), androidUtcEpoch());
      androidAudioSetActive(true);
      break;
    case APP_CMD_LOST_FOCUS:
    case APP_CMD_PAUSE:
    case APP_CMD_STOP:
      gActive = false;
      androidAudioSetActive(false);
      suspendGame();
      break;
    default:
      break;
  }
}

static void presentFrame() {
  if (!gWindow || !gfx || !gfx->frameReady) return;
  gfx->frameReady = false;
  ANativeWindow_Buffer buffer{};
  if (ANativeWindow_lock(gWindow, &buffer, nullptr) != 0) return;
  uint8_t *pixels = static_cast<uint8_t *>(buffer.bits);
  for (int y = 0; y < buffer.height; ++y)
    memset(pixels + (size_t)y * buffer.stride * 4, 0, (size_t)buffer.width * 4);
  int side = std::min(buffer.width, buffer.height);
  int left = (buffer.width - side) / 2, top = (buffer.height - side) / 2;
  const uint16_t *src = gfx->buffer();
  for (int y = 0; y < side; ++y) {
    int sy = y * PANEL / side;
    uint8_t *row = pixels + ((size_t)(top + y) * buffer.stride + left) * 4;
    for (int x = 0; x < side; ++x) {
      uint16_t c = src[(size_t)sy * PANEL + x * PANEL / side];
      row[x * 4 + 0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
      row[x * 4 + 1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
      row[x * 4 + 2] = (uint8_t)((c & 31) * 255 / 31);
      row[x * 4 + 3] = 255;
    }
  }
  ANativeWindow_unlockAndPost(gWindow);
}

void android_main(android_app *app) {
  app_dummy();
  gGameLifecycle = AndroidGameLifecycle{};
  gActive = false;
  gWindow = nullptr;
  gApp = app;
  gAssets = app->activity->assetManager;
  app->onAppCmd = onCommand;
  app->onInputEvent = onInput;
  ANativeActivity_setWindowFlags(app->activity,
      AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_KEEP_SCREEN_ON, 0);

  static const char *packs[] = {
    "sprites-alola.pak", "sprites-galar.pak", "sprites-hoenn.pak",
    "sprites-johto.pak", "sprites-kalos.pak", "sprites-kanto.pak",
    "sprites-paldea.pak", "sprites-sinnoh.pak", "sprites-unova.pak"
  };
  int indexed = 0;
  for (const char *pack : packs) if (indexPack(pack)) ++indexed;
  __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
      "Sprite index ready: %zu unique files from %d packs", gPackedFiles.size(), indexed);

  gSavePath = std::string(app->activity->internalDataPath) + "/tamapoke.nvs";
  nvsLoad(gSavePath.c_str());
  // ko.1.1.2 and earlier could persist an app-private RTC offset. On the first
  // fixed launch, preserve the saved level/state and rebase only its timestamp
  // to the phone/watch clock instead of interpreting the old offset as offline
  // play and jumping dozens of levels.
  Preferences clockMigration;
  clockMigration.begin("tamapoke", false);
  uint32_t localNow = rtcEpoch();
  uint32_t utcNow = androidUtcEpoch();
  uint32_t savedUtc = clockMigration.getUInt("aseen", 0);
  bool migrated = clockMigration.getBool("andclk1", false);
  if (localNow && utcNow) {
    if (migrated && savedUtc && utcNow > savedUtc) {
      uint32_t elapsed = utcNow - savedUtc;
      uint32_t cap = 14UL * 24 * 60 * 60;
      if (elapsed > cap) elapsed = cap;
      clockMigration.putUInt("seen", localNow > elapsed ? localNow - elapsed : localNow);
    } else {
      // First fixed launch (or a clock correction backwards): keep the saved
      // level and establish a safe current-time baseline.
      clockMigration.putUInt("seen", localNow);
    }
    clockMigration.putUInt("aseen", utcNow);
    clockMigration.putBool("andclk1", true);
  }
  setup();
  gGameLifecycle.start();

  while (!app->destroyRequested) {
    int events = 0;
    android_poll_source *source = nullptr;
    int timeout = (gActive && gWindow) ? 0 : -1;
    int ident;
    while ((ident = ALooper_pollOnce(timeout, nullptr, &events,
                                    reinterpret_cast<void **>(&source))) >= 0) {
      if (source) source->process(app, source);
      if (app->destroyRequested) break;
      timeout = 0;
    }
    if (app->destroyRequested) break;
    if (gActive && gWindow && gGameLifecycle.canRun()) {
      loop();
      presentFrame();
      saveIfDirty(false);
      usleep(5000);
    }
  }
  androidAudioSetActive(false);
  checkpointGame();
}
