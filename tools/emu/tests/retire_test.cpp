// Retiring a creature on demand without delaying the next creature.
//
// An early retirement still gives the creature up for good and does not grant
// the farewell egg bonus. Since ko.1.1.0 it carries no evolution penalty.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
uint32_t g_seed=41; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
static uint32_t gNow = 1;
uint32_t millis(){ return gNow; }
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static void finish(Pet &p, Party &q){
  gNow += CEREMONY_MS + 1000;
  p.update(gNow);
  if (p.endedKind != CER_NONE) {
    if (!q.add(p.endedMon)) q.boxAdd(p.endedMon);
    p.endedKind = CER_NONE;
  }
}

static void young(Pet &p, int16_t dex, uint8_t lvl){
  p.dbgHatchAs(dex,false);
  p.ageMinutes = (uint32_t)(lvl-1)*MINUTES_PER_LEVEL;
  p.fullness=p.joy=p.energy=p.hygiene=100;
}

int main(){
  // --- an early retirement is allowed, but the creature is not banked
  {
    Pet p; Party q; p.begin(); q.begin();
    young(p, 4, 21);
    ck(!p.canFarewellNow(), "a young creature is not offered a farewell");
    ck(p.canRetireNow(), "but it can be retired on demand");
    ck(!p.retireIsFree(), "and the game knows this is an early retirement");
    p.startRetire();
    ck(p.ceremony == CER_FAREWELL, "retiring runs the farewell ceremony");
    ck(p.retireIsEarly(), "and remembers this one was early");
    finish(p, q);
    ck(q.count() == 0, "an early retirement does NOT bank the creature");
    ck(p.endedKind == CER_NONE, "so nothing is left waiting for a party slot");
    ck(p.lastEnd == CER_RELEASE,
       "and the next egg is neutral, not blessed -- or this is a shiny farm");
    ck(p.isEgg(), "and a new egg is waiting");
    ck(p.evoPenalty() == 0, "the next creature has no evolution penalty");

    // The next creature evolves at its ordinary threshold, with no hidden day.
    p.dbgHatchAs(1, false);
    p.fullness=p.joy=p.energy=p.hygiene=100;
    const DexEntry &d = DEX_TBL[p.speciesId];
    p.ageMinutes = (uint32_t)(d.evolveLevel - 1) * MINUTES_PER_LEVEL - 1;
    ck(!p.canEvolveNow(), "it does not evolve one minute before its normal time");
    p.ageMinutes++;
    ck(p.canEvolveNow(), "and evolves at its normal time after an early retirement");
  }

  // --- retiring one that has earned its farewell still banks it
  {
    Pet p; Party q; p.begin(); q.begin();
    p.dbgHatchAs(6, false);
    p.ageMinutes = FAREWELL_AGE_MIN + 60;
    p.fullness=p.joy=p.energy=p.hygiene=100;
    ck(p.canFarewellNow() && p.retireIsFree(),
       "a final-form creature past two days has earned its farewell");
    p.startRetire();
    ck(!p.retireIsEarly(), "an earned retirement is not early");
    finish(p, q);
    ck(p.evoPenalty() == 0, "an earned retirement also leaves no penalty");
    ck(q.count() == 1 && q.slots[0].dex == 6,
       "an earned retirement banks the creature like a normal farewell");
    ck(p.lastEnd == CER_FAREWELL, "and still blesses the next egg");
  }

  // --- repeated early retirements never create or accumulate a penalty
  {
    Pet p; Party q; p.begin(); q.begin();
    for (int i = 0; i < PARTY_SLOTS; i++) q.releaseAt(i);
    for (int i = 0; i < 3; i++) {
      young(p, 4, 10);
      p.startRetire();
      finish(p, q);
      ck(p.evoPenalty() == 0, "early retirement leaves no evolution debt");
    }
    ck(q.count() == 0, "and none of the early-retired creatures was banked");
  }

  // --- old saves carrying the removed debt are migrated immediately
  {
    Preferences legacy; legacy.begin("tamapoke", false);
    legacy.putUChar("evop", 36);
    Pet migrated; migrated.begin();
    ck(migrated.evoPenalty() == 0, "a legacy evolution debt is cleared on load");
    ck(legacy.getUChar("evop", 255) == 0, "the cleared debt is written back to NVS");
  }

  // --- what cannot be retired
  {
    Pet p; Party q; p.begin(); q.begin();
    p.newEgg();
    ck(!p.canRetireNow(), "an egg cannot be retired");
    young(p, 4, 10);
    p.frozen = true;
    ck(!p.canRetireNow(), "nor can a revived companion");
    p.frozen = false;
    p.sleeping = true;
    ck(!p.canRetireNow(), "nor one that is asleep");
  }

  // --- the no-penalty state survives a reload
  {
    Pet p; Party q; p.begin(); q.begin();
    young(p, 7, 12);
    p.startRetire(); finish(p, q);
    Pet again; again.begin();
    ck(again.evoPenalty() == 0, "no evolution debt appears after reload");
    ck(again.isEgg(), "and the waiting egg survives reload");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}
