#include "pet.h"
#include "avatars.h"
#include "dex.h"
#include "moves.h"
#include "form_moves.h"
#include "noart.h"   // speciesHasArt(): the egg pool skips what cannot be drawn
#include "audio.h"
#include "egg_rewards.h"
#include "pokedex_progress.h"

// Avoid modulo bias in the emulator's 16-bit PRNG as well as on devices.
static uint16_t rollEggOdds(uint16_t range) {
  uint32_t limit = 65536UL / range * range, value;
  do { value = random(65536L); } while (value >= limit);
  return value % range;
}

// Reads a blob that may be LONGER than the array we are reading it into.
//
// Preferences::getBytes reads the stored length FIRST and, if it exceeds the
// caller's buffer, logs and returns 0 WITHOUT COPYING ANYTHING:
//
//     if (len > maxLen) { log_e("not enough space in buffer"); return 0; }
//
// Growing the dex was always safe -- a short blob lands in the front of a bigger
// array and the rest keeps its zero initialiser, which is what every migration
// here relies on. SHRINKING was not: flashing a build with a smaller DEX_COUNT,
// GYM_REGIONS or REGION_COUNT over a newer save left dexReg, dexShinyReg, the
// badge arrays and eggByRegion entirely ZERO. The creature survived, being all
// scalars, while the Pokedex and every badge past Kanto quietly vanished --
// which from the player's side is their game rolling back.
//
// The PREFIX is the right thing to keep: dex bit n means the same species
// whatever the table grew to afterwards, and region n is the same region.
// Only the tail we have no room for is dropped, which is data about content
// this build does not have anyway.
//
// The emulator's Preferences stub used to truncate rather than refuse, so no
// test could see any of this -- see tools/emu/Preferences.h.
static void loadBlob(Preferences &p, const char *key, void *dst, size_t n) {
  size_t have = p.getBytesLength(key);
  if (!have) return;                       // absent: keep the initialiser
  if (have <= n) { p.getBytes(key, dst, n); return; }
  uint8_t *tmp = (uint8_t *)malloc(have);
  if (!tmp) return;                        // rather no read than a half one
  if (p.getBytes(key, tmp, have) == have) memcpy(dst, tmp, n);
  free(tmp);
}

void Pet::begin() {
  prefs.begin("tamapoke", false);
  opened = true;
  lastSeenEpoch = prefs.getUInt("seen", 0);
  farewellQuota = prefs.getUInt("byeQuota", 0);
  endedKind=CER_NONE;endedMon=PartyMon();ceremony=CER_NONE;
  // Zeroed BEFORE the branch below, not inside load(): getBytes() leaves its
  // destination untouched when the key is missing, and the fresh-install path
  // returns without ever calling load(). Without this a begin() after a factory
  // reset keeps the old Pokedex alive in RAM -- the firmware reboots on WIPE so
  // it never showed there, but anything calling begin() twice would see it, and
  // Party::begin() already guards the same way for the same reason.
  memset(badgesX, 0, sizeof(badgesX));
  memset(badgesHardX, 0, sizeof(badgesHardX));
  memset(dexReg, 0, sizeof(dexReg));
  memset(dexShinyReg, 0, sizeof(dexShinyReg));
  for (int i = 0; i < REGION_COUNT; i++) eggByRegion[i] = 0;
  if (!prefs.getBool("init", false)) {
    prefs.putBool("init", true);
    newEgg();
  } else {
    load();
  }
  PartyMon waiting;
  if(prefs.getBytesLength("endWait")==sizeof(waiting) &&
     prefs.getBytes("endWait",&waiting,sizeof(waiting))==sizeof(waiting) &&
     waiting.dex>=1 && waiting.dex<=DEX_COUNT) {
    endedMon=waiting;endedMon.nick[sizeof(endedMon.nick)-1]=0;
    for(auto &m:endedMon.moves)if(m>=MOVE_COUNT)m=0;
    endedKind=lastEnd==CER_RELEASE?CER_RELEASE:CER_FAREWELL;
    uint32_t ticket=0;memcpy(&ticket,waiting.care+32,4);
    // A crash anywhere between handoff and the completed egg save resumes here.
    if(ticket && prefs.getUInt("endEgg",0)!=ticket)newEgg();
  }
  Party::recoverActiveSwap(*this);
#if defined(TAMAPOKE_FULL_DEX) || defined(TAMAPOKE_FULL_SHINY)
  // Developer-only encyclopedia builds.  Unlock every normal entry in memory;
  // each emulator variant gets a separate NVS file, so a player's regular
  // progress is never overwritten by a test catalogue.
  memset(dexReg, 0xFF, sizeof(dexReg));
  if (DEX_COUNT & 7)
    dexReg[sizeof(dexReg) - 1] = (uint8_t)((1u << (DEX_COUNT & 7)) - 1u);
#endif
#ifdef TAMAPOKE_FULL_SHINY
  memset(dexShinyReg, 0xFF, sizeof(dexShinyReg));
  if (DEX_COUNT & 7)
    dexShinyReg[sizeof(dexShinyReg) - 1] = (uint8_t)((1u << (DEX_COUNT & 7)) - 1u);
#endif
  lastTick = millis();
}

void Pet::newEgg() {
  ceremony = CER_NONE;
  neglectTicks = 0;
  weight = 0;
  speciesId = -1;
  form = 0;
  prevSpeciesId = -1;
  for (int i = 0; i < REGION_COUNT; i++) eggByRegion[i] = 0;
  // Decide shininess before selecting the species: shiny eggs must not be
  // restricted to families missing from the normal Pokedex. Roll only here,
  // never on reload or a companion swap; existing eggs retain their result.
  eggShiny = (rollEggOdds(EggRewards::SHINY_ROLL) < eggShinyWeight());
  eggTarget = pickEggSpecies();  // especie oculta segun rareza y pokedex
  eggByRegion[region % REGION_COUNT] = eggTarget;
  starterPick = (registeredCount() == 0);  // primera partida: el jugador elige inicial
  // Early retirement no longer delays the next creature's evolution.
  evoPen = 0;
  retirePending = false;
  eggTaps = 0;
  fullness = 80;
  joy = 80;
  energy = 80;
  hygiene = 100;
  poops = 0;
  ageMinutes = 0;
  careMistakes = 0;
  mistakeCooldown = 0;
  sleeping = false;
  frozen = false;
  save();
  if(opened && endedKind!=CER_NONE) {
    uint32_t ticket=0;memcpy(&ticket,endedMon.care+32,4);
    if(prefs.getShort("dexn",0)==-1 && prefs.getShort("eggT2",0)==eggTarget)
      prefs.putUInt("endEgg",ticket);
  }
}

// progresion offline: el tiempo paso aunque estuviera apagado, pero con
// piedad — las barras bajan con suelo en 15 (vuelve hambriento, no muerto),
// sin descuidos ni escapadas en ausencia
static uint8_t dropTo(uint8_t v, uint8_t d, uint8_t fl) {
  if (v <= fl) return v;
  return (v - fl > d) ? v - d : fl;
}

void Pet::setClock(uint32_t nowEpoch) {
  lastSeenEpoch = nowEpoch;
#ifndef ANDROID
  deviceClockRemainder = 0;
#endif
  if (nowEpoch) save();  // persiste ya: un corte de luz no pierde la referencia
}

void Pet::syncClock(uint32_t nowEpoch) {
  if (activeSwapBlocked) return; // setup must not advance or rewrite an unresolved swap.
  uint32_t seen = prefs.getUInt("seen", 0);
  lastSeenEpoch = nowEpoch;
  deviceClockRemainder = 0;
  if (nowEpoch == 0) return;
  uint32_t mins = (seen && nowEpoch > seen) ? (nowEpoch - seen) / 60 : 0;
  applyOfflineMinutes(mins);
  save();
}

void Pet::applyOfflineMinutes(uint32_t mins) {
  if (activeSwapBlocked || !mins || ceremony != CER_NONE || starterPick) return;
  if (mins > 14UL * 24 * 60) mins = 14UL * 24 * 60;  // tope: 2 semanas

  for (uint32_t i = 0; i < mins; i++) {
    ageMinutes++; // Active companions grow too; stored records never tick.
    if (isEgg()) {
      if (ageMinutes >= 3) hatch();  // eclosiona en tu ausencia
      continue;
    }
    if (sleeping) {  // descanso: baja lento y con suelo, igual que en vivo
      energy = clamp100(energy + SLEEP_ENERGY_PER_MIN);
      if (ageMinutes % 2 == 0) {
        fullness = dropTo(fullness, 1, 30);
        joy = dropTo(joy, 1, 35);
      }
      if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
      continue;
    }
    fullness = dropTo(fullness, 2, 15);
    energy = dropTo(energy, 1, 15);
    hygiene = dropTo(hygiene, 1, 15);
    joy = dropTo(joy, 1, 15);
  }
  if (!isEgg()) {
    if (!sleeping) {  // durmiendo no ensucia
      uint8_t p = poops + mins / 240;
      poops = p > 3 ? 3 : p;
    }
    // la evolucion NO se aplica offline: queda lista y la dispara el usuario
    // tocando al bicho cuando vuelve (para que vea la transformacion)
  }
  Serial.printf("offline: %u min aplicados (nv.%u)\n", mins, level());
}

void Pet::resumeProgressClock(uint32_t nowMs, uint32_t localEpoch, uint32_t utcEpoch) {
  if (activeSwapBlocked) return;
  // Called only after verified recovery. Time spent in the blocking overlay
  // must not be replayed as active care on the next Android frame.
  lastTick = nowMs;
  deviceClockRemainder = 0;
  deviceProgressEpoch = utcEpoch;
  if (localEpoch) lastSeenEpoch = localEpoch;
  save();
}

void Pet::update(uint32_t nowMs) {
  if(activeSwapBlocked) return;
  // fin de ceremonia: la criatura se va y queda un huevo nuevo
  if (ceremony != CER_NONE && millis() > ceremonyUntil) {
    if(snapshotForParty())newEgg();
    return;
  }
  while (nowMs - lastTick >= PET_TICK_MS) {
    lastTick += PET_TICK_MS;
    tick();
  }
}

