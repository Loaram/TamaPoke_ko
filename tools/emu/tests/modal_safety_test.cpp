#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "wild.h"
#include <cstdio>
#include <new>
uint32_t g_seed=35377;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void onTap(int16_t,int16_t);void onSwipe(int);void onSwipeV(int);
bool startWildBattle(bool);
extern Pet pet;
extern bool partyOpen,boxOpen,releaseConfirm,playerOpen,kbOpen,cardOpen,menuOpen,trainOpen;
extern bool battleOpen,exploreOpen,formsOpen,movePickOpen,galleryOpen,clockOpen,pickOpen,lanOpen,gymOpen;
extern uint8_t partyDetail,playerPage,boxSwapFrom,boxPage,nameLen;extern uint16_t boxDetail,boxSel;
extern char nameBuf[];extern uint8_t exploreRegion,exploreNotice,wildResult;extern bool btlWild;
static int bad=0;
static void ck(bool ok,const char*s){printf("%s %s\n",ok?"PASS":"FAIL",s);bad+=!ok;}
static PartyMon mon(int dex){PartyMon m;m.dex=dex;m.level=60;m.ivAtk=m.ivDef=m.ivSpe=m.ivHp=25;m.moves[0]=1;return m;}
static void fresh(){
 nvs().clear();activeSwapBlocked=tradeStorageBlocked=false;
 pet.~Pet();new(&pet)Pet();party.~Party();new(&party)Party();party.begin();pet.begin();
 pet.dbgHatchAs(6,false);while(pet.hasLearnOffer())pet.declineLearn();
 pet.fullness=pet.joy=pet.energy=pet.hygiene=100;pet.saveNow();
 party.slots[0]=mon(25);party.slots[1]=mon(133);party.save();
 partyOpen=boxOpen=releaseConfirm=playerOpen=kbOpen=cardOpen=menuOpen=trainOpen=false;
 battleOpen=exploreOpen=formsOpen=movePickOpen=galleryOpen=clockOpen=pickOpen=lanOpen=gymOpen=false;
 partyDetail=playerPage=boxSwapFrom=boxPage=0;boxDetail=boxSel=0;btlWild=false;wildResult=0;
}
int main(){
 nvs().clear();setup();fresh();playerOpen=true;pet.renameTrainer("TEST");
 onTap(233,50);ck(kbOpen,"trainer-name tap opens keyboard");
 auto len=nameLen;onTap(66,172);
 ck(nameLen==len+1&&nameBuf[len]=='A',"first visible A reaches keyboard");
 onTap(66,172);ck(nameLen==len+2,"second A is a second character, not the first");
 onSwipe(-1);onSwipeV(-1);
 ck(playerOpen&&playerPage==0&&kbOpen,"keyboard gestures do not page or close the hidden trainer screen");
 onTap(390,384);ck(!kbOpen&&!strcmp(pet.trainerName,"TESTAA")&&playerOpen,"OK saves the name and returns to its trainer screen");
 for(bool vertical:{false,true}){
  fresh();partyOpen=true;onTap(100,120);onTap(310,360);
  ck(partyDetail==1&&releaseConfirm,"first Pokemon opens its release confirmation");
  if(vertical)onSwipeV(-1);else onSwipe(-1);
  ck(!partyDetail&&!releaseConfirm,"leaving party detail cancels the release intent");
  onTap(280,120);ck(partyDetail==2&&!releaseConfirm,"second Pokemon starts with no release confirmation");
  ck(party.slots[0].dex==25&&party.slots[1].dex==133,"navigation deletes neither individual");
 }
 for(int lock=0;lock<3;lock++){
  fresh();
  if(lock==0){nvsFailKey()="rosterF";party.boxAdd(mon(150));nvsFailKey().clear();}
  else if(lock==1)tradeStorageBlocked=true;
  else activeSwapBlocked=true;
  exploreRegion=REGION_ALL;exploreOpen=true;auto before=nvs();auto seed=g_seed;
  ck(!startWildBattle(false)&&!startWildBattle(true)&&exploreNotice==3&&pet.energy==100&&nvs()==before&&g_seed==seed,
     "read-only, trade and recovery locks refuse both Explore modes before charging or rolling");
 }
 fresh();exploreRegion=REGION_ALL;
 ck(startWildBattle(false)&&pet.energy==100-WILD_ENERGY_COST,"healthy storage allows Explore at its configured cost");
 return bad?1:0;
}
