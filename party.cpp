#include "party.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "dex.h"
#include "moves.h"
#include "pet.h"
#include <new>
#include "roster_store.h"
#include "i18n.h"
#include <algorithm>

Party party;
bool activeSwapBlocked=false;
bool Party::hasEndedMon(const PartyMon &m) const {
  uint32_t ticket=0;memcpy(&ticket,m.care+32,4);
  if(!ticket || m.empty() || !writable())return false;
  for(const auto &s:slots)if(!memcmp(&s,&m,sizeof(m)))return true;
  for(const auto &s:box)if(!memcmp(&s,&m,sizeof(m)))return true;
  return false;
}
static constexpr size_t LEGACY_STRIDE = 34;
static_assert(offsetof(PartyMon,moves)==26 && offsetof(PartyMon,form)==34 && offsetof(PartyMon,care)==36 && sizeof(PartyMon)==PARTY_RECORD_BYTES,
              "legacy prefix or runtime extension changed");
static constexpr size_t ROSTER_N = PARTY_STORAGE_SLOTS + BOX_SLOTS;
static constexpr size_t ROSTER_BYTES = PARTY_ROSTER_BYTES;
static uint32_t rosterHash(const uint8_t *p, size_t n) {
  uint32_t h=2166136261u;
  while(n--) {h^=*p++; h*=16777619u;}
  return h;
}

// The last released layout ended with four one-byte move IDs. Keep an exact
// reader for it: treating those bytes as the prefix of MoveId[4] would merge
// adjacent IDs (1,2 becomes 513) and silently change every banked moveset.
struct LegacyPartyMon8 {
  int16_t dex;
  uint16_t level;
  uint16_t medals;
  uint8_t ivAtk, ivDef, ivSpe, ivHp;
  uint8_t trAtk, trDef, trSpe;
  uint8_t shiny;
  char nick[12];
  uint8_t moves[MOVE_SLOTS];
};
static_assert(sizeof(LegacyPartyMon8) == 30, "legacy party stride changed");

struct LegacyPartyMon0 {
  int16_t dex;
  uint16_t level;
  uint16_t medals;
  uint8_t ivAtk, ivDef, ivSpe, ivHp;
  uint8_t trAtk, trDef, trSpe;
  uint8_t shiny;
  char nick[12];
};
static_assert(sizeof(LegacyPartyMon0) == 26, "pre-move party stride changed");

static void copyLegacy(PartyMon &out, const LegacyPartyMon8 &in) {
  out = PartyMon();
  out.dex = in.dex; out.level = in.level; out.medals = in.medals;
  out.ivAtk = in.ivAtk; out.ivDef = in.ivDef; out.ivSpe = in.ivSpe; out.ivHp = in.ivHp;
  out.trAtk = in.trAtk; out.trDef = in.trDef; out.trSpe = in.trSpe;
  out.shiny = in.shiny;
  memcpy(out.nick, in.nick, sizeof(out.nick));
  for (int i = 0; i < MOVE_SLOTS; i++) out.moves[i] = in.moves[i];
}

static void copyLegacy(PartyMon &out, const LegacyPartyMon0 &in) {
  out = PartyMon();
  out.dex = in.dex; out.level = in.level; out.medals = in.medals;
  out.ivAtk = in.ivAtk; out.ivDef = in.ivDef; out.ivSpe = in.ivSpe; out.ivHp = in.ivHp;
  out.trAtk = in.trAtk; out.trDef = in.trDef; out.trSpe = in.trSpe;
  out.shiny = in.shiny;
  memcpy(out.nick, in.nick, sizeof(out.nick));
}