void Pet::updateDeviceClock(uint32_t nowMs, uint32_t localEpoch, uint32_t utcEpoch,
                            bool offline) {
  if(activeSwapBlocked) return;
  // Animations still use the monotonic clock; only care/growth minutes follow
  // Android's user-visible device clock.
  if (ceremony != CER_NONE && millis() > ceremonyUntil) {
    if(snapshotForParty())newEgg();
    lastTick = nowMs;
    return;
  }
  if (!localEpoch || !utcEpoch) {
    update(nowMs);  // defensive fallback if Android ever supplies no wall time
    return;
  }
  lastSeenEpoch = localEpoch;  // display, bedtime and care-day use device local time
  if (!deviceProgressEpoch || utcEpoch < deviceProgressEpoch) {
    // A timezone/automatic-time correction backwards must never become an
    // unsigned multi-year catch-up. Rebase and continue from the new setting.
    deviceProgressEpoch = utcEpoch;
    deviceClockRemainder = 0;
    lastTick = nowMs;
    return;
  }

  uint32_t elapsed = utcEpoch - deviceProgressEpoch;
  deviceProgressEpoch = utcEpoch;
  lastTick = nowMs;          // do not later replay the same pause via millis()
  uint64_t accumulated = (uint64_t)deviceClockRemainder + elapsed;
  uint32_t mins = (uint32_t)(accumulated / 60);
  deviceClockRemainder = (uint32_t)(accumulated % 60);
  if (mins > 14UL * 24 * 60) mins = 14UL * 24 * 60;
  if (offline) applyOfflineMinutes(mins);
  else while (mins--) tick();
}

void Pet::tick() {
  if(activeSwapBlocked)return;
  if (ceremony != CER_NONE) return;  // el tiempo se detiene en la despedida
  if (starterPick) return;  // la partida no empieza hasta elegir inicial: si el
                            // tiempo corriera aqui, el huevo eclosionaria solo a
                            // los 3 min con la especie sorteada y se perderia la
                            // eleccion del jugador
  ageMinutes++; // Companion protection no longer prevents active growth.

  if (isEgg()) {
    if (ageMinutes >= 3) hatch();  // si no lo tocas, eclosiona solo a los 3 min
    return;
  }

  applyAutoSleep();   // put down at 21:00 still goes to bed at 22:00


  // el sueño es descanso: la energia se recupera y las necesidades bajan MUCHO
  // mas lento que despierto y con suelo (amanece pidiendo algo de mimo, no a
  // cero, sin descuidos ni escapadas). despierto: comida -2/min, hig/joy -1/min.
  // El peso aun se quema y el descanso cuenta para la DEF (ver defTick).
  if (sleeping) {
    energy = clamp100(energy + SLEEP_ENERGY_PER_MIN);
    if (weight > 0 && ageMinutes % 3 == 0) weight--;
    if (ageMinutes % 2 == 0) {                 // ~4x mas lento que despierto
      fullness = dropTo(fullness, 1, 30);
      joy = dropTo(joy, 1, 35);
    }
    if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
    defTick(true);  // descansar tambien es bienestar: cuenta para la DEF
    checkMedals();  // aun puede cruzar un nivel por edad mientras duerme
    if (++ticksSinceSave >= 5) pendingSave = true;
    return;
  }

  if (ageMinutes % MINUTES_PER_LEVEL == 0) sfxPlay(SFX_LEVEL);  // subio de nivel (despierto)

  fullness = clamp100(fullness - 2);
  energy = clamp100(energy - 1);
  if (fullness > 40 && poops < 3 && random(100) < 15) poops++;

  hygiene = clamp100(hygiene - 1 - 4 * poops);
  // el sobrepeso da pereza: la energia cae el doble
  if (weight > 50) energy = clamp100(energy - 1);
  if (weight > 0 && ageMinutes % 3 == 0) weight--;

  defTick(false);  // la calma forja la defensa

  int dJoy = -1;
  if (fullness < 30) dJoy -= 2;
  if (hygiene < 30) dJoy -= 2;
  joy = clamp100(joy + dJoy);

  // Descuido: dejar una estadistica por los suelos cuenta como error de
  // cuidado (con enfriamiento para no contar el mismo descuido cada minuto)
  if (mistakeCooldown > 0) mistakeCooldown--;
  if (lowestStat() <= 10 && mistakeCooldown == 0) {
    careMistakes++;
    mistakeCooldown = 60;
    if (bond > 1) bond--;  // el descuido enfria el vinculo, pero sin arrasarlo:
                           // a -3 cada 30 min se perdia mucho mas de lo que se
                           // podia ganar en todo un dia y el vinculo se atascaba
  }

  checkMedals();  // la evolucion la dispara el usuario (canEvolveNow + tap), no el tick
  checkLearnGates();

  // abandono total: con TODO a cero durante una hora queda lista para escaparse;
  // NO se va sola, la dispara el usuario con el boton (final triste, lo presencia)
  if (inTotalNeglect()) {
    if (neglectTicks < RUNAWAY_TICKS) neglectTicks++;
  } else {
    neglectTicks = 0;  // un solo cuidado la salva
  }

  // ciclo completo (forma final + 1 dia): la despedida NO salta sola; queda
  // lista (canFarewellNow) y la dispara el usuario con el boton, para que la vea

  // autoguardado periodico: NO escribir a flash aqui (corre dentro del loop,
  // mientras se anima); solo marcar y dejar que el loop lo vuelque al atenuar
  if (++ticksSinceSave >= 5) pendingSave = true;
}

// Copies the creature into endedMon so it can be offered a party slot, since
// newEgg() is about to wipe every field. Only the two endings the player CHOSE
// qualify: a runaway ran off after an hour of total neglect, and letting it
// come back on the team would remove the cost from the one ending that has any.
// Legacy companions retain ending protection; all active individuals can grow.
void Pet::reviveFrom(const PartyMon &m) {
  if (m.empty()) return;
  ceremony = CER_NONE;
  neglectTicks = 0;
  speciesId = m.dex;
  form = m.form;
  prevSpeciesId = -1;
  eggTaps = 0;
  starterPick = false;
  shiny = m.shiny != 0;
  ivAtk = m.ivAtk; ivDef = m.ivDef; ivSpe = m.ivSpe; ivHp = m.ivHp;
  trAtk = m.trAtk; trDef = m.trDef; trSpe = m.trSpe;
  for (int i = 0; i < MOVE_SLOTS; i++) moves[i] = m.moves[i];
  // its banked level expressed as an age, so level() needs no special case
  ageMinutes = (uint32_t)(m.level ? m.level - 1 : 0) * MINUTES_PER_LEVEL;
  lastLearnLevel = level();     // do not replay every gate it already passed
  learnQCount = 0;
  medals = m.medals;
  careMistakes = 0;
  mistakeCooldown = 0;
  sleeping = false;
  bond = 0;
  bondToday = 0;
  berryKnown = false;
  weight = 0;
  fullness = joy = energy = 80;
  hygiene = 100;
  poops = 0;
  frozen = true;
  strncpy(nick, m.nick, sizeof(nick) - 1);
  nick[sizeof(nick) - 1] = 0;
  sleepAuto = SLEEP_NONE;
  goodTicks = 0;
  evoDeclinedLv = 0;
  farDeclinedAge = 0;
  restoreCare(m);
  endedMon = PartyMon(); endedKind = CER_NONE;
  retirePending = false;
  eatUntil = heartUntil = evolveUntil = ceremonyUntil = medalUntil = 0;
  lastTick = millis();
  deviceClockRemainder = 0;
  registerSpecies(speciesId);
  save();
}

bool Pet::canSwapActive() const {
  // Resolve a starter, move offer or ending first; otherwise exchanging is reversible.
  return !starterPick && ceremony==CER_NONE && !evolving() && learnQCount==0 &&
         endedKind==CER_NONE && !retirePending;
}

PartyMon Pet::storageSnapshot() const {
  PartyMon m;
  if(isEgg()) return m;
  m.dex=speciesId; m.level=level(); m.form=form; m.medals=medals;
  m.ivAtk=ivAtk; m.ivDef=ivDef; m.ivSpe=ivSpe; m.ivHp=ivHp;
  m.trAtk=trAtk; m.trDef=trDef; m.trSpe=trSpe; m.shiny=shiny;
  memcpy(m.nick,nick,sizeof(m.nick)); memcpy(m.moves,moves,sizeof(m.moves));
  uint8_t *c=m.care;
  c[0]=1; c[1]=(frozen?1:0)|(sleeping?2:0)|(berryKnown?4:0);
  c[2]=fullness; c[3]=joy; c[4]=energy; c[5]=hygiene; c[6]=poops; c[7]=weight;
  c[8]=careMistakes; c[9]=bond; c[10]=bondToday; c[11]=mistakeCooldown;
  c[12]=neglectTicks; c[13]=evoDeclinedLv; c[14]=lastLearnLevel; c[15]=sleepAuto;
  memcpy(c+16,&goodTicks,2); memcpy(c+20,&ageMinutes,4);
  memcpy(c+24,&farDeclinedAge,4); uint32_t day=today(); memcpy(c+28,&day,4);
  return m;
}

void Pet::restoreCare(const PartyMon &m) {
  const uint8_t *c=m.care;
  if(c[0]!=1) return;
  frozen=c[1]&1; sleeping=c[1]&2; berryKnown=c[1]&4;
  fullness=c[2]; joy=c[3]; energy=c[4]; hygiene=c[5]; poops=c[6]; weight=c[7];
  careMistakes=c[8]; bond=c[9]; bondToday=c[10]; mistakeCooldown=c[11];
  neglectTicks=c[12]; evoDeclinedLv=c[13]; lastLearnLevel=c[14]; sleepAuto=c[15];
  memcpy(&goodTicks,c+16,2); memcpy(&ageMinutes,c+20,4);
  memcpy(&farDeclinedAge,c+24,4); uint32_t day=0; memcpy(&day,c+28,4);
  if(today() && day!=today()) bondToday=0;
}

bool Pet::storedIndividualMatches() {
  if(!opened) return false;
  Pet check; check.prefs.begin("tamapoke",true); check.lastSeenEpoch=lastSeenEpoch;
  check.load(false); // Verification must not offer moves or normalize saved training.
  PartyMon expected=storageSnapshot(), actual=check.storageSnapshot();
  check.prefs.end();
  return !memcmp(&expected,&actual,sizeof(expected));
}

