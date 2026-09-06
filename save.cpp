#include "save.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include <stdlib.h>
#include "party.h"
#include "roster_store.h"

// Every key the firmware persists. Adding one here is the whole job of adding
// it to the backup; save_test fails if a key exists in NVS and not in this list.
const SaveField SAVE_FIELDS[] = {
  // the creature
  { "init", SK_BOOL },  { "full", SK_U8 },    { "joy", SK_U8 },
  { "ene", SK_U8 },     { "hyg", SK_U8 },     { "poop", SK_U8 },
  { "wgt", SK_U8 },     { "age", SK_U32 },    { "dexn", SK_I16 },
  { "eggT2", SK_I16 },  { "crack", SK_U8 },   { "mist", SK_U8 },
  { "sleep", SK_BOOL }, { "lend", SK_U8 },    { "seen", SK_U32 },
  { "bond", SK_U8 },    { "nick", SK_STR },   { "froz", SK_BOOL },
  // individual values and training
  { "ivat", SK_U8 },    { "ivdf", SK_U8 },    { "ivsp", SK_U8 },
  { "ivhp", SK_U8 },    { "tatk", SK_U8 },    { "tdef", SK_U8 },
  { "tspe", SK_U8 },
  // moves
  { "mvs", SK_BYTES },  { "mvlv", SK_U8 },
  { "mvq", SK_BYTES },  { "mvqn", SK_U8 },
  // flags
  { "bk", SK_BOOL },    { "shy", SK_BOOL },   { "eshy", SK_BOOL },
  { "stpk", SK_BOOL },  { "evop", SK_U8 },    { "slpa", SK_U8 },    { "rtpn", SK_BOOL },
  // the player: outlives every creature, which is exactly why it must be here
  { "tnam", SK_STR },   { "avtr", SK_U8 },    { "badg", SK_U16 },
  { "reg", SK_U8 },     { "eggR", SK_BYTES },
  { "badgX", SK_BYTES },{ "badhX", SK_BYTES },
  { "badh", SK_U16 },   { "dexreg", SK_BYTES }, { "dexsh", SK_BYTES },
  { "strk", SK_U16 },   { "bstrk", SK_U16 },  { "cday", SK_U32 },
  { "byeQuota", SK_U32 },
  { "cer", SK_U8 }, { "endWait", SK_BYTES }, { "endEgg", SK_U32 },
  { "medal", SK_U16 },  { "tmedal", SK_U16 }, { "mstone", SK_U16 },
  { "ghi", SK_U16 },    { "shi", SK_U16 },    { "qhi", SK_U16 },
  // the banked creatures
  { "party", SK_BYTES }, { "box", SK_BYTES },
  { "rosterF", SK_BYTES }, { "form", SK_U16 }, { "formDex", SK_I16 },
  { "liveCare", SK_BYTES },
  // settings, so a restored device plays the way it did
  { "lang", SK_U8 },    { "snd", SK_BOOL },   { "vol", SK_U8 },
};
const uint16_t SAVE_FIELD_COUNT = sizeof(SAVE_FIELDS) / sizeof(SAVE_FIELDS[0]);

#define MAX_VAL PARTY_ROSTER_BYTES
static_assert(MAX_VAL + 1024 < SAVE_MAX_BYTES,
              "whole-save buffer must leave room beyond the box blob");

static uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint16_t)p[i] << 8;
    for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021)
                                                : (uint16_t)(c << 1);
  }
  return c;
}

static uint8_t widthOf(uint8_t kind) {
  switch (kind) {
    case SK_U8: case SK_I8: case SK_BOOL: return 1;
    case SK_U16: case SK_I16: return 2;
    case SK_U32: return 4;
    default: return 0;               // variable
  }
}

// Reads one field out of NVS. Returns its length, or -1 if the key is absent --
// absent is normal (a fresh save has no party) and is simply left out.
static int readField(Preferences &p, const SaveField &f, uint8_t *val) {
  if(!strcmp(f.key,"rosterF") && rosterExists(p)) {
    size_t n=rosterStoredSize(p);
    if(n>MAX_VAL || rosterRead(p,val,MAX_VAL)!=n)return -2;
    return (int)n;
  }
  if (!p.isKey(f.key)) return -1;
  switch (f.kind) {
    case SK_U8:   val[0] = p.getUChar(f.key, 0); return 1;
    case SK_I8:   val[0] = (uint8_t)p.getChar(f.key, 0); return 1;
    case SK_BOOL: val[0] = p.getBool(f.key, false) ? 1 : 0; return 1;
    case SK_U16: { uint16_t v = p.getUShort(f.key, 0); memcpy(val, &v, 2); return 2; }
    case SK_I16: { int16_t v = p.getShort(f.key, 0); memcpy(val, &v, 2); return 2; }
    case SK_U32: { uint32_t v = p.getUInt(f.key, 0); memcpy(val, &v, 4); return 4; }
    case SK_BYTES: {
      size_t n = p.getBytesLength(f.key);
      if (n > MAX_VAL) return -1;
      p.getBytes(f.key, val, n);
      return (int)n;
    }
    case SK_STR: {
      char tmp[64] = {0};
      p.getString(f.key, tmp, sizeof(tmp));
      size_t n = strlen(tmp);
      memcpy(val, tmp, n);
      return (int)n;
    }
  }
  return -1;
}