// Loads any known roster stride and preserves as many leading slots as fit.
// Returns true when a legacy/different-capacity blob should be rewritten.
static bool loadRoster(Preferences &prefs, const char *key,
                       PartyMon *out, size_t capacity) {
  size_t stored = prefs.getBytesLength(key);
  if (!stored) return false;
  uint8_t *raw = (uint8_t *)malloc(stored);
  if (!raw) return false;
  if (prefs.getBytes(key, raw, stored) != stored) { free(raw); return false; }

  size_t stride = 0, count = 0;
  enum { CURRENT, LEGACY8, LEGACY0 } format = CURRENT;
  if (stored % LEGACY_STRIDE == 0) {
    stride = LEGACY_STRIDE; count = stored / stride;
  } else if (stored % sizeof(LegacyPartyMon8) == 0) {
    format = LEGACY8; stride = sizeof(LegacyPartyMon8); count = stored / stride;
  } else if (stored % sizeof(LegacyPartyMon0) == 0) {
    format = LEGACY0; stride = sizeof(LegacyPartyMon0); count = stored / stride;
  }
  if (!stride) { free(raw); return false; }
  if (count > capacity) count = capacity;
  for (size_t i = 0; i < count; i++) {
    if (format == CURRENT) memcpy(&out[i], raw + i * stride, LEGACY_STRIDE);
    else if (format == LEGACY8) {
      LegacyPartyMon8 old;
      memcpy(&old, raw + i * stride, sizeof(old));
      copyLegacy(out[i], old);
    } else {
      LegacyPartyMon0 old;
      memcpy(&old, raw + i * stride, sizeof(old));
      copyLegacy(out[i], old);
    }
  }
  free(raw);
  return true;
}

// Same NVS namespace as the pet on purpose: WIPE (Pet::factoryReset) calls
// clear() on it, and a factory reset that left the party behind would be a lie.
void Party::begin() {
  // Start from empty: getBytes() leaves the destination untouched when the key
  // is missing, so without this a reload after a wipe would keep showing the
  // old party out of RAM.
  for (auto &s : slots) s = PartyMon();
  prefs.begin("tamapoke", false);
  rosterReadOnly = false;
  pendingLive = PartyMon();
  bool legacyLoaded=loadRoster(prefs, "party", slots, PARTY_STORAGE_SLOTS);
  // a blob written by an older/newer build could hold nonsense; drop anything
  // that is not a real Pokedex number rather than indexing DEX_TBL with it
  for (auto &s : slots) {
    if (s.dex < 1 || s.dex > DEX_COUNT) s.dex = 0;
    s.nick[sizeof(s.nick) - 1] = 0;
    for (int i = 0; i < MOVE_SLOTS; i++) if (s.moves[i] >= MOVE_COUNT) s.moves[i] = 0;
  }
  // The box is a separate key and simply absent on an older save, which leaves
  // it zeroed -- exactly what an empty box is.
  for (auto &s : box) s = PartyMon();
  legacyLoaded=loadRoster(prefs, "box", box, BOX_SLOTS) || legacyLoaded;
  // One atomic NVS blob owns BOTH rosters, including their form IDs. Old
  // records remain recovery copies, not the authority when this key exists.
  if (rosterExists(prefs)) {
    uint8_t *raw=(uint8_t*)malloc(ROSTER_BYTES);
    size_t stored=rosterStoredSize(prefs);
    bool old=false;size_t count=0,stride=0;
    bool valid=raw && stored>=12 && stored<=ROSTER_BYTES &&
      rosterRead(prefs,raw,ROSTER_BYTES)==stored;
    if (valid) {
      old=!memcmp(raw,"TFR1",4);
      bool wide=!memcmp(raw,"TFR3",4);
      count=raw[4]+(wide?((size_t)raw[7]<<8):0);
      stride=old?36:PARTY_RECORD_BYTES;
      uint32_t sum=0; memcpy(&sum,raw+stored-4,4);
      valid=(old || wide || !memcmp(raw,"TFR2",4)) && count>=PARTY_STORAGE_SLOTS && count<=ROSTER_N &&
        stored==12+stride*(count+(old?0:1)) &&
        raw[5]<=(old?0:1) && raw[6]==stride && (wide || raw[7]==0) &&
        sum==rosterHash(raw,stored-4);
    }
    if (valid) {
      for(auto &m:slots)m=PartyMon();for(auto &m:box)m=PartyMon();
      for(size_t i=0;i<count;i++) {
        PartyMon &m=i<PARTY_STORAGE_SLOTS ? slots[i] : box[i-PARTY_STORAGE_SLOTS];
        memcpy(&m,raw+8+(old?36:PARTY_RECORD_BYTES)*i,old?36:PARTY_RECORD_BYTES);
      }
      if(!old && raw[5]) memcpy(&pendingLive,raw+8+PARTY_RECORD_BYTES*count,PARTY_RECORD_BYTES);
      if(!old && raw[5] && (pendingLive.dex<1 || pendingLive.dex>DEX_COUNT || pendingLive.care[0]!=1))
        rosterReadOnly=true;
    } else {
      // Do not replace an unreadable/future authoritative save with defaults.
      rosterReadOnly=true;
      Serial.println("forms: roster unreadable; recovery copies are read-only");
    }
    free(raw);
  }
  for(auto &s:slots) {
    if(s.dex<1 || s.dex>DEX_COUNT) s.dex=0;
    s.nick[sizeof(s.nick)-1]=0;
    for(auto &m:s.moves) if(m>=MOVE_COUNT) m=0;
  }
  for (auto &s : box) {
    if (s.dex < 1 || s.dex > DEX_COUNT) s.dex = 0;
    s.nick[sizeof(s.nick) - 1] = 0;
    for (int i = 0; i < MOVE_SLOTS; i++) if (s.moves[i] >= MOVE_COUNT) s.moves[i] = 0;
  }
  // Builds through ko.1.1.6 allowed six banked members, which made seven with
  // the live pet. Move that former sixth member into the first box opening.
  // If a player's box is completely full, keep it in the reserved physical
  // slot instead of deleting it; the next box opening will migrate it.
  activeSwapBlocked=!pendingLive.empty();
  if(!activeSwapBlocked) migrateLegacyOverflow();
  if(legacyLoaded && !rosterExists(prefs)) saveRoster();
}