bool Pet::readStoredSnapshot(PartyMon &out) const {
  Pet check; check.prefs.begin("tamapoke",true);
  PartyMon durable;
  bool ok=check.prefs.getBytesLength("liveCare")==sizeof(durable) &&
    check.prefs.getBytes("liveCare",&durable,sizeof(durable))==sizeof(durable);
  if(!ok) {check.prefs.end();return false;}
  // Android rebases `seen` to the current timezone BEFORE recovery. The date
  // in care is a derived snapshot field, not evidence of a torn individual.
  // Reconstruct at the snapshot's own date, retaining the full byte comparison
  // for ordinary keys (IVs/training/moves/age/etc.) and care metadata.
  uint32_t savedDay=0;memcpy(&savedDay,durable.care+28,4);
  if(savedDay>UINT32_MAX/86400UL) {check.prefs.end();return false;}
  check.lastSeenEpoch=savedDay*86400UL;
  check.load(false);
  out=check.storageSnapshot();
  check.prefs.end();
  // Every ordinary key and the last complete snapshot must agree. Never adopt
  // a mixed individual left by a power cut in the middle of the live-key save.
  return ok && !out.empty() && !memcmp(&out,&durable,sizeof(out));
}

bool Pet::snapshotForParty() {
  if(endedKind!=CER_NONE)return true;
  if (isEgg()) return true;
  if (ceremony != CER_FAREWELL && ceremony != CER_RELEASE) return true;
  // An EARLY retire gives the creature up for good -- it is not banked at all.
  // Retiring one that has EARNED its farewell still banks it, because that is
  // simply the farewell reached by another button. retirePending is still set
  // here: update() snapshots before newEgg() spends it, and that ordering is
  // what this depends on, so retire_test drives the real update() rather than
  // calling the two halves by hand.
  if (retireIsEarly()) return true;
  endedMon = PartyMon();
  endedMon.dex = speciesId;
  endedMon.form = form;
  endedMon.level = level();
  endedMon.medals = medals;
  endedMon.ivAtk = ivAtk;
  endedMon.ivDef = ivDef;
  endedMon.ivSpe = ivSpe;
  endedMon.ivHp = ivHp;
  endedMon.trAtk = trAtk;
  endedMon.trDef = trDef;
  endedMon.trSpe = trSpe;
  endedMon.shiny = shiny ? 1 : 0;
  for (int i = 0; i < MOVE_SLOTS; i++) endedMon.moves[i] = moves[i];  // frozen too
  strncpy(endedMon.nick, nick, sizeof(endedMon.nick) - 1);
  endedMon.nick[sizeof(endedMon.nick) - 1] = 0;
  // Reserved companion bytes carry a handoff ticket. The roster and this
  // single NVS blob can then be acknowledged idempotently after a restart.
  uint32_t ticket=((uint32_t)random(65536L)<<16)|(uint32_t)random(65536L);
  if(!ticket)ticket=1;memcpy(endedMon.care+32,&ticket,4);
  if(opened) {
    prefs.putBytes("endWait",&endedMon,sizeof(endedMon));
    PartyMon check;
    if(prefs.getBytesLength("endWait")!=sizeof(check) ||
       prefs.getBytes("endWait",&check,sizeof(check))!=sizeof(check) ||
       memcmp(&check,&endedMon,sizeof(check)))return false;
  }
  endedKind = ceremony;
  return true;
}

bool Pet::acknowledgeEnding() {
  if(opened) {
    PartyMon empty,check;
    prefs.putBytes("endWait",&empty,sizeof(empty));
    if(prefs.getBytesLength("endWait")!=sizeof(check) ||
       prefs.getBytes("endWait",&check,sizeof(check))!=sizeof(check) ||
       memcmp(&check,&empty,sizeof(check)))return false;
  }
  endedKind=CER_NONE;
  return true;
}

// vuelca el guardado periodico pendiente (lo llama el loop en un momento sin
// animacion para que el paron de la escritura a flash no se vea)
bool Pet::selectForm(FormId id) {
  if (isEgg() || ceremony != CER_NONE || !formEligible(speciesId,id,level())) return false;
  FormId old=form;
  form=id;
  save();
  if(opened && (prefs.getUShort("form",65535)!=id || prefs.getShort("formDex",-1)!=speciesId)) {
    form=old;return false;
  }
  return true;
}

void Pet::saveNow() { save(); }

void Pet::flushSave() {
  if (pendingSave) save();
}

// La calma forja la defensa: cada hora de bienestar (descansando, o despierto
// con todo >= 40) da +1 de DEF, hasta el tope que permita el IV.
//
// Antes pedia 12 h SEGUIDAS y CUALQUIER desliz ponia el contador a cero, ademas
// de no contar el sueno. Simulando una vida entera (3 dias) eso daba 1 punto al
// jugador teoricamente perfecto (uno que actue cada minuto durante 72 h) y 0 a
// todos los demas, incluido uno que atienda cada 15 min: la comida cae 2/min,
// asi que quien no pase por el bicho cada media hora esta SIEMPRE por debajo de
// 40 y el contador no arrancaba nunca. La DEF era, en la practica, inentrenable.
// Ahora acumula en vez de resetear: un descuido cuesta los minutos malos, no
// todo el progreso.
void Pet::defTick(bool resting) {
  if (!resting && lowestStat() < 40) return;
  if (++goodTicks < DEF_TRAIN_TICKS) return;
  goodTicks = 0;
  if (trDef < trMaxDef()) trDef++;
}

uint8_t Pet::evolutionOptions(int16_t base, int16_t *out) const {
  uint8_t n = 0;
  for (uint8_t branch = 0; branch < EVO_BRANCH_COUNT; branch++) {
    if (EVO_BRANCHES[branch].base != base) continue;
    for (uint8_t i = 0; i < EVO_BRANCHES[branch].count; i++) {
      int16_t target = EVO_BRANCHES[branch].targets[i];
      if (target < 1 || target > DEX_COUNT) continue;
      if (!speciesHasArt(target)) continue;                    // no art anywhere
      if (!regionAvailable(regionOfDex(target))) continue;     // pack not on the card
      out[n++] = target;
    }
    break;
  }
  return n;
}

// A branch can lead into another ordinary evolution (Wurmple is the useful
// example), so collection completeness must walk every path, not merely look at
// the immediate choices.
static bool lineMissingFrom(const Pet &pet, int16_t cur, uint8_t depth) {
  if (cur < 1 || cur > DEX_COUNT || depth >= 8) return false;
  if (!pet.isRegistered(cur)) return true;
  int16_t opts[EVO_BRANCH_MAX];
  uint8_t n = pet.evolutionOptions(cur, opts);
  if (n) {
    for (uint8_t i = 0; i < n; i++)
      if (lineMissingFrom(pet, opts[i], depth + 1)) return true;
    return false;
  }
  return lineMissingFrom(pet, DEX_TBL[cur].evolvesTo, depth + 1);
}

// quedan miembros sin registrar en la linea evolutiva de esta base?
bool Pet::lineHasUnregistered(int16_t base) const {
  return lineMissingFrom(*this, base, 0);
}

uint8_t Pet::eggRarity() const {
  return (eggTarget >= 1 && eggTarget <= DEX_COUNT) ? DEX_TBL[eggTarget].rarity : R_COMUN;
}

// elige la especie del huevo: tirada de rareza (mejorada por una despedida
// completa, castigada por una escapada) y sesgo hacia lineas incompletas
uint16_t gRegionArt = 0xFFFF;   // everything, until the SD narrows it

bool regionAvailable(uint8_t r) {
  if (r >= REGION_COUNT) return false;
  if (r == REGION_ALL) {                       // the mixed pool: any pack will do
    for (uint8_t i = 0; i < REGION_COUNT; i++)
      if (i != REGION_ALL && (gRegionArt & (uint16_t)(1u << i))) return true;
    return false;
  }
  return (gRegionArt & (uint16_t)(1u << r)) != 0;
}

uint8_t regionOfDex(int16_t d) {
  for (uint8_t i = 0; i < REGION_COUNT; i++) {
    if (i == REGION_ALL) continue;
    if (d >= REGIONS[i].lo && d <= REGIONS[i].hi) return i;
  }
  return REGION_ALL;
}

uint8_t nextAvailableRegion(uint8_t from) {
  for (uint8_t i = 1; i <= REGION_COUNT; i++) {
    uint8_t r = (uint8_t)((from + i) % REGION_COUNT);
    if (regionAvailable(r)) return r;
  }
  return from;                      // nothing available anywhere: stay put
}

// The region to actually hatch from. Normally the player's own, but a card can
// be swapped under a save: rather than rewrite their choice (which would lose
// it silently the moment they put the right card back), the CHOICE is kept and
// only the roll falls through to somewhere playable.
static uint8_t eggRegionFallback(uint8_t want) {
  if (regionAvailable(want)) return want;
  for (uint8_t i = 0; i < REGION_COUNT; i++)
    if (i != REGION_ALL && regionAvailable(i)) return i;
  return want;                      // no art anywhere: behave as we always did
}

int16_t Pet::pickEggSpecies() {
  const uint8_t use = eggRegionFallback(region % REGION_COUNT);
  const RegionInfo &rg = REGIONS[use];
  // primera partida: inicial clasico -- del region elegida, so a Johto run
  // starts with a Johto starter rather than a Kanto one
  if (registeredCount() == 0) {
    return rg.starters[random(rg.starterCount)];
  }

  uint8_t tier = R_COMUN;
  if (lastEnd != CER_RUNAWAY) {
    bool blessed = (lastEnd == CER_FAREWELL);
    int rare = (blessed ? 45 : 27) + careBonus();
    int leg = eggLegendWeight();
    int r = rollEggOdds(900);
    if (r < leg) tier = R_LEGENDARIO;
    else if (r < leg + rare * 9) tier = R_RARO;
  }

  // candidatos del tier con linea incompleta; si no hay, baja de tier;
  // si la pokedex del tier esta completa, vale cualquiera del tier
  // Normal eggs prefer incomplete families. Shiny eggs skip that preference
  // entirely (including the shiny Dex), but retain rarity/art/region gates.
  for (int pass = eggShiny ? 1 : 0; pass < 2; pass++) {
    for (int t = tier; t >= R_COMUN; t--) {
      // Reservoir sampling keeps every eligible species equally likely without
      // a fixed candidate array. The old 260-entry cap silently cut late
      // generations out of the ALL/common pool once the dex grew past 809.
      int16_t picked = 0;
      uint16_t n = 0;
      for (int16_t d = rg.lo; d <= rg.hi; d++) {
        if (DEX_TBL[d].rarity != t) continue;
        if (pass == 0 && !lineHasUnregistered(d)) continue;
        // ALL spans every region, so filter per species: a missing Sinnoh pack
        // must not put a Sinnoh creature in a mixed egg.
        if (!regionAvailable(regionOfDex(d))) continue;
        // A species with no art, and any earlier member whose evolution line
        // can reach it, never hatches. Dex numbers and existing saves stay
        // untouched; the incomplete family is only removed from new eggs.
        if (!speciesCanHatch(d)) continue;
        n++;
        if (random(n) == 0) picked = d;
      }
      if (n > 0) return picked;
    }
  }
  return rg.starters[random(rg.starterCount)];  // inalcanzable, por si acaso
}

