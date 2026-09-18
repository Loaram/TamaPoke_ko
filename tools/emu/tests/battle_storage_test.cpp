#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "wild.h"
#include <cstdio>
#include <type_traits>
static_assert(!std::is_copy_constructible<Pet>::value && !std::is_move_constructible<Pet>::value,
              "Pet must not duplicate its owning storage handle");
uint32_t g_seed=393;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();bool startWildBattle(bool);void finishWildBattle();
extern Pet pet;
extern bool battleOpen,btlWild,btlWon,btlOver,exploreOpen;
extern uint8_t wildResult,wildLevel,wildIvAtk,wildIvDef,wildIvSpe,wildIvHp;
extern int16_t wildDex;
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);bad+=!b;}
int main(){
  {Preferences owner;owner.begin("tamapoke");owner.putUInt("probe",1);
   {Preferences copied=owner;}
   owner.putUInt("probe",2);Preferences reader;reader.begin("tamapoke",true);
   ck(reader.getUInt("probe")==1,"destroying a Preferences copy invalidates its owner's handle");
   ck(!owner.begin("tamapoke"),"begin alone cannot reopen a stale started handle");}
  nvs().clear();setup();
  pet.dbgHatchAs(485,false);pet.learnQCount=0;pet.energy=100;pet.saveNow();
  PartyMon partner;partner.dex=890;partner.form=10359;partner.level=100;
  partner.ivAtk=21;partner.ivDef=22;partner.ivSpe=23;partner.ivHp=24;partner.moves[0]=1;
  party.slots[0]=partner;
  for(int i=1;i<PARTY_SLOTS;i++){party.slots[i]=partner;party.slots[i].dex=25;party.slots[i].form=0;}
  party.save();
  ck(party.swapActive(pet,false,0)&&!activeSwapBlocked,"Heatran to Eternamax swap commits before battle");
  pet.learnQCount=0;pet.sleeping=false;pet.energy=100;pet.saveNow();
  ck(startWildBattle(false),"real exploration builds the battle squad");
  pet.trAtk=17;pet.saveNow();
  ck(pet.storedIndividualMatches(),"live save remains writable after battle squad temporary is destroyed");
  btlWon=btlOver=true;wildResult=WILD_RESULT_NONE;wildDex=84;wildLevel=60;
  wildIvAtk=21;wildIvDef=22;wildIvSpe=23;wildIvHp=24;
  for(uint32_t seed=1;seed<100000;seed++){
    g_seed=seed;if(wildCaptureNow(wildCatchRateForDex(wildDex),pet.collectibleRegisteredCount())){g_seed=seed;break;}
  }
  finishWildBattle();
  ck(party.boxCount()==1&&party.box[0].dex==84,"capture commits to box while party is full");
  battleOpen=btlWild=btlOver=exploreOpen=false;pet.learnQCount=0;
  ck(party.swapActive(pet,false,0)&&!activeSwapBlocked&&pet.speciesId==485,"return to Heatran finishes without save protection overlay");
  ck(pet.storedIndividualMatches(),"returned individual is verified on disk without a reboot");
  // A second, unrelated pair pins the user's independent reproduction.
  pet.dbgHatchAs(1007,false);pet.learnQCount=0;pet.saveNow();
  partner.dex=792;partner.form=0;party.slots[0]=partner;party.save();
  ck(party.swapActive(pet,false,0)&&!activeSwapBlocked,"Koraidon to Lunala swap commits");
  pet.learnQCount=0;pet.sleeping=false;pet.energy=100;pet.saveNow();
  wildResult=WILD_RESULT_NONE;
  ck(startWildBattle(false),"second species pair starts exploration");
  btlWon=btlOver=true;wildDex=191;wildResult=WILD_RESULT_NONE;
  for(uint32_t seed=1;seed<100000;seed++){
    g_seed=seed;if(wildCaptureNow(wildCatchRateForDex(wildDex),pet.collectibleRegisteredCount())){g_seed=seed;break;}
  }
  finishWildBattle();
  ck(party.boxCount()==2&&party.box[1].dex==191,"second capture remains in box");
  battleOpen=btlWild=btlOver=exploreOpen=false;pet.learnQCount=0;
  ck(party.swapActive(pet,false,0)&&!activeSwapBlocked&&pet.speciesId==1007,"return to Koraidon commits without reboot");
  pet.begin();party.begin();
  ck(!activeSwapBlocked&&pet.speciesId==1007&&party.boxCount()==2,"reload preserves both captured Pokemon and returned companion");
  return bad?1:0;
}