bool Party::save() {
  if(!writable())return false;
  saveRoster();
  return !rosterReadOnly;
}

bool Party::boxSave() {
  if(!writable())return false;
  saveRoster();
  return !rosterReadOnly;
}

bool Party::sortBox(BoxSortOrder order) {
  if(tradeStorageBlocked)return false;
  if(rosterReadOnly || !pendingLive.empty() || order>BOX_SORT_LEVEL) return false;
  for(const auto &m:box) if(m.dex>DEX_COUNT) return false;
  // ESP puts the snapshot/keys in PSRAM, not the small UI task stack.
  struct SortScratch { PartyMon before[BOX_SLOTS]; uint16_t index[BOX_SLOTS]; const char *name[BOX_SLOTS]; };
#ifdef ESP32
  auto *s=static_cast<SortScratch*>(ps_malloc(sizeof(SortScratch)));
#else
  auto *s=static_cast<SortScratch*>(malloc(sizeof(SortScratch)));
#endif
  if(!s) return false;
  memcpy(s->before,box,sizeof(box));
  for(uint16_t i=0;i<BOX_SLOTS;i++) {
    s->index[i]=i;
    s->name[i]=box[i].empty()?"":koreanName(DEX_TBL[box[i].dex].name);
  }
  std::sort(s->index,s->index+BOX_SLOTS,[&](uint16_t a,uint16_t b) {
    const auto &x=s->before[a], &y=s->before[b];
    if(x.empty()!=y.empty()) return !x.empty();
    if(x.empty()) return a<b;
    if(order==BOX_SORT_NAME) {
      int cmp=strcmp(s->name[a],s->name[b]); if(cmp) return cmp<0;
    }
    if(order==BOX_SORT_LEVEL && x.level!=y.level) return x.level>y.level;
    if(x.dex!=y.dex) return x.dex<y.dex;
    if(x.level!=y.level) return x.level>y.level;
    return a<b; // stable ties, including different forms and shiny individuals
  });
  for(uint16_t i=0;i<BOX_SLOTS;i++) memcpy(&box[i],&s->before[s->index[i]],sizeof(PartyMon));
  bool ok=!memcmp(box,s->before,sizeof(box)) || boxSave();
  if(!ok) memcpy(box,s->before,sizeof(box));
  free(s);
  return ok;
}