// Rolls a species of a GIVEN tier inside a region. Used when the player changes
// region while an egg is waiting: the rarity they were granted is kept and only
// the region changes, so switching cannot be farmed for a legendary.
int16_t Pet::rollInRegion(uint8_t r, uint8_t tier) {
  const RegionInfo &rg = REGIONS[eggRegionFallback(r % REGION_COUNT)];
  for (int t = tier; t >= R_COMUN; t--) {
    int16_t picked = 0;
    uint16_t n = 0;
    for (int16_t d = rg.lo; d <= rg.hi; d++)
      if (DEX_TBL[d].rarity == t && regionAvailable(regionOfDex(d)) &&
          speciesCanHatch(d)) {
        n++;
        if (random(n) == 0) picked = d;
      }
    if (n) return picked;
  }
  return rg.starters[0];      // a region with nothing in it cannot happen
}

// Changing region swaps the WAITING egg to that region's creature.
//
// Without this the setting would look broken: you would pick Johto and still
// hatch a Rattata, because the species is rolled when the egg appears and not
// when it cracks. Two rules stop it becoming a re-roll button:
//
//   1. The rarity tier is kept. Only which species of that tier changes, so
//      toggling can never be farmed for a legendary.
//   2. Each region's answer is REMEMBERED for this egg. Switching back shows
//      the same creature again, so there is nothing to gain by flipping.
//
// A hatched creature is untouched -- this only ever moves an egg.
void Pet::setRegion(uint8_t r) {
  r %= REGION_COUNT;
  // The sprite pack is a real gate, not a hint: without it the region is not
  // selectable at all. The chooser still SHOWS it, greyed and with a reason --
  // hiding it outright is how Johto and Hoenn once came to look absent when
  // they were built and reachable all along.
  if (!regionAvailable(r)) return;
  if (r == region) return;
  uint8_t old = region;
  region = r;
  if (isEgg() && eggTarget >= 1) {
    if (old < REGION_COUNT) eggByRegion[old] = eggTarget;
    int16_t known = eggByRegion[r];
    eggTarget = known >= 1 ? known : rollInRegion(r, eggRarity());
    eggByRegion[r] = eggTarget;
  }
  save();
}

void Pet::registerSpecies(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return;
  dexReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
  if (shiny) dexShinyReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
}

// la racha y el vinculo mejoran el sorteo del huevo (0..~14)
int Pet::careBonus() const {
  int s = streak > 30 ? 30 : streak;
  return s / 3 + bond / 25;
}

uint8_t Pet::eggBonusDays() const {
  uint32_t d = today();
  // Yesterday's streak remains usable until today's first care. A missed full
  // calendar day invalidates it immediately, even before the next care action.
  if (!d || !lastCareDay || d < lastCareDay || d - lastCareDay > 1) return 0;
  return streak > 10 ? 10 : (uint8_t)streak;
}

uint16_t Pet::eggShinyWeight() const {
  return EggRewards::weight(EggRewards::SHINY_BASE, EggRewards::SHINY_SOLO,
      EggRewards::SHINY_MAX, eggBonusDays(), collectibleRegisteredCount(), pokedexCollectibleCount());
}

uint16_t Pet::eggLegendWeight() const {
  if (registeredCount() < 25 || lastEnd == CER_RUNAWAY) return 0;
  return EggRewards::weight(EggRewards::LEGEND_BASE, EggRewards::LEGEND_SOLO,
      EggRewards::LEGEND_MAX, eggBonusDays(), collectibleRegisteredCount(), pokedexCollectibleCount());
}

uint16_t Pet::collectibleRegisteredCount() const {
  uint16_t n = registeredCount();
  // Keep historical registrations, but all locked evolution families are
  // excluded from both the numerator and denominator of collection bonuses.
  for (int i = 0; i < NO_HATCH_COUNT; ++i)
    if (NO_HATCH[i] >= 1 && NO_HATCH[i] <= DEX_COUNT && isRegistered(NO_HATCH[i])) --n;
  return n;
}

uint8_t Pet::farewellsRemaining() const {
  // A backward/unknown clock never refills the persisted allowance.
  return 3 - (today() > (farewellQuota >> 2) ? 0 : (farewellQuota & 3));
}

bool Pet::consumeFarewell() {
  if (!farewellsRemaining()) return false;
  uint32_t d = today(), oldDay = farewellQuota >> 2;
  uint32_t used = d > oldDay ? 0 : farewellQuota & 3;
  if (d < oldDay) d = oldDay;
  uint32_t next = (d << 2) | (used + 1);
  // Commit one atomic scalar BEFORE starting the ceremony. On storage failure
  // do not lose the creature or let repeated restarts provide free rerolls.
  if (opened) {
    prefs.putUInt("byeQuota", next);
    if (prefs.getUInt("byeQuota", 0) != next) return false;
  }
  farewellQuota = next;
  return true;
}

// primer cuidado del dia: avanza la racha y afianza el vinculo
void Pet::registerCare() {
  if (isEgg() || ceremony != CER_NONE) return;
  uint32_t d = today();
  if (d == 0 || d <= lastCareDay) return;  // no clock, duplicate or rollback
  if (lastCareDay != 0 && d == lastCareDay + 1) {
    if (streak < UINT16_MAX) streak++;
  } else {
    streak = 1;        // hubo un hueco de dias
    lastMilestone = 0;
  }
  lastCareDay = d;
  bondToday = 0;
  if (streak > bestStreak) bestStreak = streak;
  bond = clamp100(bond + 4);
  uint16_t ms = (streak >= 100) ? 100 : (streak >= 30) ? 30
              : (streak >= 7)   ? 7   : (streak >= 3)  ? 3 : 0;
  if (ms > lastMilestone) {
    lastMilestone = ms;
    milestoneUntil = millis() + 4500;
  }
  checkMedals();
  save();
}

void Pet::addBond(uint8_t amt) {
  if (bondToday >= 20) return;  // tope diario: el vinculo no se farmea
  bond = clamp100(bond + amt);
  bondToday += amt;
}

void Pet::checkMedals() {
  if (isEgg()) return;
  uint16_t before = medals;
  if (level() >= 10) medals |= MED_LV10;
  if (level() >= 25) medals |= MED_LV25;
  if (level() >= 50) medals |= MED_LV50;
  if (berryKnown) medals |= MED_BERRY;
  if (streak >= 7) medals |= MED_STREAK7;
  if (bond >= 100) medals |= MED_BOND;
  if (DEX_TBL[speciesId].evolvesTo == 0) medals |= MED_FINAL;
  if (weight == 0 && level() >= 5 && careMistakes == 0) medals |= MED_FIT;
  uint16_t gained = medals & ~before;
  if (gained) {
    for (uint16_t m = gained; m; m &= (m - 1)) totalMedals++;
    newMedal = gained;
    medalUntil = millis() + 4000;
    if (!sleeping) sfxPlay(SFX_MEDAL);
    save();
  }
}

void Pet::rename(const char *name) {
  strncpy(nick, name, sizeof(nick) - 1);
  nick[sizeof(nick) - 1] = 0;
  save();
}

// La aportacion del IV (IV x nivel / 100) es exactamente la de los juegos de
// 3a generacion en adelante: un IV perfecto vale +31 a nivel 100. El resto de
// la formula es la de TamaPoke (base plana + nivel) y no la de los juegos: con
// el x nivel/100 canonico sobre la base, un bicho recien nacido mostraria
// stats de un solo digito, que en una pantalla de mascota parece un error.
static uint16_t calcStat(uint8_t base, uint8_t iv, uint8_t lvl, uint8_t tr) {
  return (uint16_t)base + lvl + (uint16_t)iv * lvl / 100 + tr;
}

uint16_t Pet::atkStat() const {
  return isEgg() ? 0 : calcStat(formDex(speciesId, form).bAtk, ivAtk, level(), trAtk);
}
uint16_t Pet::defStat() const {
  return isEgg() ? 0 : calcStat(formDex(speciesId, form).bDef, ivDef, level(), trDef);
}
uint16_t Pet::speStat() const {
  return isEgg() ? 0 : calcStat(formDex(speciesId, form).bSpe, ivSpe, level(), trSpe);
}
// la vitalidad no se entrena (no hay nada que la suba), asi que lleva un +10
// fijo en lugar del entrenamiento, igual que el +Nivel+10 del HP en los juegos
uint16_t Pet::vitStat() const {
  return isEgg() ? 0 : calcStat(formDex(speciesId, form).bHp, ivHp, level(), 10);
}
// Special reuses the physical IV and training against the species' special base
// stat, which is what keeps Alakazam (50 Atk / 135 SpA) a real attacker without
// adding IVs or migrating saves.
uint16_t Pet::spaStat() const {
  return isEgg() ? 0 : calcStat(formDex(speciesId, form).bSpA, ivAtk, level(), trAtk);
}
uint16_t Pet::spdStat() const {
  return isEgg() ? 0 : calcStat(formDex(speciesId, form).bSpD, ivDef, level(), trDef);
}

// ---------- moves ----------

uint8_t Pet::moveCount() const {
  uint8_t n = 0;
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (moves[i]) n++;
  return n;
}

bool Pet::knowsMove(MoveId mv) const {
  if (!mv) return false;
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (moves[i] == mv) return true;
  return false;
}

// Picks a sensible default set, best first.
//
// NOT "the newest four": 1907 of the 2281 learnset entries sit at level 0 and
// another 237 at level 1, so for most species level orders nothing and taking
// the last four in table order is arbitrary -- it handed a level 100 Charizard
// GROWL and LEER. So score instead: attacks over status, stronger over weaker,
// STAB ahead of equal power, and a bonus for the handful of moves that really
// are gated behind a level, since those are meant to be upgrades.
// TMs unlock at one level, for everything.
//
// This replaced a power/2 curve, which was impossible for a player to predict
// (SURF at 45, ROCK SLIDE at 37) and dribbled unlocks out one at a time so none
// of them felt like anything. A single number is explainable in one sentence and
// lands on a seam the game already has: the first five leaders sit at 14-43, so
// you fight the early ladder on what your species actually learns, and TMs
// arrive as you enter the back half. A creature retires at 73 and caps at 100.
//
// It only works because dex_moves.py now carries the cheap early attacks --
// SCRATCH, PECK, POISON STING, BUBBLE and the rest. Without those, gating TMs
// this hard would leave young creatures with nothing at all, which is exactly
// what the power/2 version was papering over.
#define TM_LEVEL 40