static void writeField(Preferences &p, const SaveField &f,
                       const uint8_t *val, uint16_t n) {
  switch (f.kind) {
    case SK_U8:   if (n == 1) p.putUChar(f.key, val[0]); break;
    case SK_I8:   if (n == 1) p.putChar(f.key, (int8_t)val[0]); break;
    case SK_BOOL: if (n == 1) p.putBool(f.key, val[0] != 0); break;
    case SK_U16:  if (n == 2) { uint16_t v; memcpy(&v, val, 2); p.putUShort(f.key, v); } break;
    case SK_I16:  if (n == 2) { int16_t v; memcpy(&v, val, 2); p.putShort(f.key, v); } break;
    case SK_U32:  if (n == 4) { uint32_t v; memcpy(&v, val, 4); p.putUInt(f.key, v); } break;
    case SK_BYTES: if (n) p.putBytes(f.key, val, n); break;
    case SK_STR: {
      char tmp[64] = {0};
      if (n >= sizeof(tmp)) return;
      memcpy(tmp, val, n);
      tmp[n] = 0;
      p.putString(f.key, tmp);
      break;
    }
  }
}

size_t saveExportSize() {
  return SAVE_MAX_BYTES;
}

size_t saveExport(uint8_t *out, size_t cap) {
  if (cap < SAVE_HDR + 2) return 0;
  Preferences p;
  p.begin("tamapoke", true);
  size_t at = SAVE_HDR;
  uint16_t count = 0;
  uint8_t *val=(uint8_t*)malloc(MAX_VAL);
  if(!val) {p.end();return 0;}
  for (uint16_t i = 0; i < SAVE_FIELD_COUNT; i++) {
    const SaveField &f = SAVE_FIELDS[i];
    int n = readField(p, f, val);
    if(n==-2){free(val);p.end();return 0;} // never export a silently truncated roster
    if (n < 0) continue;             // absent: nothing to back up
    size_t klen = strlen(f.key);
    if (klen > 15) continue;         // NVS keys cannot be longer anyway
    if (at + 1 + klen + 1 + 2 + (size_t)n + 2 > cap) { free(val);p.end(); return 0; }
    out[at++] = (uint8_t)klen;
    memcpy(out + at, f.key, klen); at += klen;
    out[at++] = f.kind;
    out[at++] = (uint8_t)(n & 0xFF);
    out[at++] = (uint8_t)(n >> 8);
    memcpy(out + at, val, (size_t)n); at += (size_t)n;
    count++;
  }
  p.end();
  free(val);
  out[0] = SAVE_MAGIC0; out[1] = SAVE_MAGIC1;
  out[2] = SAVE_MAGIC2; out[3] = SAVE_MAGIC3;
  out[4] = SAVE_VERSION;
  out[5] = (uint8_t)(count & 0xFF);
  out[6] = (uint8_t)(count >> 8);
  out[7] = 0;
  uint16_t c = crc16(out, at);
  out[at++] = (uint8_t)(c & 0xFF);
  out[at++] = (uint8_t)(c >> 8);
  return at;
}

// Looks a key up in the table. An unknown key is skipped rather than trusted:
// the kind on the wire decides nothing, the table does.
static const SaveField *fieldFor(const char *key, uint8_t kind) {
  for (uint16_t i = 0; i < SAVE_FIELD_COUNT; i++)
    if (SAVE_FIELDS[i].kind == kind && !strcmp(SAVE_FIELDS[i].key, key))
      return &SAVE_FIELDS[i];
  return nullptr;
}

