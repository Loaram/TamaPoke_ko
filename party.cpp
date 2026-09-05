#include "party.h"
#include <stdlib.h>
#include <string.h>
#include "dex.h"
#include "moves.h"

Party party;

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
  size_t currentBytes = sizeof(PartyMon) * capacity;
  if (stored == currentBytes) {
    // A current-format read needs no rewrite.  If Preferences ever reports a
    // short read, keep the zero-initialised destination instead of saving that
    // failed read back over the only copy.
    prefs.getBytes(key, out, currentBytes);
    return false;
  }

  uint8_t *raw = (uint8_t *)malloc(stored);
  if (!raw) return false;
  if (prefs.getBytes(key, raw, stored) != stored) { free(raw); return false; }

  size_t stride = 0, count = 0;
  enum { CURRENT, LEGACY8, LEGACY0 } format = CURRENT;
  if (stored % sizeof(PartyMon) == 0) {
    stride = sizeof(PartyMon); count = stored / stride;
  } else if (stored % sizeof(LegacyPartyMon8) == 0) {
    format = LEGACY8; stride = sizeof(LegacyPartyMon8); count = stored / stride;
  } else if (stored % sizeof(LegacyPartyMon0) == 0) {
    format = LEGACY0; stride = sizeof(LegacyPartyMon0); count = stored / stride;
  }
  if (!stride) { free(raw); return false; }
  if (count > capacity) count = capacity;
  for (size_t i = 0; i < count; i++) {
    if (format == CURRENT) memcpy(&out[i], raw + i * stride, sizeof(PartyMon));
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
  if (loadRoster(prefs, "party", slots, PARTY_SLOTS)) save();
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
  if (loadRoster(prefs, "box", box, BOX_SLOTS)) boxSave();
  for (auto &s : box) {
    if (s.dex < 1 || s.dex > DEX_COUNT) s.dex = 0;
    s.nick[sizeof(s.nick) - 1] = 0;
    for (int i = 0; i < MOVE_SLOTS; i++) if (s.moves[i] >= MOVE_COUNT) s.moves[i] = 0;
  }
}

void Party::save() {
  prefs.putBytes("party", slots, sizeof(slots));
}

void Party::boxSave() {
  prefs.putBytes("box", box, sizeof(box));
}

uint8_t Party::boxCount() const {
  uint8_t n = 0;
  for (auto &s : box)
    if (!s.empty()) n++;
  return n;
}

int Party::boxFirstFree() const {
  for (int i = 0; i < BOX_SLOTS; i++)
    if (box[i].empty()) return i;
  return -1;
}

bool Party::boxAdd(const PartyMon &m) {
  int i = boxFirstFree();
  if (i < 0) return false;
  box[i] = m;
  boxSave();
  return true;
}

void Party::boxReleaseAt(uint8_t i) {
  if (i >= BOX_SLOTS) return;
  box[i] = PartyMon();
  boxSave();
}

void Party::swapPartyBox(uint8_t partyIdx, uint8_t boxIdx) {
  if (partyIdx >= PARTY_SLOTS || boxIdx >= BOX_SLOTS) return;
  PartyMon t = slots[partyIdx];
  slots[partyIdx] = box[boxIdx];
  box[boxIdx] = t;
  save();
  boxSave();
}

uint8_t Party::count() const {
  uint8_t n = 0;
  for (auto &s : slots)
    if (!s.empty()) n++;
  return n;
}

int Party::firstFree() const {
  for (int i = 0; i < PARTY_SLOTS; i++)
    if (slots[i].empty()) return i;
  return -1;
}

bool Party::add(const PartyMon &m) {
  int i = firstFree();
  if (i < 0) return false;
  slots[i] = m;
  save();
  return true;
}

void Party::replaceAt(uint8_t i, const PartyMon &m) {
  if (i >= PARTY_SLOTS) return;
  slots[i] = m;
  save();
}

void Party::releaseAt(uint8_t i) {
  if (i >= PARTY_SLOTS) return;
  slots[i] = PartyMon();
  save();
}

// Mirrors calcStat() in pet.cpp: base + level + IV contribution + training.
// Kept in step with it by hand; there is no shared home for it that both the
// live pet and a frozen party member could use without dragging Pet in here.
static uint16_t calcStat(uint8_t base, uint8_t iv, uint16_t lvl, uint8_t tr) {
  return (uint16_t)base + lvl + (uint32_t)iv * lvl / 100 + tr;
}

uint16_t Party::atkOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bAtk, m.ivAtk, m.level, m.trAtk);
}
uint16_t Party::defOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bDef, m.ivDef, m.level, m.trDef);
}
uint16_t Party::speOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bSpe, m.ivSpe, m.level, m.trSpe);
}
uint16_t Party::vitOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bHp, m.ivHp, m.level, 10);
}
// Special reuses the physical IV and training, same rule as Pet::spaStat().
uint16_t Party::spaOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bSpA, m.ivAtk, m.level, m.trAtk);
}
uint16_t Party::spdOf(const PartyMon &m) const {
  return m.empty() ? 0 : calcStat(DEX_TBL[m.dex].bSpD, m.ivDef, m.level, m.trDef);
}