uint8_t tmLevelFor(const MoveEntry &m) {
  (void)m;
  return TM_LEVEL;
}

// THE single answer, used by relearnFromLevel(), by the STAB fallback and by
// the move picker in the sketch. Three call sites once had three opinions.
uint8_t moveUnlockLevel(int16_t dex, uint8_t idx) {
  uint8_t at = learnLevel(dex, idx);
  if (at > 0) return at;                 // a real level-up move
  MoveId mv = learnMove(dex, idx);
  if (!mv || mv >= MOVE_COUNT) return 255;
  return tmLevelFor(MOVE_TBL[mv]);       // a TM: no natural level, so the gate
}

void Pet::relearnFromLevel() {
  for (int i = 0; i < MOVE_SLOTS; i++) moves[i] = 0;
  if (isEgg()) return;
  const DexEntry d = formDex(speciesId,form);
  uint8_t lvl = level(), n = formLearnCount(speciesId,form);
  int16_t score[MOVE_SLOTS] = { 0, 0, 0, 0 };
  // Two passes. Level-up moves (level >= 1) are what a creature grows into, so
  // they fill the set first; TMs (level 0, no gate) only top up the slots left
  // over. Without this a just-hatched pet opens with FIRE BLAST and SOLAR BEAM,
  // because every TM is legal at level 1.
  for (int pass = 0; pass < 2; pass++) {
  bool tmPass = (pass == 1);
  for (uint8_t i = 0; i < n; i++) {
    uint8_t at = formLearnLevel(speciesId,form,i);
    if (at > lvl) continue;
    if (tmPass != (at == 0)) continue;
    MoveId mv = formLearnMove(speciesId,form,i);
    if (!mv || mv >= MOVE_COUNT || knowsMove(mv)) continue;
    const MoveEntry &m = MOVE_TBL[mv];
    // A TM carries no level requirement in the data, which is true of the games
    // but wrong here: with only one or two level-up moves early on, the spare
    // slots were filled with the strongest TMs in the table and a NEWBORN opened
    // with SURF, BLIZZARD and OUTRAGE. That, not the damage formula, is why a
    // level 1 Squirtle could beat Brock.
    //
    // So a TM is gated by its own power: roughly power/2, which puts the 40s
    // around level 20 and the 110s out past 50 where a creature is genuinely
    // built. Level-up moves are untouched -- they already have real gates.
    if (tmPass && lvl < tmLevelFor(m)) continue;
    int16_t sc = (m.cat == MC_STATUS) ? 10 : (int16_t)m.power + 20;
    // STAB outweighs raw power, or every species defaults to the same two
    // generic sledgehammers and the roster loses its identity.
    if (m.cat != MC_STATUS && (m.type == d.type1 || m.type == d.type2)) sc += 40;
    if (m.effect == EF_RECHARGE) sc -= 35;   // a free turn for the opponent
    if (m.effect == EF_RECOIL) sc -= 20;
    sc += at;
    if (sc < 1) sc = 1;
    int slot = -1;
    for (int s = 0; s < MOVE_SLOTS; s++)
      if (sc > score[s]) { slot = s; break; }
    if (slot < 0) continue;
    for (int s = MOVE_SLOTS - 1; s > slot; s--) {
      score[s] = score[s - 1];
      moves[s] = moves[s - 1];
    }
    score[slot] = sc;
    moves[slot] = mv;
  }
  if (moveCount() >= MOVE_SLOTS) break;   // level-up moves already filled it
  }
  // Guarantee one same-type move. Machamp's only Fighting options are weak or
  // recoil-laden, so pure scoring left it with four generic attacks and nothing
  // that reads as a Machamp. If the set came out with no STAB, the weakest slot
  // gives way to the best same-type attack the species can actually learn.
  for (int i = 0; i < MOVE_SLOTS; i++) {
    if (!moves[i]) continue;
    const MoveEntry &m = MOVE_TBL[moves[i]];
    if (m.cat != MC_STATUS && (m.type == d.type1 || m.type == d.type2)) return;
  }
  MoveId best = 0;
  int16_t bestSc = 0;
  for (uint8_t i = 0; i < n; i++) {
    uint8_t at = formLearnLevel(speciesId,form,i);
    if (at > lvl) continue;
    MoveId mv = formLearnMove(speciesId,form,i);
    if (!mv || mv >= MOVE_COUNT) continue;
    const MoveEntry &m = MOVE_TBL[mv];
    if (m.cat == MC_STATUS || (m.type != d.type1 && m.type != d.type2)) continue;
    // The same TM gate as above. This fallback used to ignore it, which is how
    // a level 1 Squirtle ended up holding SURF: it had no Water move, so the
    // guarantee reached past every check and handed it the best one in the
    // table. A creature with no STAB it can legally use simply has none yet.
    if (at == 0 && lvl < tmLevelFor(m)) continue;
    int16_t sc = (int16_t)m.power;
    if (m.effect == EF_RECHARGE) sc -= 35;
    if (m.effect == EF_RECOIL) sc -= 20;
    if (sc > bestSc) { bestSc = sc; best = mv; }
  }
  if (best) moves[MOVE_SLOTS - 1] = best;
  // Some species only know moves outside this deliberately compact table at
  // low levels (Abra's TELEPORT, Ditto's TRANSFORM, Smeargle's SKETCH, and so
  // on). They must still be able to enter a battle. STRUGGLE is a temporary
  // engine fallback and is replaced as soon as a real level-up move arrives.
  bool hasAttack = false;
  int freeSlot = -1;
  for (int i = 0; i < MOVE_SLOTS; i++) {
    if (!moves[i] && freeSlot < 0) freeSlot = i;
    if (moves[i] && MOVE_TBL[moves[i]].cat != MC_STATUS) hasAttack = true;
  }
  // Natural tables now include every status-only starting move too. Abra's
  // TELEPORT and Magikarp's SPLASH must not displace the temporary attack that
  // lets this single-player battle system finish a fight.
  if (!hasAttack) moves[freeSlot >= 0 ? freeSlot : MOVE_SLOTS - 1] = MV_STRUGGLE;
}

// Queues every level-up move unlocked since the last check. A free slot is
// filled silently -- the games do not ask when there is room either -- and only
// a full moveset produces an offer the player has to answer.
void Pet::learnMoveNow(MoveId mv) {
  if (!mv || mv >= MOVE_COUNT || knowsMove(mv)) return;
  int freeSlot = -1;
  // STRUGGLE is not a learned move. The first real move replaces it instead
  // of leaving the fallback in the set forever.
  if (MOVE_TBL[mv].cat != MC_STATUS)
    for (int s = 0; s < MOVE_SLOTS; s++)
      if (moves[s] == MV_STRUGGLE) { freeSlot = s; break; }
  for (int s = 0; s < MOVE_SLOTS; s++)
    if (freeSlot < 0 && !moves[s]) { freeSlot = s; break; }
  if (freeSlot >= 0) {
    moves[freeSlot] = mv;
    pendingSave = true;
    return;
  }
  for (uint8_t q = 0; q < learnQCount; q++)
    if (learnQueue[q] == mv) return;
  if (learnQCount < LEARN_QUEUE_MAX) {
    learnQueue[learnQCount++] = mv;
    pendingSave = true;
  }
}

void Pet::checkLearnGates() {
  if (isEgg() || ceremony != CER_NONE) return;
  uint8_t lvl = level();
  if (lvl <= lastLearnLevel) return;
  uint8_t n = formLearnCount(speciesId,form);
  for (uint8_t i = 0; i < n; i++) {
    uint8_t at = formLearnLevel(speciesId,form,i);
    if (at == 0 || at <= lastLearnLevel || at > lvl) continue;  // 0 = TM, no gate
    MoveId mv = formLearnMove(speciesId,form,i);
    learnMoveNow(mv);
  }
  lastLearnLevel = lvl;
  pendingSave = true;
}

static void popLearn(MoveId *q, uint8_t &n) {
  if (!n) return;
  for (uint8_t i = 0; i + 1 < n; i++) q[i] = q[i + 1];
  q[--n] = 0;
}

void Pet::acceptLearn(uint8_t slot) {
  if (!learnQCount || slot >= MOVE_SLOTS) return;
  moves[slot] = learnQueue[0];
  popLearn(learnQueue, learnQCount);
  save();
}

void Pet::declineLearn() {
  popLearn(learnQueue, learnQCount);
  save();
}

uint8_t Pet::pendingLearnables(MoveId *out, uint8_t max) const {
  if (isEgg() || !out || !max) return 0;
  uint8_t lvl = level(), n = formLearnCount(speciesId,form), w = 0;
  // Keep a declined evolution move recoverable from the move card even when
  // it is not part of this edition's ordinary level/TM learnset.
  for (uint8_t i = 0; !formLearnIndex(speciesId,form) && i < evolutionMoveCount(speciesId) && w < max; i++) {
    MoveId mv = evolutionMove(speciesId, i);
    if (mv && !knowsMove(mv)) out[w++] = mv;
  }
  for (uint8_t i = 0; i < n && w < max; i++) {
    uint8_t at=formLearnLevel(speciesId,form,i);
    MoveId mv = formLearnMove(speciesId,form,i);
    if((at?at:tmLevelFor(MOVE_TBL[mv]))>lvl)continue;
    if (knowsMove(mv)) continue;
    bool dup = false;                     // do not offer the same move twice
    for (uint8_t j = 0; j < w; j++)
      if (out[j] == mv) { dup = true; break; }
    if (!dup) out[w++] = mv;
  }
  return w;
}

// Tirada de un IV: 8-31. El suelo en 8 es deliberado — en los juegos un 0 es
// posible porque puedes criar cientos de huevos, aqui cada crianza dura 3 dias
// y un individuo de desecho seria un castigo desproporcionado. La racha y el
// vinculo del bicho ANTERIOR empujan la tirada: cuidar bien mejora la camada.
uint8_t Pet::rollIV(int bonus) const {
  int v = 8 + (int)random(24) + bonus / 2;  // bonus 0..14 -> +0..7
  return (uint8_t)(v > 31 ? 31 : v);
}