void Party::saveRoster() {
  if(tradeStorageBlocked)return;
  if(rosterReadOnly) return;
  uint8_t *raw=(uint8_t*)calloc(1,ROSTER_BYTES);
  if(!raw) {rosterReadOnly=true;return;}
  memcpy(raw,"TFR3",4); raw[4]=(uint8_t)ROSTER_N; raw[7]=ROSTER_N>>8;
  raw[5]=!pendingLive.empty(); raw[6]=PARTY_RECORD_BYTES;
  for(size_t i=0;i<ROSTER_N;i++) {
    const PartyMon &m=i<PARTY_STORAGE_SLOTS ? slots[i] : box[i-PARTY_STORAGE_SLOTS];
    memcpy(raw+8+PARTY_RECORD_BYTES*i,&m,PARTY_RECORD_BYTES);
  }
  memcpy(raw+8+PARTY_RECORD_BYTES*ROSTER_N,&pendingLive,PARTY_RECORD_BYTES);
  uint32_t sum=rosterHash(raw,ROSTER_BYTES-4);memcpy(raw+ROSTER_BYTES-4,&sum,4);
  // Verify the authoritative write before touching either recovery copy.
  uint8_t *check=(uint8_t*)malloc(ROSTER_BYTES);
  if(!check) {free(raw);rosterReadOnly=true;return;}
  bool written=rosterWrite(prefs,raw,ROSTER_BYTES);
  bool committed=written && check && rosterStoredSize(prefs)==ROSTER_BYTES &&
    rosterRead(prefs,check,ROSTER_BYTES)==ROSTER_BYTES && !memcmp(check,raw,ROSTER_BYTES);
  free(check);
  if(!committed) {free(raw);rosterReadOnly=true;Serial.println("forms: roster write failed; recovery copies retained");return;}
  // Keep released-format recovery copies, without ever writing a 36-byte
  // untagged stride to keys that old releases interpret as 34/30/26 bytes.
  for(size_t i=0;i<PARTY_STORAGE_SLOTS;i++) memcpy(raw+34*i,&slots[i],34);
  prefs.putBytes("party",raw,34*PARTY_STORAGE_SLOTS);
  // Only the released 60-slot recovery prefix fits ESP's original NVS.
  for(size_t i=0;i<60;i++) memcpy(raw+34*i,&box[i],34);
  prefs.putBytes("box",raw,34*60);
  free(raw);
}

bool Party::swapActive(Pet &pet, bool fromBox, uint16_t index) {
  if(tradeStorageBlocked)return false;
  if(rosterReadOnly || !pendingLive.empty() || !pet.canSwapActive() ||
     index>=(fromBox?BOX_SLOTS:PARTY_SLOTS)) return false;
  PartyMon &slot=fromBox?box[index]:slots[index];
  if(slot.empty() || (slot.care[0]!=0 && slot.care[0]!=1)) return false;
  PartyMon before=slot;
  // Normalize legacy companions without writing NVS or changing player progress.
  Pet incoming; incoming.lastSeenEpoch=pet.lastSeenEpoch; incoming.reviveFrom(slot);
  if(!incoming.moveCount()) incoming.relearnFromLevel();
  pendingLive=incoming.storageSnapshot();
  slot=pet.storageSnapshot(); // empty when an egg was waiting, as before
  saveRoster(); // one verified commit owns BOTH individuals before live keys change
  if(rosterReadOnly) {slot=before;pendingLive=PartyMon();return false;}
  activeSwapBlocked=true;
  finishActiveSwap(pet);
  return true;
}

