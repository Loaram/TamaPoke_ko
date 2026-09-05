// Move storage: learnset population, save/load round-trip, backfill of a save
// made before moves existed, and the party blob migration. Asserts against the
// real Pet/Party rather than restating their rules.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "dex.h"
#include "moves.h"
#include <cstdio>

uint32_t g_seed = 0xC0FFEE;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
bool wasPressed = false;
static uint32_t g_ms = 0;
uint32_t millis() { return g_ms; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}
static void dump(const char *tag, const MoveId *mv) {
  printf("     %s:", tag);
  for (int i = 0; i < MOVE_SLOTS; i++)
    printf(" %s", mv[i] ? MOVE_TBL[mv[i]].name : "-");
  printf("\n");
}

int main() {
  ck(sizeof(MoveId) == 2, "expanded move IDs use 16-bit storage");
  ck(MOVE_COUNT == 696, "the complete natural move table contains 695 moves plus NONE");

  // Audit the generated table itself before exercising individual creatures.
  // Every appended move must be reachable from at least one species, and every
  // row must be safe to use as a MOVE_TBL index on device.
  bool referenced[MOVE_COUNT] = { false };
  int badRows = 0;
  for (MoveId mv = 1; mv < MOVE_COUNT; mv++) {
    const MoveEntry &m = MOVE_TBL[mv];
    if (!m.name || !m.name[0] || m.type >= TYPE_COUNT || m.cat > MC_STATUS ||
        m.effect > EF_ALWAYS_CRIT || m.target > TG_FOE || m.ailment > AIL_CONFUSE)
      badRows++;
  }
  for (int16_t dex = 1; dex <= DEX_COUNT; dex++)
    for (uint8_t i = 0; i < learnCount(dex); i++) {
      MoveId mv = learnMove(dex, i);
      if (!mv || mv >= MOVE_COUNT) badRows++;
      else referenced[mv] = true;
    }
  int orphanedExpanded = 0;
  for (MoveId mv = 142; mv < MOVE_COUNT; mv++)
    if (!referenced[mv]) orphanedExpanded++;
  ck(badRows == 0, "all generated move and learnset rows are valid");
  ck(orphanedExpanded == 0, "every newly added move belongs to a Pokemon learnset");

  int badEvolutionRows = 0;
  for (uint16_t i = 0; i < EVOLUTION_LEARN_COUNT; i++) {
    const EvolutionLearnEntry &e = EVOLUTION_LEARN_TBL[i];
    bool alsoRelearnable = false;
    if (e.dex < 1 || e.dex > DEX_COUNT || !e.move || e.move >= MOVE_COUNT) {
      badEvolutionRows++;
      continue;
    }
    for (uint8_t k = 0; k < learnCount(e.dex); k++)
      if (learnMove(e.dex, k) == e.move && learnLevel(e.dex, k) == 1)
        alsoRelearnable = true;
    if (!alsoRelearnable) badEvolutionRows++;
  }
  ck(EVOLUTION_LEARN_COUNT == 253 && badEvolutionRows == 0,
     "all 253 evolution moves are valid and remain relearnable");

  bool bulbaSeedBomb = false, venusInitial = false, venusEvolution = false;
  for (uint8_t i = 0; i < learnCount(1); i++)
    if (learnMove(1, i) == MV_SEED_BOMB && learnLevel(1, i) == 18)
      bulbaSeedBomb = true;
  for (uint8_t i = 0; i < learnCount(3); i++)
    if (learnMove(3, i) == MV_PETAL_BLIZZARD && learnLevel(3, i) == 1)
      venusInitial = true;
  for (uint8_t i = 0; i < evolutionMoveCount(3); i++)
    if (evolutionMove(3, i) == MV_PETAL_BLIZZARD) venusEvolution = true;
  ck(MV_SEED_BOMB > 255 && bulbaSeedBomb,
     "a newly added 16-bit level-up move keeps its exact level");
  ck(MV_PETAL_BLIZZARD > 255 && venusInitial && venusEvolution,
     "a newly added 16-bit initial/evolution move is present in both paths");

  // --- a level-100 Charizard should know four real, distinct moves
  Pet p;
  p.dbgHatchAs(6, false);
  p.ageMinutes = 5940;                 // level 100
  p.relearnFromLevel();
  dump("Charizard L100", p.moves);
  ck(p.moveCount() == 4, "L100 Charizard knows 4 moves");
  bool distinct = true, valid = true;
  for (int i = 0; i < MOVE_SLOTS; i++) {
    if (p.moves[i] >= MOVE_COUNT) valid = false;
    for (int j = i + 1; j < MOVE_SLOTS; j++)
      if (p.moves[i] && p.moves[i] == p.moves[j]) distinct = false;
  }
  ck(distinct, "no duplicate moves");
  ck(valid, "every move is a valid MOVE_TBL index");

  // --- a freshly hatched pet knows fewer, and never more than it should
  Pet baby;
  baby.dbgHatchAs(6, false);
  baby.ageMinutes = 0;                 // level 1
  baby.relearnFromLevel();
  dump("Charizard L1  ", baby.moves);
  ck(baby.moveCount() >= 1, "a level 1 pet still knows at least one move");
  ck(baby.moveCount() <= 4, "never exceeds 4 slots");

  // every known move must actually be learnable at or below its level
  bool legal = true;
  for (int i = 0; i < MOVE_SLOTS; i++) {
    if (!baby.moves[i]) continue;
    bool found = false;
    for (uint8_t k = 0; k < learnCount(6); k++)
      if (learnMove(6, k) == baby.moves[i] && learnLevel(6, k) <= 1) found = true;
    if (!found) legal = false;
  }
  ck(legal, "a level 1 pet knows nothing it has not learned yet");

  // Wimpod's original filtered set kept only TMs, all gated until level 40.
  // Its real level-1 STRUGGLE BUG must survive the compact move-table filter.
  Pet wimpod;
  wimpod.dbgHatchAs(767, false);
  wimpod.ageMinutes = 0;
  wimpod.relearnFromLevel();
  dump("Wimpod L1    ", wimpod.moves);
  ck(wimpod.moveCount() >= 1, "a level 1 Wimpod has a usable move");
  bool struggleBug = false;
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (wimpod.moves[i] && !strcmp(MOVE_TBL[wimpod.moves[i]].name, "STRUGGLE BUG"))
      struggleBug = true;
  ck(struggleBug, "Wimpod starts with STRUGGLE BUG");

  ck(MOVE_TBL[MV_DRUM_BEATING].effect == EF_STAGE &&
     MOVE_TBL[MV_DRUM_BEATING].statMask == ST_SPE &&
     MOVE_TBL[MV_DRUM_BEATING].stages == -1 &&
     MOVE_TBL[MV_DRUM_BEATING].target == TG_FOE,
     "DRUM BEATING lowers the foe's speed");
  ck(MOVE_TBL[MV_PYRO_BALL].ailment == AIL_BURN &&
     MOVE_TBL[MV_PYRO_BALL].ailChance == 10,
     "PYRO BALL keeps its burn chance");
  ck(MOVE_TBL[MV_FLOWER_TRICK].effect == EF_ALWAYS_CRIT &&
     MOVE_TBL[MV_FLOWER_TRICK].acc == 0,
     "FLOWER TRICK cannot miss and always crits");
  Combatant flowerUser, flowerTarget;
  flowerUser.dex = 908; flowerUser.level = 50; flowerUser.maxHp = flowerUser.hp = 999;
  flowerTarget.dex = 7; flowerTarget.level = 50; flowerTarget.maxHp = flowerTarget.hp = 999;
  for (uint8_t i = 0; i < SI_COUNT; i++)
    flowerUser.base[i] = flowerTarget.base[i] = 100;
  TurnLog flowerLog;
  battleAct(flowerUser, flowerTarget, MV_FLOWER_TRICK, flowerLog);
  ck(!flowerLog.missed && flowerLog.crit && flowerLog.damage > 0,
     "FLOWER TRICK resolves as a guaranteed critical hit");
  ck(MOVE_TBL[MV_TORCH_SONG].effect == EF_STAGE &&
     MOVE_TBL[MV_TORCH_SONG].statMask == ST_SPA &&
     MOVE_TBL[MV_TORCH_SONG].stages == 1 &&
     MOVE_TBL[MV_TORCH_SONG].target == TG_SELF,
     "TORCH SONG raises the user's special attack");
  ck(MOVE_TBL[MV_AQUA_STEP].effect == EF_STAGE &&
     MOVE_TBL[MV_AQUA_STEP].statMask == ST_SPE &&
     MOVE_TBL[MV_AQUA_STEP].stages == 1 &&
     MOVE_TBL[MV_AQUA_STEP].target == TG_SELF,
     "AQUA STEP raises the user's speed");

  // Evolution-only signature moves are not ordinary level gates. The old form
  // has already consumed the evolution level before evolve() changes species,
  // so these must be attached to the resulting form explicitly.
  struct EvoMoveCase { int16_t from, to; MoveId move; const char *name; };
  const EvoMoveCase evoMoves[] = {
    { 811, 812, MV_DRUM_BEATING, "Rillaboom learns DRUM BEATING" },
    { 814, 815, MV_PYRO_BALL, "Cinderace learns PYRO BALL" },
    { 817, 818, MV_SNIPE_SHOT, "Inteleon learns SNIPE SHOT" },
    { 907, 908, MV_FLOWER_TRICK, "Meowscarada learns FLOWER TRICK" },
    { 910, 911, MV_TORCH_SONG, "Skeledirge learns TORCH SONG" },
    { 913, 914, MV_AQUA_STEP, "Quaquaval learns AQUA STEP" },
  };
  for (const EvoMoveCase &tc : evoMoves) {
    Pet evolved;
    evolved.dbgHatchAs(tc.from, false);
    evolved.ageMinutes = (DEX_TBL[tc.from].evolveLevel - 1) * MINUTES_PER_LEVEL;
    evolved.fullness = evolved.joy = evolved.energy = evolved.hygiene = 100;
    evolved.lastLearnLevel = evolved.level(); // the old form handled this level
    evolved.moves[0] = MV_TACKLE;
    evolved.moves[1] = MV_SCRATCH;
    evolved.moves[2] = MV_POUND;
    evolved.moves[3] = MV_SWIFT;              // force the replace prompt
    evolved.learnQCount = 0;
    evolved.evolve();
    ck(evolved.speciesId == tc.to, "evolution reaches the expected final form");
    ck(evolved.hasLearnOffer() && evolved.learnOffer() == tc.move, tc.name);
    evolved.acceptLearn(0);
    ck(evolved.knowsMove(tc.move), "accepting the evolution offer stores the move");
  }

  Pet meowscarada;
  meowscarada.dbgHatchAs(907, false);
  meowscarada.ageMinutes = (DEX_TBL[907].evolveLevel - 1) * MINUTES_PER_LEVEL;
  meowscarada.fullness = meowscarada.joy = meowscarada.energy = meowscarada.hygiene = 100;
  meowscarada.lastLearnLevel = meowscarada.level();
  meowscarada.moves[0] = MV_SCRATCH;
  meowscarada.moves[1] = meowscarada.moves[2] = meowscarada.moves[3] = MV_NONE;
  meowscarada.learnQCount = 0;
  meowscarada.evolve();
  ck(meowscarada.knowsMove(MV_FLOWER_TRICK) && !meowscarada.hasLearnOffer(),
     "Meowscarada fills a free slot with FLOWER TRICK immediately");

  Pet declinedFlower;
  declinedFlower.dbgHatchAs(907, false);
  declinedFlower.ageMinutes = (DEX_TBL[907].evolveLevel - 1) * MINUTES_PER_LEVEL;
  declinedFlower.fullness = declinedFlower.joy = declinedFlower.energy = declinedFlower.hygiene = 100;
  declinedFlower.lastLearnLevel = declinedFlower.level();
  declinedFlower.moves[0] = MV_TACKLE;
  declinedFlower.moves[1] = MV_SCRATCH;
  declinedFlower.moves[2] = MV_POUND;
  declinedFlower.moves[3] = MV_SWIFT;
  declinedFlower.learnQCount = 0;
  declinedFlower.evolve();
  declinedFlower.declineLearn();
  MoveId recovery[8] = { 0 };
  uint8_t recoveryCount = declinedFlower.pendingLearnables(recovery, 8);
  bool recoversFlower = false;
  for (uint8_t i = 0; i < recoveryCount; i++)
    if (recovery[i] == MV_FLOWER_TRICK) recoversFlower = true;
  ck(recoversFlower, "a declined FLOWER TRICK remains available to relearn");

  // Full-dex guard: every one of the 1025 table entries can be reached through
  // hatching, evolution, party restore or the debug tools. None may arrive at
  // battle with an empty or invalid set, even when its signature move is not
  // part of the compact move table.
  int emptyL1 = 0, emptyL100 = 0, invalidAny = 0;
  for (int16_t dex = 1; dex <= DEX_COUNT; dex++) {
    for (int lvl : { 1, 100 }) {
      Pet all;
      all.dbgHatchAs(dex, false);
      all.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
      all.relearnFromLevel();
      if (!all.moveCount()) (lvl == 1 ? emptyL1 : emptyL100)++;
      for (int s = 0; s < MOVE_SLOTS; s++)
        if (all.moves[s] >= MOVE_COUNT) invalidAny++;
    }
  }
  ck(emptyL1 == 0, "all 1025 species have a usable move at level 1");
  ck(emptyL100 == 0, "all 1025 species have a usable move at level 100");
  ck(invalidAny == 0, "all full-dex move slots stay inside MOVE_TBL");

  Pet magikarp;
  magikarp.dbgHatchAs(129, false);
  magikarp.ageMinutes = 0;
  magikarp.relearnFromLevel();
  bool magikarpStruggle = false, magikarpSplash = false;
  for (int s = 0; s < MOVE_SLOTS; s++) {
    magikarpStruggle |= magikarp.moves[s] == MV_STRUGGLE;
    magikarpSplash |= magikarp.moves[s] == MV_SPLASH;
  }
  ck(magikarpSplash && magikarpStruggle,
     "a status-only level 1 species gets its real move plus battle STRUGGLE");
  magikarp.lastLearnLevel = 1;
  magikarp.ageMinutes = 14UL * MINUTES_PER_LEVEL;  // level 15: TACKLE
  magikarp.checkLearnGates();
  bool stillStruggling = false, learnedTackle = false;
  for (int s = 0; s < MOVE_SLOTS; s++) {
    stillStruggling |= magikarp.moves[s] == MV_STRUGGLE;
    learnedTackle |= magikarp.moves[s] == MV_TACKLE;
  }
  ck(learnedTackle && !stillStruggling,
     "the first real move replaces temporary STRUGGLE");

  Preferences oldWimpod;
  oldWimpod.begin("tamapoke", false);
  oldWimpod.clear();
  oldWimpod.putBool("init", true);
  oldWimpod.putShort("dexn", 767);
  oldWimpod.putUInt("age", 0);
  uint8_t noMoves[MOVE_SLOTS] = { 0, 0, 0, 0 };
  oldWimpod.putBytes("mvs", noMoves, sizeof(noMoves));
  oldWimpod.putUChar("mvlv", 1);  // the released build saved this bad state
  oldWimpod.end();
  Pet repairedWimpod;
  repairedWimpod.begin();
  ck(repairedWimpod.moveCount() >= 1,
     "an existing empty Wimpod save is repaired on load");

  // The live pet was also one-byte-per-move in every released save.  Prove
  // that both the old byte layout and a new ID above 255 survive reload.
  {
    Preferences oldPet;
    oldPet.begin("tamapoke", false);
    oldPet.clear();
    oldPet.putBool("init", true);
    oldPet.putShort("dexn", 6);
    uint8_t oldIds[MOVE_SLOTS] = { MV_TACKLE, MV_SCRATCH, MV_EMBER, MV_SURF };
    oldPet.putBytes("mvs", oldIds, sizeof(oldIds));
    oldPet.end();
    Pet migrated;
    migrated.begin();
    ck(migrated.moves[0] == MV_TACKLE && migrated.moves[1] == MV_SCRATCH &&
       migrated.moves[2] == MV_EMBER && migrated.moves[3] == MV_SURF,
       "released one-byte live moves migrate without IDs merging together");

    migrated.moves[0] = MV_PETAL_BLIZZARD;
    migrated.rename("WIDEID");             // persists the complete pet
    Pet reloaded;
    reloaded.begin();
    ck(reloaded.moves[0] == MV_PETAL_BLIZZARD,
       "a move ID above 255 survives the live-save round trip");
  }

  // --- a default set must be mostly attacks, not a pile of stat-lowering
  // status moves. This is what caught GROWL/LEER on a level 100 Charizard.
  for (int dex : { 6, 9, 3, 65, 68, 25, 143, 150 }) {
    Pet q;
    q.dbgHatchAs(dex, false);
    q.ageMinutes = 5940;
    q.relearnFromLevel();
    int atk = 0, stab = 0;
    for (int i = 0; i < MOVE_SLOTS; i++) {
      if (!q.moves[i]) continue;
      const MoveEntry &m = MOVE_TBL[q.moves[i]];
      if (m.cat != MC_STATUS) atk++;
      if (m.type == DEX_TBL[dex].type1 || m.type == DEX_TBL[dex].type2) stab++;
    }
    printf("     %-11s atk=%d stab=%d :", DEX_TBL[dex].name, atk, stab);
    for (int i = 0; i < MOVE_SLOTS; i++)
      printf(" %s", q.moves[i] ? MOVE_TBL[q.moves[i]].name : "-");
    printf("\n");
    if (atk < 3) { printf("FAIL  %s has fewer than 3 attacks\n", DEX_TBL[dex].name); bad++; }
    if (stab < 1) { printf("FAIL  %s has no same-type move\n", DEX_TBL[dex].name); bad++; }
  }
  ck(true, "default sets are attack-led with STAB (see above)");

  // --- learn candidates are offered, and never ones already known
  MoveId cand[8];
  uint8_t n = p.pendingLearnables(cand, 8);
  bool alreadyKnown = false;
  for (uint8_t i = 0; i < n; i++)
    if (p.knowsMove(cand[i])) alreadyKnown = true;
  printf("     L100 learnables not yet known: %u\n", n);
  ck(!alreadyKnown, "pendingLearnables never offers a known move");

  // --- party migration: a blob written in the OLD layout (no moves[]) must
  // survive, with slots still aligned. This is the case that silently
  // corrupted the party if migrated by a plain getBytes().
  const size_t oldStride = sizeof(PartyMon) - sizeof(MoveId) * MOVE_SLOTS;
  uint8_t legacy[PARTY_SLOTS * (sizeof(PartyMon))];
  for (int i = 0; i < PARTY_SLOTS; i++) {
    PartyMon m;
    m.dex = 1 + i * 20;                // 1, 21, 41, 61, 81, 101
    m.level = 40 + i;
    m.ivAtk = m.ivDef = m.ivSpe = m.ivHp = 20;
    snprintf(m.nick, sizeof(m.nick), "OLD%d", i);
    memcpy(legacy + i * oldStride, &m, oldStride);   // old records, old stride
  }
  Preferences seed;
  seed.begin("tamapoke", false);
  seed.putBytes("party", legacy, PARTY_SLOTS * oldStride);
  seed.end();

  Party pty;
  pty.begin();
  bool aligned = true;
  for (int i = 0; i < PARTY_SLOTS; i++) {
    if (pty.slots[i].dex != 1 + i * 20 || pty.slots[i].level != 40 + i) aligned = false;
    printf("     slot %d: dex=%d lvl=%u nick=%s\n",
           i, pty.slots[i].dex, pty.slots[i].level, pty.slots[i].nick);
  }
  ck(aligned, "legacy party blob migrates with slots still aligned");
  ck(pty.count() == PARTY_SLOTS, "all 5 legacy members survive");

  // The MOVE PICKER had its own gate and so its own opinion: it checked
  // learnLevel() alone, and a TM is stored as level 0, so a level 22 Charmeleon
  // was offered FIRE BLAST (110 power). Found by hand on the board, exactly
  // like the level 1 Squirtle holding SURF -- the same bug in the one path that
  // fix never reached. moveUnlockLevel() is the single answer now.
  {
    int zero = 0, tooEarly = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++)
      for (uint8_t i = 0; i < learnCount(d); i++) {
        uint8_t at = moveUnlockLevel(d, i);
        if (at == 0) zero++;
        if (learnLevel(d, i) == 0 && at < 20) tooEarly++;
      }
    ck(zero == 0, "no learnset entry anywhere unlocks at level 0");
    ck(tooEarly == 0, "and no TM is reachable before a creature is built");

    const int16_t CHARMELEON = 5;
    bool early = false, late = false, ember1 = false, flame30 = false;
    for (uint8_t i = 0; i < learnCount(CHARMELEON); i++) {
      MoveId mv = learnMove(CHARMELEON, i);
      if (mv >= MOVE_COUNT) continue;
      uint8_t at = moveUnlockLevel(CHARMELEON, i);
      if (!strcmp(MOVE_TBL[mv].name, "FIRE BLAST")) { early = at <= 22; late = at >= 40; }
      if (!strcmp(MOVE_TBL[mv].name, "EMBER")) ember1 = (at == 1);
      if (!strcmp(MOVE_TBL[mv].name, "FLAMETHROWER")) flame30 = (at == 30);
    }
    ck(!early, "a level 22 Charmeleon is NOT offered FIRE BLAST");
    ck(late, "it waits for the TM level like every other TM");
    ck(ember1 && flame30,
       "while current level-up moves keep their exact levels (EMBER 1, FLAMETHROWER 30)");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