// Guardados con el sistema viejo de genes (90-110%): se convierten al rango de
// IV que se sortea hoy (8-31) para que nadie salga perdiendo con la
// actualizacion. gene 0 = mascota anterior incluso a los genes.
uint8_t Pet::ivFromGene(uint8_t gene) const {
  if (gene == 0) return rollIV(0);
  if (gene < 90) gene = 90;
  if (gene > 110) gene = 110;
  return 8 + (uint8_t)(((uint16_t)(gene - 90) * 23) / 20);
}

void Pet::rollIVs() {
  int bonus = careBonus();
  ivAtk = rollIV(bonus);
  ivDef = rollIV(bonus);
  ivSpe = rollIV(bonus);
  ivHp = rollIV(bonus);
  // los legendarios nacen con 3 de 4 IV perfectos, como en los juegos
  if (speciesId >= 1 && speciesId <= DEX_COUNT && DEX_TBL[speciesId].rarity == R_LEGENDARIO) {
    uint8_t *p[4] = { &ivAtk, &ivDef, &ivSpe, &ivHp };
    for (int k = 3; k > 0; k--) {  // baraja para elegir cuales 3
      int j = random(k + 1);
      uint8_t *t = p[k]; p[k] = p[j]; p[j] = t;
    }
    for (int k = 0; k < 3; k++) *p[k] = 31;
  }
  // en la 2a generacion el shiny ERA un patron de DV concreto: un shiny nunca
  // era mediocre. Aqui se traduce como un suelo de 20 en todos los IV.
  if (shiny) {
    if (ivAtk < 20) ivAtk = 20;
    if (ivDef < 20) ivDef = 20;
    if (ivSpe < 20) ivSpe = 20;
    if (ivHp < 20) ivHp = 20;
  }
}

uint16_t Pet::registeredCount() const {
  uint16_t n = 0;
  for (int i = 1; i <= DEX_COUNT; i++)
    if (isRegistered(i)) n++;
  return n;
}

void Pet::registerCaughtSpecies(int16_t dex, bool caughtShiny) {
  if (dex < 1 || dex > DEX_COUNT) return;
  dexReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
  if (caughtShiny) dexShinyReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
  save();
}

// forma final que ya cumplio su ciclo (1 dia): lista para despedirse. La
// despedida la dispara el usuario con el boton (no salta sola, para que la vea)
bool Pet::canFarewellNow() const {
  if (frozen || endedKind!=CER_NONE || !farewellsRemaining()) return false;
  return !isEgg() && !sleeping && ceremony == CER_NONE &&
         DEX_TBL[speciesId].evolvesTo == 0 && ageMinutes >= FAREWELL_AGE_MIN;
}

// abandono total durante 1h: lista para escaparse. La dispara el usuario con el
// boton (final triste); cuidarla un solo tick la salva (neglectTicks se resetea)
bool Pet::canRunawayNow() const {
  if (frozen) return false;
  // inTotalNeglect() as well as the counter, and NOT just the counter. The
  // sleeping branch of tick() returns before the neglect block, so neglectTicks
  // is frozen rather than cleared for the whole night: a creature that went to
  // bed at zero woke with energy back at 100 and was still one tap from
  // leaving, until the next tick 60 s later cleared it. That tap is a caress --
  // the button is drawn over the creature -- so the window really was reachable
  // and it cost somebody a DRAGONAIR.
  return !isEgg() && !sleeping && ceremony == CER_NONE &&
         neglectTicks >= RUNAWAY_TICKS && inTotalNeglect();
}

bool Pet::canRetireNow() const {
  if (frozen || endedKind!=CER_NONE || !farewellsRemaining()) return false;
  return !isEgg() && !sleeping && ceremony == CER_NONE && !starterPick;
}

// The ceremony is the same one. Mark it before the ceremony starts so the
// creature is not banked if this is an early retirement.
void Pet::startRetire() {
  if (!canRetireNow()) return;
  beginFarewell(!canFarewellNow());
}

void Pet::startFarewell() {
  beginFarewell(false);
}

void Pet::beginFarewell(bool early) {
  if (isEgg() || ceremony != CER_NONE || endedKind!=CER_NONE) return;
  if (!consumeFarewell()) return;
  // Persist the correct ending on the FIRST save, not a good ending followed
  // by a second write that could be interrupted during early retirement.
  retirePending = early;
  lastEnd = early ? CER_RELEASE : CER_FAREWELL;
  ceremony = CER_FAREWELL;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;  // corazones durante toda la despedida
  sfxPlay(SFX_BYE);
  save();
}

void Pet::startRunaway() {
  if (isEgg() || ceremony != CER_NONE || endedKind!=CER_NONE) return;
  lastEnd = CER_RUNAWAY;
  ceremony = CER_RUNAWAY;
  ceremonyUntil = millis() + CEREMONY_MS;
  sfxPlay(SFX_BYE);
  save();
}

void Pet::release() {
  if (isEgg() || ceremony != CER_NONE || endedKind!=CER_NONE) return;
  if (!consumeFarewell()) return;
  lastEnd = CER_RELEASE;
  ceremony = CER_RELEASE;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;
  sfxPlay(SFX_BYE);
  save();
}

void Pet::hatch() {
  speciesId = eggTarget;
  form = 0;
  shiny = eggShiny;
  // IV del individuo (cada crianza es unica). Se tiran ANTES de resetear el
  // vinculo a proposito: el careBonus que los empuja es el del bicho anterior.
  rollIVs();
  trAtk = trDef = trSpe = 0;
  goodTicks = 0;
  berryKnown = false;
  bond = 0;          // vinculo, medallas y nombre son del individuo
  bondToday = 0;
  medals = 0;
  newMedal = 0;
  nick[0] = 0;
  registerSpecies(speciesId);  // criado = registrado en la pokedex
  // Start empty: checkLearnGates() fills the level-1 moves. Seeding from TMs
  // instead would hand a newborn FIRE BLAST, which no level 1 creature knows.
  for (int i = 0; i < MOVE_SLOTS; i++) moves[i] = 0;
  learnQCount = 0;
  lastLearnLevel = 0;
  checkLearnGates();
  checkMedals();     // por si nace ya en forma final (legendario)
  sfxPlay(SFX_HATCH);
  save();
}

// ¿se dan ya las condiciones para evolucionar? Cada descuido retrasa la
// evolucion 1 nivel, y ademas tiene que estar bien cuidado en ese momento
// (ninguna estadistica por debajo de 40). NO evoluciona sola: la dispara el
// usuario tocando al bicho (evolve()), para que vea la transformacion.
bool Pet::canEvolveNow() const {
  if (isEgg() || sleeping || ceremony != CER_NONE) return false;
  // Legacy owned members remain usable, but must not evolve into blank art.
  if (!speciesIsCollectible(speciesId)) return false;
  const DexEntry &d = DEX_TBL[speciesId];
  if (d.evolvesTo == 0) return false;
  return level() >= (uint16_t)(d.evolveLevel + careMistakes) &&
         lowestStat() >= 40;
}

void Pet::evolve() {
  if (!canEvolveNow()) return;
  const DexEntry &d = DEX_TBL[speciesId];
  prevSpeciesId = speciesId;
  int16_t next = d.evolvesTo;
  // Every multi-result family uses Eevee's collection rule: prefer a branch
  // whose line still has an unseen member, then choose randomly once all paths
  // are complete. If no target has installed art, the table's primary result
  // remains the fallback so evolution never freezes.
  int16_t opts[EVO_BRANCH_MAX];
  uint8_t n = evolutionOptions(speciesId, opts);
  if (n) {
    int16_t fresh[EVO_BRANCH_MAX];
    uint8_t m = 0;
    for (uint8_t i = 0; i < n; i++)
      if (lineHasUnregistered(opts[i])) fresh[m++] = opts[i];
    next = m ? fresh[random(m)] : opts[random(n)];
  }
  speciesId = next;
  form = 0; // Normal evolution remains separate from optional forms.
  registerSpecies(speciesId);
  // Evolution moves are not level gates. The old form has already consumed
  // this level before the player taps EVOLVE, which is why Meowscarada used to
  // miss FLOWER TRICK entirely. Teach or queue the new form's move explicitly.
  for (uint8_t i = 0; i < evolutionMoveCount(speciesId); i++)
    learnMoveNow(evolutionMove(speciesId, i));
  checkLearnGates();
  sfxPlay(SFX_EVOLVE);
  evolveUntil = millis() + EVOLVE_ANIM_MS;
  save();
}

void Pet::feed() {
  feedBerry(0);
}

