// Android/Wear and PC share the existing length-prefixed NVS file format.
// Validate the entire file before exposing any records: rosterF alone is 22 KB.
#pragma once
#include "Preferences.h"
#include <cerrno>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#pragma push_macro("INPUT")
#undef INPUT
#include <windows.h>
#pragma pop_macro("INPUT")
#include <io.h>
#include <filesystem>
#else
#include <unistd.h>
#endif

class NvsFile {
public:
  static constexpr uint32_t MAX_RECORDS = 1024;
  static constexpr uint32_t MAX_VALUE = 64 * 1024;
  static constexpr size_t MAX_FILE = 1024 * 1024;

  bool load(const char *path, NvsStore &store) {
    writable = false;
    FILE *f = open(path, "rb");
    if (!f) {
      if (errno != ENOENT) return false;
      store.clear(); lastSaved.clear(); writable = true; return true;
    }
    NvsStore loaded;
    uint32_t count = 0;
    size_t bytes = 4;
    bool ok = fread(&count, 4, 1, f) == 1 && count <= MAX_RECORDS;
    for (uint32_t i = 0; ok && i < count; ++i) {
      uint32_t ks = 0, vs = 0;
      ok = fread(&ks, 4, 1, f) == 1 && ks && ks <= 64;
      if (!ok) break;
      std::string key(ks, '\0');
      ok = fread(key.data(), 1, ks, f) == ks &&
           key.find('\0') == std::string::npos && !loaded.count(key) &&
           fread(&vs, 4, 1, f) == 1 && vs <= MAX_VALUE;
      bytes += 8 + ks + vs;
      if (!ok || bytes > MAX_FILE) { ok = false; break; }
      std::vector<uint8_t> value(vs);
      ok = !vs || fread(value.data(), 1, vs, f) == vs;
      if (ok) loaded.emplace(std::move(key), std::move(value));
    }
    ok = ok && fgetc(f) == EOF && !ferror(f);
    if (fclose(f) != 0) ok = false;
    if (!ok) return false; // Never publish a partial map or overwrite a bad file.
    store = std::move(loaded); lastSaved = store; writable = true;
    return true;
  }

  bool save(const char *path, const NvsStore &store, bool force = false) {
    if (!writable) return false;
    if (!force && store == lastSaved) return true;
    if (store.size() > MAX_RECORDS) return false;
    size_t bytes = 4;
    for (const auto &entry : store) {
      if (entry.first.empty() || entry.first.size() > 64 ||
          entry.first.find('\0') != std::string::npos || entry.second.size() > MAX_VALUE)
        return false;
      bytes += 8 + entry.first.size() + entry.second.size();
      if (bytes > MAX_FILE) return false;
    }
    std::string temp = std::string(path) + ".tmp";
    FILE *f = open(temp.c_str(), "wb");
    if (!f) return false;
    uint32_t count = (uint32_t)store.size();
    bool ok = fwrite(&count, 4, 1, f) == 1;
    for (const auto &entry : store) {
      uint32_t ks = (uint32_t)entry.first.size(), vs = (uint32_t)entry.second.size();
      ok = ok && fwrite(&ks, 4, 1, f) == 1 &&
           fwrite(entry.first.data(), 1, ks, f) == ks &&
           fwrite(&vs, 4, 1, f) == 1 &&
           (!vs || fwrite(entry.second.data(), 1, vs, f) == vs);
      if (!ok) break;
    }
    ok = ok && fflush(f) == 0;
#ifdef _WIN32
    ok = ok && _commit(_fileno(f)) == 0;
#else
    ok = ok && fsync(fileno(f)) == 0;
#endif
    if (fclose(f) != 0) ok = false;
#ifdef _WIN32
    auto from = std::filesystem::u8path(temp), to = std::filesystem::u8path(path);
    ok = ok && MoveFileExW(from.c_str(), to.c_str(),
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(from.c_str());
#else
    ok = ok && rename(temp.c_str(), path) == 0;
    if (!ok) remove(temp.c_str());
#endif
    if (ok) lastSaved = store; // Failed writes remain dirty for the next retry.
    return ok;
  }

private:
  bool writable = false;
  NvsStore lastSaved;
  static FILE *open(const char *path, const char *mode) {
#ifdef _WIN32
    auto wide = std::filesystem::u8path(path);
    return _wfopen(wide.c_str(), mode[0] == 'r' ? L"rb" : L"wb");
#else
    return fopen(path, mode);
#endif
  }
};
