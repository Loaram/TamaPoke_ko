// Shared energy through real capture, active swapping and torn-write recovery.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "save.h"
#include "wild.h"
#include <cstdio>
uint32_t g_seed=370; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);} int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); bool startWildBattle(bool); void finishWildBattle();
extern Pet pet; extern bool battleOpen,btlWild,btlWon,btlOver;
extern uint8_t wildResult; extern int16_t wildDex;
static int bad=0;
static void ck(bool ok,const char*s){printf("%s %s\n",ok?"PASS":"FAIL",s);bad+=!ok;}
static void clearOffers(){while(pet.hasLearnOffer())pet.declineLearn();}
static PartyMon member(int dex){
  PartyMon m;m.dex=dex;m.level=60;m.ivAtk=20;m.ivDef=21;m.ivSpe=22;m.ivHp=23;
  m.moves[0]=1;return m;
}
static void fresh(uint8_t energy){
  nvsFailKey().clear();nvs().clear();activeSwapBlocked=tradeStorageBlocked=false;
  pet.begin();party.begin();pet.dbgHatchAs(6,false);clearOffers();
  pet.energy=energy;pet.saveNow();
  battleOpen=btlWild=false;wildResult=WILD_RESULT_NONE;
}
static void reboot(){pet.begin();party.begin();clearOffers();}
int main(){
  nvs().clear();setup();
  // Every banked value (including zero/full and legacy no-care records) is
  // ignored on activation. Keep other care/individual metadata unchanged.
  bool matrix=true;
  for(bool box:{false,true})for(int stored=0;stored<=100;stored++){
    fresh(17);Pet banked;banked.reviveFrom(member(25));
    banked.energy=stored;banked.fullness=46;banked.bond=39;
    PartyMon incoming=banked.storageSnapshot();
    if(box)party.box[0]=incoming;else party.slots[0]=incoming;
    party.save();
    matrix &= party.swapActive(pet,box,0)&&pet.energy==17&&pet.fullness==46&&pet.bond==39;
    clearOffers();matrix &= pet.spendEnergy(7);
    matrix &= party.swapActive(pet,box,0)&&pet.energy==10;
    reboot();matrix &= pet.energy==10&&!activeSwapBlocked;
  }
  ck(matrix,"202 party/box swaps ignore all banked energy values and retain care, spending and reloads");

  for(bool box:{false,true}){
    fresh(100);
    if(box){for(auto&m:party.slots)m=member(25);party.save();}
    int captures=0;bool ok=true;
    while(pet.energy>=WILD_ENERGY_COST && captures<100){
      uint8_t expected=pet.energy-WILD_ENERGY_COST;
      ok &= startWildBattle((captures&1)!=0);
      // Force a winning catch but keep the real capture/storage code path.
      wildDex=10;btlWon=btlOver=true;
      bool found=false;
      for(uint32_t seed=1;seed<10000;seed++){
        g_seed=seed;
        if(wildCaptureNow(wildCatchRateForDex(wildDex),pet.collectibleRegisteredCount())){
          g_seed=seed;found=true;break;
        }
      }
      ok &= found;finishWildBattle();
      ok &= wildResult==(box?WILD_RESULT_BOX:WILD_RESULT_PARTY)&&pet.energy==expected;
      battleOpen=btlWild=false;wildResult=WILD_RESULT_NONE;
      ok &= party.swapActive(pet,box,box?captures:0)&&pet.energy==expected;
      if(!box)party.swapPartyBox(0,captures); // keep the next catch's party slot free
      reboot();ok &= pet.energy==expected&&!activeSwapBlocked;
      captures++;
    }
    auto before=nvs();
    ok &= captures==100/WILD_ENERGY_COST && pet.energy==100%WILD_ENERGY_COST;
    ok &= !startWildBattle(false)&&!startWildBattle(true)&&nvs()==before;
    ck(ok,box?"capture-to-box/bring/reboot loop stops at shared energy limit":"capture-to-party/bring/reboot loop stops at shared energy limit");
  }

  fresh(WILD_ENERGY_COST);party.slots[0]=member(25);party.save();
  ck(startWildBattle(false)&&pet.energy==0,"exact exploration cost starts once and reaches zero");
  battleOpen=btlWild=false;wildResult=WILD_RESULT_NONE;
  ck(party.swapActive(pet,false,0)&&pet.energy==0,"legacy/newly received companion does not refill zero energy");
  clearOffers();pet.newEgg();reboot();
  ck(pet.isEgg()&&pet.energy==0,"new egg and restart do not refill the shared pool");
  pet.dbgHatchAs(6,false);clearOffers();pet.sleeping=true;pet.dbgTick();
  ck(pet.energy==8,"sleep still recovers eight shared energy per minute");
  pet.sleeping=false;pet.saveNow();reboot();
  ck(pet.energy==8,"recovered energy persists");
  uint8_t backup[SAVE_MAX_BYTES];size_t n=saveExport(backup,sizeof(backup));
  nvs().clear();bool imported=saveImport(backup,n);reboot();
  ck(imported&&pet.energy==8&&backup[4]==SAVE_VERSION,"whole-save transfer preserves shared energy without a format change");

  // Fail each important write after the atomic roster journal commits. The
  // incoming record must contain the shared value, never the banked value.
  bool faults=true;
  for(bool box:{false,true})for(const char*key:{"dexn","ene","age","ivat","liveCare"}){
    fresh(12);pet.ivAtk=31;pet.saveNow();PartyMon m=member(25);
    if(box)party.box[0]=m;else party.slots[0]=m;party.save();
    if(!strcmp(key,"ene"))nvs()["ene"][0]=80; // stale/partially written live key
    nvsFailKey()=key;
    bool committed=party.swapActive(pet,box,0);
    bool blocked=activeSwapBlocked;
    // With no extra care counters, ordinary keys may already be sufficient
    // despite a failed optional liveCare write; either safe path is valid.
    bool expectedBlock=strcmp(key,"liveCare")!=0;
    faults &= committed&&(!expectedBlock||blocked);
    nvsFailKey().clear();reboot();
    faults &= !activeSwapBlocked&&pet.speciesId==25&&pet.energy==12;
    if(!committed||(expectedBlock&&!blocked)||activeSwapBlocked||pet.speciesId!=25||pet.energy!=12)
      printf("fault detail box=%d key=%s committed=%d blocked=%d recoveredBlocked=%d dex=%d energy=%u\n",
             box,key,committed,blocked,activeSwapBlocked,pet.speciesId,pet.energy);
    reboot();faults &= pet.energy==12;
  }
  ck(faults,"ten interrupted party/box commits recover shared energy, never a stale refill");
  fresh(12);party.slots[0]=member(25);party.save();auto before=nvs();
  nvsFailKey()="rosterF";
  ck(!party.swapActive(pet,false,0)&&pet.energy==12&&nvs()==before,"failed roster commit preserves energy and both individuals");
  nvsFailKey().clear();pet.factoryReset();pet.begin();
  ck(pet.energy==80,"actual factory reset creates the initial energy pool");
  return bad?1:0;
}