void Pet::feedBerry(uint8_t color) {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  if (lovesBerry(color)) {
    fullness = clamp100(fullness + 35);
    joy = clamp100(joy + 10);
    heartUntil = millis() + HEART_MS;  // "le encanta!"
    berryKnown = true;                 // descubierto: se muestra en la ficha
    addBond(2);
  } else {
    fullness = clamp100(fullness + 25);
  }
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

void Pet::feedCandy() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  fullness = clamp100(fullness + 10);
  joy = clamp100(joy + 12);
  weight = clamp100(weight + 12);  // las chuches pasan factura
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

uint8_t Pet::playResult(uint8_t score) {
  if (activeSwapBlocked || ceremony != CER_NONE || isEgg()) return 0;
  // The ball game is DEFENCE's trainer now. It used to train SPEED, which was
  // moved to its own reaction test to stop playing being a stat grind -- and
  // that left DEF with no active trainer at all, only the slow passive tick.
  // Keeping the ball on the defensive stat fits it: you are stopping something
  // from getting past you.
  uint8_t before = trDef;
  uint8_t gain = score / 2;
  if (gain > 18) gain = 18;          // the same per-session ceiling as the bag
  uint16_t v = (uint16_t)trDef + gain;
  if(before < trMaxDef()) trDef = v > trMaxDef() ? trMaxDef() : (uint8_t)v;
  gain = trDef - before;
  joy = clamp100(joy + 5 + (score > 15 ? 30 : score * 2));
  // A better result should improve the reward, not multiply the price. The old
  // score-based formula cost as much as 30 energy, far above the other games.
  energy = dropTo(energy, DEF_TRAIN_ENERGY_COST, 5);
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - score * 2;  // el ejercicio quema peso
  weight = burn > 0 ? burn : 0;
  if (score >= 5) heartUntil = millis() + HEART_MS;
  if (score > gameHi) gameHi = score;  // nuevo record
  // Training bonds, and it scales with the session: a token effort is worth the
  // base, a full one is worth more. The daily cap in addBond() still stops it
  // being farmed -- this changes how fast a good session gets there, not the
  // ceiling.
  addBond((uint8_t)(2 + gain / 6));
  registerCare();
  save();
  return gain;
}

// saco de entrenamiento: los golpes entrenan la fuerza. Devuelve la subida.
uint8_t Pet::rewardTraining(uint8_t amount, uint8_t &which) {
  which = 0;
  if (activeSwapBlocked || ceremony != CER_NONE || isEgg() || !amount) return 0;
  // Only the stats with headroom are candidates.
  uint8_t room[3], n = 0;
  if (trAtk < trMaxAtk()) room[n++] = 0;
  if (trDef < trMaxDef()) room[n++] = 1;
  if (trSpe < trMaxSpe()) room[n++] = 2;
  if (!n) return 0;                     // nothing left to train
  which = room[random(n)];
  uint8_t before, capped;
  switch (which) {
    case 0: before = trAtk; capped = trMaxAtk(); trAtk = (uint8_t)min<uint16_t>(before + amount, capped); amount = trAtk - before; break;
    case 1: before = trDef; capped = trMaxDef(); trDef = (uint8_t)min<uint16_t>(before + amount, capped); amount = trDef - before; break;
    default: before = trSpe; capped = trMaxSpe(); trSpe = (uint8_t)min<uint16_t>(before + amount, capped); amount = trSpe - before; break;
  }
  // The IV-bound ceiling is never crossed: a mediocre individual not reaching
  // as far is the whole point of trMaxFor().
  save();
  return amount;
}

bool Pet::spendEnergy(uint8_t amount) {
  if (!amount || energy < amount) return false;
  energy = (uint8_t)(energy - amount);
  save();
  return true;
}

uint8_t Pet::trainSpeed(uint16_t hits) {
  if (activeSwapBlocked || ceremony != CER_NONE || isEgg()) return 0;
  uint8_t gain = min<uint16_t>(hits / 2,18);
  uint8_t before = trSpe;
  uint16_t v = (uint16_t)trSpe + gain;
  if(before < trMaxSpe()) trSpe = v > trMaxSpe() ? trMaxSpe() : (uint8_t)v;
  gain = trSpe - before;
  energy = dropTo(energy, SPE_TRAIN_ENERGY_COST, 5);
  fullness = dropTo(fullness, 4, 5);
  int burn = (int)weight - hits / 2;
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + 4);
  if (hits > spdHi) spdHi = hits;
  // Training bonds, and it scales with the session: a token effort is worth the
  // base, a full one is worth more. The daily cap in addBond() still stops it
  // being farmed -- this changes how fast a good session gets there, not the
  // ceiling.
  addBond((uint8_t)(2 + gain / 6));
  registerCare();
  save();
  return gain;
}

uint8_t Pet::trainStrength(uint16_t hits) {
  if (activeSwapBlocked || ceremony != CER_NONE || isEgg()) return 0;
  uint8_t gain = min<uint16_t>(hits / 4,18);
  uint8_t before = trAtk;
  uint16_t v = (uint16_t)trAtk + gain;
  if(before < trMaxAtk()) trAtk = v > trMaxAtk() ? trMaxAtk() : (uint8_t)v;
  gain = trAtk - before;            // lo que de verdad subio (puede topar)
  energy = dropTo(energy, ATK_TRAIN_ENERGY_COST, 5);   // cansa
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - hits / 3;  // tambien quema peso
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + 6);
  if (hits >= 20) heartUntil = millis() + HEART_MS;
  if (hits > strHi) strHi = hits;   // record de golpes
  // Training bonds, and it scales with the session: a token effort is worth the
  // base, a full one is worth more. The daily cap in addBond() still stops it
  // being farmed -- this changes how fast a good session gets there, not the
  // ceiling.
  addBond((uint8_t)(2 + gain / 6));
  registerCare();
  save();
  return gain;
}

void Pet::play() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 25);
  energy = clamp100(energy - 10);
  fullness = clamp100(fullness - 5);
  heartUntil = millis() + HEART_MS;
  addBond(2);
  registerCare();
  save();
}

// The hour off the RTC, the same source the scene uses. With no clock at all
// there is no night, so a board that has never been set never auto-sleeps.
bool Pet::isNightHour() const {
  if (!lastSeenEpoch) return false;
  int h = (int)((lastSeenEpoch / 3600) % 24);
  // The window may or may not cross midnight, and it must keep working either
  // way: with NIGHT_START 0 the old `h >= START || h < END` was true for every
  // hour of the day, which put the creature to sleep the moment the screen went
  // off at noon. sleep_test catches it, and did.
  if (NIGHT_START < NIGHT_END) return h >= NIGHT_START && h < NIGHT_END;
  return h >= NIGHT_START || h < NIGHT_END;
}

// Auto-sleep needs the screen off AND the night hours, and is re-checked every
// tick rather than only on the button, so a device put down at 21:00 nods off
// at 22:00 and gets up at 06:00 without anyone touching it.
//
// Both halves earn their place. Screen-off alone paused the game whenever you
// put the device down, and the creature is meant to get hungry during the day.
// The hour alone sent it to bed while you were still playing with it.
//
// Only what this put to sleep is woken by it: a creature the player sent to bed
// with the light button stays there until the player says otherwise.
void Pet::applyAutoSleep() {
  if (isEgg() || ceremony != CER_NONE) return;
  bool night = isNightHour();
  if (screenIsOff && night && !sleeping && sleepAuto != SLEEP_PLAYER) {
    sleeping = true;
    sleepAuto = SLEEP_AUTO;
    pendingSave = true;
  }
  // NOTHING wakes it here, and that is the whole point. Waking at 06:00 would
  // reopen the hole this exists to close: from the sleep floors, food is empty
  // by 06:15 and every stat by 07:40, so anyone who sleeps past eight would
  // find the creature ready to run away again. It sleeps until YOU are up,
  // which is the screen coming back on.
  if (!night && sleepAuto == SLEEP_PLAYER) sleepAuto = SLEEP_NONE;  // a new day
}

void Pet::setScreenOff(bool off) {
  screenIsOff = off;
  // Coming back to the device is what wakes it -- only if the device is what
  // put it to sleep. A creature sent to bed with the light stays there.
  if (!off && sleeping && sleepAuto == SLEEP_AUTO) {
    sleeping = false;
    sleepAuto = SLEEP_NONE;
  }
  applyAutoSleep();
  save();
}

void Pet::toggleLight() {
  if (ceremony != CER_NONE) return;
  if (isEgg()) return;
  sleeping = !sleeping;
  // The player's hand beats the clock until morning: waking it at 23:00 must
  // not be undone a minute later by the auto-sleep, and neither must putting
  // it to bed early.
  sleepAuto = SLEEP_PLAYER;
  save();
}

void Pet::clean() {
  if (ceremony != CER_NONE) return;
  poops = 0;
  hygiene = 100;
  addBond(1);
  registerCare();
  save();
}

void Pet::caress() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 5);
  heartUntil = millis() + HEART_MS;
  addBond(1);
  registerCare();
}

void Pet::eggTap() {
  if (!isEgg()) return;
  if (++eggTaps >= 3) hatch();
  else save();
}

PetMood Pet::mood() const {
  if (sleeping) return MOOD_SLEEPING;
  if (eating()) return MOOD_EATING;
  if (lowestStat() < 25) return MOOD_SAD;
  return MOOD_HAPPY;
}

void Pet::save() {
  // A Pet nobody opened is a scratch object -- the battle's opponent, a test
  // fixture, anything built to be read and thrown away. It shares the single
  // NVS namespace with the real creature, so letting it write means it
  // OVERWRITES the player's pet. See the note on `opened` in pet.h.
  if (!opened) return;
  ticksSinceSave = 0;
  pendingSave = false;
  prefs.putUChar("full", fullness);
  prefs.putUChar("joy", joy);
  prefs.putUChar("ene", energy);
  prefs.putUChar("hyg", hygiene);
  prefs.putUChar("poop", poops);
  prefs.putUChar("wgt", weight);
  prefs.putUChar("ivat", ivAtk);
  prefs.putUChar("ivdf", ivDef);
  prefs.putUChar("ivsp", ivSpe);
  prefs.putUChar("ivhp", ivHp);
  prefs.putUChar("tatk", trAtk);
  prefs.putUChar("tdef", trDef);
  prefs.putUChar("tspe", trSpe);
  prefs.putBytes("mvs", moves, sizeof(moves));
  prefs.putUChar("mvlv", lastLearnLevel);
  prefs.putBytes("mvq", learnQueue, sizeof(learnQueue));
  prefs.putUChar("mvqn", learnQCount);
  prefs.putUChar("avtr", avatar);
  prefs.putUChar("reg", region);
  prefs.putBytes("badgX", badgesX, sizeof(badgesX));
  prefs.putBytes("badhX", badgesHardX, sizeof(badgesHardX));
  prefs.putBytes("eggR", eggByRegion, sizeof(eggByRegion));
  prefs.putString("tnam", trainerName);
  prefs.putBool("froz", frozen);
  prefs.putUShort("badg", badges);
  prefs.putUShort("badh", badgesHard);
  prefs.putBool("bk", berryKnown);
  prefs.putBool("shy", shiny);
  prefs.putBool("eshy", eggShiny);
  prefs.putBool("stpk", starterPick);
  prefs.putUChar("evop", evoPen);
  prefs.putUChar("slpa", sleepAuto);
  prefs.putBool("rtpn", retirePending);
  prefs.putUChar("cer",ceremony);
  prefs.putBytes("dexsh", dexShinyReg, sizeof(dexShinyReg));
  prefs.putUInt("age", ageMinutes);
  prefs.putShort("dexn", speciesId);
  prefs.putUShort("form", form);
  prefs.putShort("formDex", speciesId);
  prefs.putShort("eggT2", eggTarget);
  prefs.putUChar("crack", eggTaps);
  prefs.putUChar("mist", careMistakes);
  prefs.putBool("sleep", sleeping);
  prefs.putUChar("lend", lastEnd);
  if (lastSeenEpoch) prefs.putUInt("seen", lastSeenEpoch);
#ifdef ANDROID
  if (deviceProgressEpoch) prefs.putUInt("aseen", deviceProgressEpoch);
#endif
  prefs.putBytes("dexreg", dexReg, sizeof(dexReg));
  prefs.putUShort("strk", streak);
  prefs.putUShort("bstrk", bestStreak);
  prefs.putUInt("cday", lastCareDay);
  prefs.putUInt("byeQuota", farewellQuota);
  prefs.putUChar("bond", bond);
  prefs.putUShort("medal", medals);
  prefs.putUShort("tmedal", totalMedals);
  prefs.putUShort("mstone", lastMilestone);
  prefs.putUShort("ghi", gameHi);
  prefs.putUShort("shi", strHi);
  prefs.putUShort("qhi", spdHi);
  prefs.putString("nick", nick);
  PartyMon care=storageSnapshot();
  prefs.putBytes("liveCare",&care,sizeof(care));
}