static bool sameIndividualIdentity(const PartyMon &a,const PartyMon &b) {
  // Dex/form, level, moves and training may have changed since an old journal.
  // IVs, shiny and nickname cannot change during ordinary care. Ambiguous
  // matches against banked individuals are explicitly refused below.
  return !a.empty() && !b.empty() && a.ivAtk==b.ivAtk && a.ivDef==b.ivDef &&
    a.ivSpe==b.ivSpe && a.ivHp==b.ivHp && a.shiny==b.shiny &&
    !memcmp(a.nick,b.nick,sizeof(a.nick));
}

bool Party::finishActiveSwap(Pet &pet, bool recovering) {
  if(pendingLive.empty() || rosterReadOnly) return false;
  bool retain=false;
  PartyMon stored;
  bool coherent=recovering && pet.readStoredSnapshot(stored);
  if(coherent && sameIndividualIdentity(stored,pendingLive)) {
    retain=!memcmp(&stored,&pendingLive,sizeof(stored));
    if(!retain) {
      // 3.5.1 could continue playing after a failed verification. A complete,
      // newer live save is authoritative, not the old incoming snapshot.
      for(const auto &m:slots) if(sameIndividualIdentity(m,stored)) return false;
      for(const auto &m:box) if(sameIndividualIdentity(m,stored)) return false;
      uint32_t oldAge=0,newAge=0;
      memcpy(&oldAge,pendingLive.care+20,4);memcpy(&newAge,stored.care+20,4);
      if(newAge<oldAge) return false; // uncertain provenance: preserve both copies
      retain=true;
    }
  }
  else if(coherent) {
    // A complete save with a changed nickname/identity is not evidence of a
    // torn write. Replay only when the old live individual is demonstrably
    // already banked by this transaction; otherwise keep both copies intact.
    auto bankedOldLive=[&stored](const PartyMon &m){
      uint32_t liveAge=0,bankedAge=0;
      memcpy(&liveAge,stored.care+20,4);memcpy(&bankedAge,m.care+20,4);
      return sameIndividualIdentity(stored,m) && stored.dex==m.dex && stored.form==m.form &&
        m.care[0]==1 && liveAge<=bankedAge;
    };
    bool oldLive=false;
    for(const auto &m:slots)oldLive=oldLive || bankedOldLive(m);
    for(const auto &m:box)oldLive=oldLive || bankedOldLive(m);
    if(!oldLive)return false;
  }
  if(retain) pet.saveNow(); // pet.begin() already loaded the latest moves/care/queue
  else pet.reviveFrom(pendingLive);
  if(!pet.storedIndividualMatches()) return false;
  PartyMon committed=pendingLive;
  pendingLive=PartyMon();
  saveRoster();
  if(rosterReadOnly) pendingLive=committed;
  activeSwapBlocked=!pendingLive.empty();
  return !rosterReadOnly;
}

void Party::recoverActiveSwap(Pet &pet) {
  // Read fresh storage, not the global Party RAM (backup restore may replace it).
  Party *recovery=new(std::nothrow) Party;
  if(!recovery) return;
  recovery->begin();
  if(!recovery->pendingLive.empty()) {
    recovery->finishActiveSwap(pet,true);
    party.begin();
  }
  delete recovery;
}

uint16_t Party::boxCount() const {
  uint16_t n = 0;
  for (auto &s : box)
    if (!s.empty()) n++;
  return n;
}

bool Party::selectForm(bool fromBox,uint16_t index,FormId id) {
  if(!writable())return false;
  if(rosterReadOnly || index>=(fromBox?BOX_SLOTS:PARTY_SLOTS)) return false;
  PartyMon &m=fromBox?box[index]:slots[index];
  if(!formEligible(m.dex,id,m.level)) return false;
  FormId old=m.form;m.form=id;saveRoster();
  if(rosterReadOnly) {m.form=old;return false;}
  return true;
}

bool Party::migrateLegacyOverflow() {
  if(tradeStorageBlocked)return false;
  if(rosterReadOnly) return false;
  PartyMon &oldSixth = slots[PARTY_SLOTS];
  if (oldSixth.empty()) return false;
  int i = boxFirstFree();
  if (i < 0) return false;
  box[i] = oldSixth;
  oldSixth = PartyMon();
  save();
  boxSave();
  return true;
}

