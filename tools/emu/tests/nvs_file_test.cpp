#include "Arduino.h"
#include "nvs_file.h"
#include <filesystem>
#include <fstream>
#include <chrono>

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
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++bad;
}
static std::vector<uint8_t> bytes(const std::string &p) {
  std::ifstream f(std::filesystem::u8path(p), std::ios::binary);
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
static void write(const std::string &p, const std::vector<uint8_t> &v) {
  std::ofstream f(std::filesystem::u8path(p), std::ios::binary | std::ios::trunc);
  f.write((const char *)v.data(), v.size());
}
int main() {
  namespace fs = std::filesystem;
  auto dir = fs::current_path() / ("nvs-test-" + std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directory(dir);
  std::string path = (dir / "save.nvs").u8string();
  NvsStore state; NvsFile file;
  ck(file.load(path.c_str(), state) && state.empty(), "missing file starts a new save");
  // These keys use the exact on-disk ordering that exposed the Android bug.
  state["age"] = {60}; state["rosterF"] = std::vector<uint8_t>(22116);
  for (size_t i=0; i<22116; ++i) state["rosterF"][i] = (uint8_t)(i * 17);
  state["sleep"] = {1}; state["slpa"] = {1}; state["snd"] = {0}; state["vol"] = {3};
  ck(file.save(path.c_str(), state), "writes 22116-byte roster and later settings");
  const auto original = bytes(path);
  NvsStore restored; NvsFile reopened;
  ck(reopened.load(path.c_str(), restored) && restored == state,
     "cold disk reopen preserves every byte including sound, volume and sleep");
  state["snd"] = {1}; state["vol"] = {0}; state["sleep"] = {0};
  ck(file.save(path.c_str(), state) && reopened.load(path.c_str(), restored) && restored==state,
     "atomic overwrite persists awake state and enabled sound at zero volume");
  // Same old format, including zero-length values and legacy small records.
  state = {{"old", {1,2,3}}, {"empty", {}}};
  ck(file.save(path.c_str(), state) && reopened.load(path.c_str(), restored) && restored==state,
     "legacy small records and empty values remain compatible");
  auto rejected = [&](std::vector<uint8_t> corrupt, const char *label) {
    write(path, corrupt); const auto before = restored;
    ck(!reopened.load(path.c_str(), restored) && restored == before &&
       !reopened.save(path.c_str(), state, true) && bytes(path) == corrupt, label);
  };
  for (size_t n : {size_t(0),size_t(3),size_t(12),size_t(4096),original.size()-1})
    rejected({original.begin(), original.begin()+n}, "truncation never exposes partial records or overwrites source");
  auto corrupt=original; corrupt.push_back(0);
  rejected(corrupt, "trailing garbage rejected without changing original");
  corrupt=original; uint32_t excessive=NvsFile::MAX_RECORDS+1;
  memcpy(corrupt.data(),&excessive,4);
  rejected(corrupt, "excessive record count rejected");
  // A duplicate key is not a valid std::map snapshot.
  rejected({2,0,0,0, 1,0,0,0,'x',1,0,0,0,1, 1,0,0,0,'x',1,0,0,0,2},
           "duplicate key rejected");
  rejected({1,0,0,0, 1,0,0,0,'x',1,0,1,0}, "oversized value rejected before allocation");
  write(path, original); ck(reopened.load(path.c_str(), restored), "valid save can be reopened after read failure");
  auto changed=restored; changed["vol"]={9};
  std::string unavailable=(dir/"missing"/"save.nvs").u8string();
  ck(!reopened.save(unavailable.c_str(),changed), "write failure reported");
  ck(reopened.save(path.c_str(),changed) && file.load(path.c_str(),state) && state==changed,
     "failed write stays dirty and succeeds on retry without force");
  const auto valid=bytes(path); changed["huge"]=std::vector<uint8_t>(NvsFile::MAX_VALUE+1);
  ck(!reopened.save(path.c_str(),changed) && bytes(path)==valid,
     "invalid output never replaces existing good save");
  fs::remove_all(dir); // Only this test's uniquely-created scratch directory.
  printf("%s\n", bad ? "FAILURES" : "all good"); return bad ? 1 : 0;
}