void Pet::load(bool progress) {
#ifdef ANDROID
  deviceProgressEpoch = prefs.getUInt("aseen", 0);
#endif
  fullness = prefs.getUChar("full", 80);
  joy = prefs.getUChar("joy", 80);
  energy = prefs.getUChar("ene", 80);
  hygiene = prefs.getUChar("hyg", 100);
  poops = prefs.getUChar("poop", 0);
  weight = prefs.getUChar("wgt", 0);
  if (prefs.isKey("ivat")) {
    ivAtk = prefs.getUChar("ivat", 16);
    ivDef = prefs.getUChar("ivdf", 16);
    ivSpe = prefs.getUChar("ivsp", 16);
    ivHp = prefs.getUChar("ivhp", 16);
  } else {
    // migracion desde los genes (90-110%) a IV (8-31) conservando la calidad
    // relativa: quien tenia un gen top mantiene un IV top. El IV de vitalidad
    // no existia, se tira ahora.
    ivAtk = ivFromGene(prefs.getUChar("gatk", 0));
    ivDef = ivFromGene(prefs.getUChar("gdef", 0));
    ivSpe = ivFromGene(prefs.getUChar("gspe", 0));
    ivHp = rollIV(0);
  }
  trAtk = prefs.getUChar("tatk", 0);
  trDef = prefs.getUChar("tdef", 0);
  trSpe = prefs.getUChar("tspe", 0);
  // Preserve already banked legacy training. The current IV cap limits NEW
  // rewards; loading or training must never reduce a saved individual's value.
  berryKnown = prefs.getBool("bk", false);
  shiny = prefs.getBool("shy", false);
  eggShiny = prefs.getBool("eshy", false);
  starterPick = prefs.getBool("stpk", false);
  // ko.1.1.0 removes the early-retirement evolution penalty. Clear any debt
  // carried by a save from an earlier build as soon as it is loaded.
  evoPen = 0;
  if (prefs.getUChar("evop", 0)) prefs.putUChar("evop", 0);
  sleepAuto = prefs.getUChar("slpa", SLEEP_NONE);
  retirePending = prefs.getBool("rtpn", false);
  loadBlob(prefs, "dexsh", dexShinyReg, sizeof(dexShinyReg));
  ageMinutes = prefs.getUInt("age", 0);
  if (prefs.isKey("dexn")) {
    speciesId = prefs.getShort("dexn", -1);
    eggTarget = prefs.getShort("eggT2", 4);
  } else {
    // migracion desde la version con indices de flash (0-8)
    static const uint8_t OLD2DEX[9] = { 4, 5, 6, 1, 2, 3, 7, 8, 9 };
    int8_t old = prefs.getChar("spec", -1);
    speciesId = (old >= 0 && old < 9) ? OLD2DEX[old] : -1;
    int8_t oldT = prefs.getChar("eggT", 0);
    eggTarget = (oldT >= 0 && oldT < 9) ? OLD2DEX[oldT] : 4;
  }
  eggTaps = prefs.getUChar("crack", 0);
  if(speciesId==0 || speciesId < -1 || speciesId>DEX_COUNT) {
    Serial.println("save: invalid live species; safely restoring an egg");
    speciesId=-1;pendingSave=true;
  }
  careMistakes = prefs.getUChar("mist", 0);
  sleeping = prefs.getBool("sleep", false);
  lastEnd = prefs.getUChar("lend", CER_NONE);
  ceremony=prefs.getUChar("cer",CER_NONE);
  if(ceremony>CER_RELEASE)ceremony=CER_NONE;
  if(ceremony!=CER_NONE)ceremonyUntil=millis()+CEREMONY_MS;
  loadBlob(prefs, "dexreg", dexReg, sizeof(dexReg));
  streak = prefs.getUShort("strk", 0);
  bestStreak = prefs.getUShort("bstrk", 0);
  lastCareDay = prefs.getUInt("cday", 0);
  farewellQuota = prefs.getUInt("byeQuota", 0);
  bond = prefs.getUChar("bond", 0);
  medals = prefs.getUShort("medal", 0);
  totalMedals = prefs.getUShort("tmedal", 0);
  lastMilestone = prefs.getUShort("mstone", 0);
  gameHi = prefs.getUShort("ghi", 0);
  strHi = prefs.getUShort("shi", 0);
  spdHi = prefs.getUShort("qhi", 0);
  prefs.getString("nick", nick, sizeof(nick));
  // Move IDs widened from 8 to 16 bits without renumbering the old table. Read
  // the former four-byte value one element at a time; copying it as a prefix of
  // the new array would combine pairs of old IDs into bogus 16-bit numbers.
  memset(moves, 0, sizeof(moves));
  size_t storedMoves = prefs.getBytesLength("mvs");
  if (storedMoves == sizeof(moves)) {
    prefs.getBytes("mvs", moves, sizeof(moves));
  } else if (storedMoves == MOVE_SLOTS) {
    uint8_t oldMoves[MOVE_SLOTS] = {};
    if (prefs.getBytes("mvs", oldMoves, sizeof(oldMoves)) == sizeof(oldMoves))
      for (int i = 0; i < MOVE_SLOTS; i++) moves[i] = oldMoves[i];
    pendingSave = true;
  }
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (moves[i] >= MOVE_COUNT) moves[i] = 0;   // never index MOVE_TBL with junk
  lastLearnLevel = prefs.getUChar("mvlv", 0);
  memset(learnQueue, 0, sizeof(learnQueue));
  learnQCount = prefs.getUChar("mvqn", 0);
  if (learnQCount > LEARN_QUEUE_MAX ||
      prefs.getBytesLength("mvq") != sizeof(learnQueue) ||
      prefs.getBytes("mvq", learnQueue, sizeof(learnQueue)) != sizeof(learnQueue)) {
    learnQCount = 0;
    memset(learnQueue, 0, sizeof(learnQueue));
  }
  for (uint8_t i = 0; i < learnQCount; i++) {
    if (!learnQueue[i] || learnQueue[i] >= MOVE_COUNT) {
      learnQCount = 0;
      memset(learnQueue, 0, sizeof(learnQueue));
      pendingSave = true;
      break;
    }
  }
  frozen = prefs.getBool("froz", false);
  avatar = prefs.getUChar("avtr", 0);
  // Absent on a Kanto-only save, which leaves both arrays zeroed -- exactly
  // "no Johto or Hoenn badges yet".
  loadBlob(prefs, "badgX", badgesX, sizeof(badgesX));
  loadBlob(prefs, "badhX", badgesHardX, sizeof(badgesHardX));
  region = prefs.getUChar("reg", REGION_ALL);
  if (region >= REGION_COUNT) region = REGION_ALL;
  // A save from the Kanto-only build has neither key; getBytes leaves the
  // array at its zeroed initialiser, which is exactly "nothing remembered".
  loadBlob(prefs, "eggR", eggByRegion, sizeof(eggByRegion));
  if (isEgg()) {
    // A waiting egg may have been rolled by an older build before an
    // incomplete evolution family was removed from the pool. Replace only
    // that unborn result; a living creature already in the save is preserved.
    uint8_t oldTier = eggRarity();
    for (uint8_t r = 0; r < REGION_COUNT; r++) {
      int16_t remembered = eggByRegion[r];
      if (remembered && (remembered < 1 || remembered > DEX_COUNT ||
                         !speciesCanHatch(remembered))) {
        eggByRegion[r] = 0;
        pendingSave = true;
      }
    }
    if (eggTarget < 1 || eggTarget > DEX_COUNT || !speciesCanHatch(eggTarget)) {
      eggTarget = rollInRegion(region, oldTier);
      eggByRegion[region] = eggTarget;
      pendingSave = true;
    }
  }
  prefs.getString("tnam", trainerName, sizeof(trainerName));
  if (avatar >= AVATAR_COUNT) avatar = 0;   // a save from when there were four
  badges = prefs.getUShort("badg", 0);
  badgesHard = prefs.getUShort("badh", 0);
  if (!isEgg() && moveCount() == 0) {
    // A save from before moves existed, or one created while a species had no
    // level-1 entry, needs the set it should already have. Do not require
    // lastLearnLevel == 0: the old Wimpod data advanced that marker to 1 while
    // leaving every slot empty.
    relearnFromLevel();
    lastLearnLevel = level();
    pendingSave = true;
  }
  // Pending offers are persisted now: a long offline catch-up can unlock more
  // than eight moves, and closing the app on the first dialog must not discard
  // the rest after lastLearnLevel has already advanced.
  form = prefs.getShort("formDex", -1) == speciesId ? prefs.getUShort("form", 0) : 0;
  PartyMon care, current=storageSnapshot();
  // The legacy live fields remain authoritative. Ignore stale metadata after
  // an old version, partial write or external save tool changes this individual.
  if(prefs.getBytesLength("liveCare")==sizeof(care) &&
     prefs.getBytes("liveCare",&care,sizeof(care))==sizeof(care) &&
     !memcmp(&care,&current,36) && !memcmp(care.care+20,&ageMinutes,4) && care.care[0]==1) {
    // Only the extra counters live here; never overwrite newer ordinary keys.
    const uint8_t *c=care.care;
    bondToday=c[10]; mistakeCooldown=c[11]; neglectTicks=c[12]; evoDeclinedLv=c[13];
    memcpy(&goodTicks,c+16,2); memcpy(&farDeclinedAge,c+24,4);
    uint32_t day=0; memcpy(&day,c+28,4);
    if(today() && day!=today()) bondToday=0;
  }
  if(progress) checkLearnGates();
  // Unknown form IDs are retained for forward-compatible saves.
  // siembra: la mascota actual cuenta como criada (guardados antiguos)
  if (speciesId >= 1) registerSpecies(speciesId);
}