int Party::boxFirstFree() const {
  for (int i = 0; i < BOX_SLOTS; i++)
    if (box[i].empty()) return i;
  return -1;
}

bool Party::boxAdd(const PartyMon &m) {
  if(tradeStorageBlocked)return false;
  if(rosterReadOnly)return false;
  migrateLegacyOverflow();
  int i = boxFirstFree();
  if (i < 0) return false;
  box[i] = m;
  if(!boxSave()){box[i]=PartyMon();return false;}
  return true;
}

void Party::boxReleaseAt(uint16_t i) {
  if(tradeStorageBlocked)return;
  if (rosterReadOnly || i >= BOX_SLOTS) return;
  PartyMon before=box[i];
  box[i] = PartyMon();
  if(!boxSave()){box[i]=before;return;}
  migrateLegacyOverflow();
}

void Party::swapPartyBox(uint8_t partyIdx, uint16_t boxIdx) {
  if(tradeStorageBlocked)return;
  if (rosterReadOnly || partyIdx >= PARTY_SLOTS || boxIdx >= BOX_SLOTS) return;
  PartyMon t = slots[partyIdx];
  slots[partyIdx] = box[boxIdx];
  box[boxIdx] = t;
  if(!save()){box[boxIdx]=slots[partyIdx];slots[partyIdx]=t;}
}

uint8_t Party::count() const {
  uint8_t n = 0;
  for (int i = 0; i < PARTY_SLOTS; i++)
    if (!slots[i].empty()) n++;
  return n;
}

int Party::firstFree() const {
  for (int i = 0; i < PARTY_SLOTS; i++)
    if (slots[i].empty()) return i;
  return -1;
}

bool Party::add(const PartyMon &m) {
  if(tradeStorageBlocked)return false;
  if(rosterReadOnly)return false;
  int i = firstFree();
  if (i < 0) return false;
  slots[i] = m;
  if(!save()){slots[i]=PartyMon();return false;}
  return true;
}

void Party::replaceAt(uint8_t i, const PartyMon &m) {
  if(tradeStorageBlocked)return;
  if (rosterReadOnly || i >= PARTY_SLOTS) return;
  PartyMon before=slots[i];
  slots[i] = m;
  if(!save())slots[i]=before;
}

void Party::releaseAt(uint8_t i) {
  if(tradeStorageBlocked)return;
  if (rosterReadOnly || i >= PARTY_SLOTS) return;
  PartyMon before=slots[i];
  slots[i] = PartyMon();
  if(!save())slots[i]=before;
}

// Mirrors calcStat() in pet.cpp: base + level + IV contribution + training.
// Kept in step with it by hand; there is no shared home for it that both the
// live pet and a frozen party member could use without dragging Pet in here.
static uint16_t calcStat(uint8_t base, uint8_t iv, uint16_t lvl, uint8_t tr) {
  return (uint16_t)base + lvl + (uint32_t)iv * lvl / 100 + tr;
}

uint16_t Party::atkOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(formDex(m.dex, m.form).bAtk, m.ivAtk, m.level, m.trAtk);
}
uint16_t Party::defOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(formDex(m.dex, m.form).bDef, m.ivDef, m.level, m.trDef);
}
uint16_t Party::speOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(formDex(m.dex, m.form).bSpe, m.ivSpe, m.level, m.trSpe);
}
uint16_t Party::vitOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(formDex(m.dex, m.form).bHp, m.ivHp, m.level, 10);
}
// Special reuses the physical IV and training, same rule as Pet::spaStat().
uint16_t Party::spaOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(formDex(m.dex, m.form).bSpA, m.ivAtk, m.level, m.trAtk);
}
uint16_t Party::spdOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(formDex(m.dex, m.form).bSpD, m.ivDef, m.level, m.trDef);
}