bool saveValidate(const uint8_t *in, size_t n) {
  if (n < SAVE_HDR + 2) return false;
  if (in[0] != SAVE_MAGIC0 || in[1] != SAVE_MAGIC1 ||
      in[2] != SAVE_MAGIC2 || in[3] != SAVE_MAGIC3) return false;
  // Version 2 widens stored move IDs. The current reader has explicit legacy
  // migration for v1 fields, so old backups remain importable; an old reader
  // still rejects v2 before it can misread a 16-bit party/box stride.
  if (in[4] < SAVE_MIN_VERSION || in[4] > SAVE_VERSION) return false;
  uint16_t count = (uint16_t)in[5] | ((uint16_t)in[6] << 8);
  uint16_t want = (uint16_t)in[n - 2] | ((uint16_t)in[n - 1] << 8);
  if (crc16(in, n - 2) != want) return false;

  // Walk the whole thing and prove it parses. Nothing is written here.
  size_t at = SAVE_HDR;
  uint16_t seen = 0;
  while (at + 4 <= n - 2) {
    uint8_t klen = in[at];
    if (!klen || klen > 15 || at + 1 + klen + 3 > n - 2) return false;
    size_t vat = at + 1 + klen;
    uint8_t kind = in[vat];
    uint16_t vlen = (uint16_t)in[vat + 1] | ((uint16_t)in[vat + 2] << 8);
    if (vlen > MAX_VAL) return false;
    if (vat + 3 + vlen > n - 2) return false;
    uint8_t w = widthOf(kind);
    if (w && vlen != w) return false;          // a scalar of the wrong width
    // A well-formed checksum is not enough: duplicate or wrong-typed known
    // keys would otherwise silently discard parts of the receiving save.
    for(uint8_t i=0;i<klen;i++) {
      uint8_t c=in[at+1+i];
      if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;
    }
    char key[16]={};memcpy(key,in+at+1,klen);
    for(uint16_t i=0;i<SAVE_FIELD_COUNT;i++)
      if(!strcmp(key,SAVE_FIELDS[i].key) && kind!=SAVE_FIELDS[i].kind)return false;
    if(kind==SK_STR && vlen>=64)return false;
    for(size_t prior=SAVE_HDR;prior<at;) {
      uint8_t k=in[prior];size_t v=prior+1+k;
      if(k==klen && !memcmp(in+prior+1,key,klen))return false;
      prior=v+3+in[v+1]+((size_t)in[v+2]<<8);
    }
    at = vat + 3 + vlen;
    seen++;
  }
  if (at != n - 2 || seen != count) return false;
  return true;
}

static const uint8_t *snapshotField(const uint8_t *in,size_t n,const SaveField &f,uint16_t &len) {
  for(size_t at=SAVE_HDR;at+4<=n-2;) {
    uint8_t k=in[at];size_t v=at+1+k;len=in[v+1]|((uint16_t)in[v+2]<<8);
    if(k==strlen(f.key) && !memcmp(in+at+1,f.key,k) && in[v]==f.kind)return in+v+3;
    at=v+3+len;
  }
  return nullptr;
}

static bool applySnapshot(Preferences &p,const uint8_t *in,size_t n,uint8_t *check) {
  for(uint16_t i=0;i<SAVE_FIELD_COUNT;i++) {
    const auto &f=SAVE_FIELDS[i];uint16_t len=0;const uint8_t *value=snapshotField(in,n,f,len);
    if(!value)continue;
    if(readField(p,f,check)==len && !memcmp(value,check,len))continue;
#if defined(ESP32) && !defined(ANDROID)
    if(!strcmp(f.key,"rosterF")) {if(!rosterWrite(p,value,len))return false;}
    else
#endif
    writeField(p,f,value,len);
    if(readField(p,f,check)!=len || memcmp(value,check,len))return false;
  }
  // Remove only after all incoming values were verified. In particular, never
  // clear the namespace before a potentially failing large roster write.
  for(uint16_t i=0;i<SAVE_FIELD_COUNT;i++) {
    const auto &f=SAVE_FIELDS[i];uint16_t len=0;
    if(snapshotField(in,n,f,len))continue;
    if(p.isKey(f.key) && !p.remove(f.key))return false;
#if defined(ESP32) && !defined(ANDROID)
    if(!strcmp(f.key,"rosterF") && p.isKey("rosterSD") && !p.remove("rosterSD"))return false;
#endif
  }
  return true;
}

bool saveImport(const uint8_t *in,size_t n) {
  if(!saveValidate(in,n))return false;
#if defined(ESP32) && !defined(ANDROID)
  uint8_t *old=(uint8_t*)ps_malloc(SAVE_MAX_BYTES),*check=(uint8_t*)ps_malloc(MAX_VAL);
#else
  uint8_t *old=(uint8_t*)malloc(SAVE_MAX_BYTES),*check=(uint8_t*)malloc(MAX_VAL);
#endif
  if(!old || !check){free(old);free(check);return false;}
  size_t oldN=saveExport(old,SAVE_MAX_BYTES);
  if(!oldN){free(old);free(check);return false;}
  Preferences p;p.begin("tamapoke",false);
  bool ok=applySnapshot(p,in,n,check);
  // Device-local UTC baseline must not survive a successful foreign import.
  if(ok && p.isKey("aseen"))ok=p.remove("aseen");
  if(!ok && !applySnapshot(p,old,oldN,check))
    Serial.println("save: storage failure during rollback; restore external backup");
  p.end();free(old);free(check);return ok;
}
