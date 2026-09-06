#pragma once
#include <Preferences.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ESP's existing 20-KiB NVS partition must not be resized over user data.
// Two SD snapshots are selected by an atomic NVS hash+slot record. An interrupted
// write never touches the selected snapshot. Old NVS rosters remain recovery copies.
#if defined(ESP32) && !defined(ANDROID)
#include <SD_MMC.h>
inline uint32_t rosterFileHash(const uint8_t *p,size_t n) {
  uint32_t h=2166136261u;while(n--){h^=*p++;h*=16777619u;}return h;
}
inline void rosterPath(char *path,unsigned slot) {
  snprintf(path,64,"/tamapoke-roster-%012llx-%u.bin",(unsigned long long)ESP.getEfuseMac(),slot);
}
inline size_t rosterStoredSize(Preferences &p) {
  if(!p.isKey("rosterSD")) return p.getBytesLength("rosterF");
  uint64_t tag=p.getULong64("rosterSD",0);char path[64];rosterPath(path,tag&1);
  File f=SD_MMC.open(path,FILE_READ);if(!f)return SIZE_MAX;
  size_t n=f.size();f.close();return n;
}
inline size_t rosterRead(Preferences &p,uint8_t *out,size_t cap) {
  if(!p.isKey("rosterSD"))return p.getBytes("rosterF",out,cap);
  uint64_t tag=p.getULong64("rosterSD",0);char path[64];rosterPath(path,tag&1);
  File f=SD_MMC.open(path,FILE_READ);if(!f)return 0;
  size_t n=f.size();bool ok=n<=cap && f.read(out,n)==n;f.close();
  return ok && rosterFileHash(out,n)==(uint32_t)(tag>>1)?n:0;
}
inline bool rosterWrite(Preferences &p,const uint8_t *raw,size_t n) {
  unsigned slot=p.isKey("rosterSD")?1-(p.getULong64("rosterSD",0)&1):0;
  char path[64];rosterPath(path,slot);
  File f=SD_MMC.open(path,FILE_WRITE);if(!f)return false;
  bool ok=f.write(raw,n)==n;f.flush();f.close();if(!ok)return false;
  uint8_t *check=(uint8_t*)malloc(n);if(!check)return false;
  f=SD_MMC.open(path,FILE_READ);
  ok=f && f.size()==n && f.read(check,n)==n && !memcmp(raw,check,n);
  f.close();free(check);if(!ok)return false;
  uint64_t tag=((uint64_t)rosterFileHash(raw,n)<<1)|slot;
  p.putULong64("rosterSD",tag);return p.getULong64("rosterSD",~tag)==tag;
}
inline bool rosterExists(Preferences &p){return p.isKey("rosterSD")||p.isKey("rosterF");}
#else
inline size_t rosterStoredSize(Preferences &p){return p.getBytesLength("rosterF");}
inline size_t rosterRead(Preferences &p,uint8_t *out,size_t cap){return p.getBytes("rosterF",out,cap);}
inline bool rosterWrite(Preferences &p,const uint8_t *raw,size_t n){
  p.putBytes("rosterF",raw,n);return p.getBytesLength("rosterF")==n;
}
inline bool rosterExists(Preferences &p){return p.isKey("rosterF");}
#endif
