#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "move_id.h"
#include "forms.h"

// The party: pets that finished their life and were kept, rather than being
// dissolved into a single Pokedex bit like every previous ending did.
//
// Only the two endings the player CHOOSES bank a pet -- farewell and release.
// A runaway does not: it is the game's one punishing outcome, and letting a
// neglected pet come back as a team member would take the sting out of it.
// Five banked members plus the live pet make the six-Pokemon battle limit.
// Keep a sixth PHYSICAL record so an older save is never truncated before its
// former last member can be moved safely into the box.
#define PARTY_SLOTS 5
#define PARTY_STORAGE_SLOTS 6
// The box: storage beyond the six that fight. Deliberately a SEPARATE NVS key
// rather than a bigger party blob -- growing that blob would change its stride
// and the length-based migration in begin() cannot tell a stride change from a
// slot-count change, so an existing party would be read back misaligned. A new
// key is purely additive and cannot corrupt anything.
#define BOX_SLOTS 300
#define BOX_PER_PAGE 6
#define BOX_PAGES ((BOX_SLOTS + BOX_PER_PAGE - 1) / BOX_PER_PAGE)
#define MOVE_SLOTS 4    // the same four every trainer gets in the real games

// Stored individuals do not age. All can grow when active; old retired/caught
// records retain companion ending protection and their own care state.
struct PartyMon {
  int16_t dex = 0;      // Pokedex number, 0 = empty slot
  uint16_t level = 1;   // paused while banked; resumes when brought to the home slot
  uint16_t medals = 0;  // what it earned in life
  uint8_t ivAtk = 0, ivDef = 0, ivSpe = 0, ivHp = 0;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  uint8_t shiny = 0;
  char nick[12] = "";
  // Frozen with everything else: the moves you chose while it was alive are
  // what it fights with forever. 0 = empty slot (MOVE_TBL[0] is the "-" filler).
  // Appended at the END of the struct on purpose -- Party::begin() migrates
  // older, shorter blobs by length, and that only works if nothing moved.
  MoveId moves[MOVE_SLOTS] = { 0, 0, 0, 0 };
  // Only runtime extends. Legacy NVS records stay explicitly 34 bytes.
  FormId form = 0;
  uint8_t care[36] = {}; // versioned individual care state; zero = legacy companion

  bool empty() const { return dex < 1; }
};

#define PARTY_RECORD_BYTES 72
#define PARTY_ROSTER_BYTES (12 + PARTY_RECORD_BYTES * (PARTY_STORAGE_SLOTS + BOX_SLOTS + 1))
class Pet;
extern bool tradeStorageBlocked;
enum BoxSortOrder : uint8_t { BOX_SORT_NAME, BOX_SORT_DEX, BOX_SORT_LEVEL };

class Party {
public:
  PartyMon slots[PARTY_STORAGE_SLOTS];
  PartyMon box[BOX_SLOTS];

  void begin();                 // load from NVS
  uint8_t count() const;
  bool isFull() const { return count() >= PARTY_SLOTS; }
  int firstFree() const;        // index of the first empty slot, -1 if full
  bool add(const PartyMon &m);  // into the first free slot; false if full
  void replaceAt(uint8_t i, const PartyMon &m);
  void releaseAt(uint8_t i);    // free a slot again
  bool save();
  bool writable() const { return !rosterReadOnly && !tradeStorageBlocked; }
  bool hasEndedMon(const PartyMon &m) const;
  uint16_t boxCount() const;
  int boxFirstFree() const;
  bool boxAdd(const PartyMon &m);     // into the first free box slot
  void boxReleaseAt(uint16_t i);
  bool boxSave();
  bool sortBox(BoxSortOrder order); // one-shot, all pages; leaves party/live pet intact
  // Swaps a party slot with a box slot. Either may be empty, so this doubles as
  // deposit and withdraw rather than needing three separate operations.
  void swapPartyBox(uint8_t partyIdx, uint16_t boxIdx);
  bool selectForm(bool fromBox, uint16_t index, FormId id);
  bool swapActive(Pet &pet, bool fromBox, uint16_t index);
  static void recoverActiveSwap(Pet &pet);

  // combat stats of a party member, same formula as the live pet's
  uint16_t atkOf(const PartyMon &m) const;
  uint16_t defOf(const PartyMon &m) const;
  uint16_t speOf(const PartyMon &m) const;
  uint16_t vitOf(const PartyMon &m) const;
  uint16_t spaOf(const PartyMon &m) const;
  uint16_t spdOf(const PartyMon &m) const;

private:
  bool migrateLegacyOverflow();
  void saveRoster();
  bool rosterReadOnly = false;
  PartyMon pendingLive; // committed with the outgoing roster, replayed after interruption
  bool finishActiveSwap(Pet &pet);
  Preferences prefs;
};

extern Party party;
