#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "korean_text.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=352;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void render();void onTap(int16_t,int16_t);void onSwipe(int);void onSwipeV(int);
bool learnDialogVisible();void renderSwapRecovery();
extern Pet pet;extern KoreanCanvas *gfx;
extern bool partyOpen,boxOpen,releaseConfirm,trainOpen;
extern uint8_t partyDetail,boxSwapFrom;extern uint16_t boxDetail,boxSel;
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);bad+=!b;}
static void clearOffers(){while(pet.hasLearnOffer())pet.declineLearn();}
static PartyMon mon(){PartyMon m;m.dex=25;m.level=60;m.ivAtk=20;m.ivDef=21;m.ivSpe=22;m.ivHp=23;m.moves[0]=1;m.trAtk=12;m.trDef=23;m.trSpe=34;return m;}
static void fresh(){
 nvs().clear();activeSwapBlocked=tradeStorageBlocked=false;
 pet.begin();pet.dbgHatchAs(6,false);clearOffers();party.begin();
 pet.ivAtk=pet.ivDef=pet.ivSpe=pet.ivHp=31;pet.saveNow();
 party.slots[0]=mon();party.box[0]=mon();party.box[0].ivHp=24;party.save();
 partyOpen=boxOpen=releaseConfirm=trainOpen=false;partyDetail=boxDetail=boxSel=boxSwapFrom=0;
}
static void shot(const char*name){render();FILE*f=fopen(name,"wb");if(!f)abort();fprintf(f,"P6\n466 466\n255\n");
 for(int i=0;i<466*466;i++){auto c=gfx->buffer()[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);}
int main(){
 nvs().clear();setup();
 for(bool box:{false,true}){
  fresh();pet.dbgHatchAs(4,false);pet.ageMinutes=0;pet.lastLearnLevel=1;pet.relearnFromLevel();clearOffers();
  partyOpen=true;boxOpen=box;partyDetail=box?0:1;boxDetail=box?1:0;
  for(int i=0;i<2000&&!pet.hasLearnOffer();i++){pet.sleeping=false;pet.fullness=pet.joy=pet.energy=pet.hygiene=100;pet.dbgTick();}
  ck(pet.hasLearnOffer()&&!learnDialogVisible(),"level-up move is deferred while storage details are visible");
  auto q=pet.learnQCount;auto dex=pet.speciesId;shot(box?"bring-box-blocked.ppm":"bring-party-blocked.ppm");
  onTap(box?118:160,360);
  ck(pet.speciesId==dex&&pet.learnQCount==q&&learnDialogVisible(),"Bring opens the real move decision without rejecting or accepting an unseen move");
  shot("bring-learn.ppm");onTap(160,350);
  ck(pet.learnQCount==q-1,"only a separate tap on the visible Skip resolves the offer");
 }
 fresh();pet.learnQueue[0]=1;pet.learnQCount=1;trainOpen=true;
 ck(!learnDialogVisible(),"training menu cannot dispatch taps to a hidden learning dialog");trainOpen=false;clearOffers();
 fresh();party.slots[0].trAtk=100;party.slots[0].ivAtk=0;party.save();
 ck(party.swapActive(pet,false,0)&&!activeSwapBlocked,"legacy over-cap training does not leave an unfinished swap");
 ck(pet.trainStrength(72)==0&&pet.trAtk==100,"training at a legacy value above the current cap never reduces it");
 pet.begin();ck(pet.trAtk==100&&!activeSwapBlocked,"legacy training survives reload without clamping");
 // Verified ordinary keys, not gameplay-adjusted load results, finish a swap.
 fresh();Pet banked;banked.reviveFrom(mon());auto m=banked.storageSnapshot();m.care[14]=1;party.slots[0]=m;party.save();
 ck(party.swapActive(pet,false,0)&&!activeSwapBlocked,"pending level-up checks cannot make successful disk writes look failed");
 // Power loss / failed write at every relevant ordinary-key boundary.
 for(const char*key:{"dexn","tatk","tdef","tspe","ivat","age","liveCare"}){
  fresh();nvsFailKey()=key;party.swapActive(pet,false,0);nvsFailKey().clear();
  if(activeSwapBlocked){
   auto before=pet.storageSnapshot();pet.trainStrength(40);pet.playResult(20);pet.trainSpeed(20);pet.dbgTick();
   auto after=pet.storageSnapshot();ck(!memcmp(&before,&after,sizeof(before)),"unfinished swap freezes training and growth");
   auto disk=nvs();onTap(160,360);onSwipe(1);onSwipeV(1);ck(nvs()==disk&&activeSwapBlocked,"recovery overlay blocks hidden Bring and swipe actions");
   shot("swap-recovery.ppm");onTap(233,330);
  }
  ck(!activeSwapBlocked&&pet.speciesId==25&&pet.trAtk==12&&pet.trDef==23&&pet.trSpe==34,"Retry repairs interrupted save without losing incoming training");
 }
 // Reproduce an OLD 3.5.1 journal with newer completed training, then upgrade.
 fresh();nvsFailKey()="dexn";party.swapActive(pet,false,0);nvsFailKey().clear();
 activeSwapBlocked=false; // 3.5.1 did not have the progression interlock
 pet.trainStrength(40);pet.playResult(20);pet.trainSpeed(20);pet.saveNow();
 ck(pet.trAtk==22&&pet.trDef==33&&pet.trSpe==44,"old-version fixture contains trained values newer than its journal");
 pet.begin();party.begin();
 ck(!activeSwapBlocked&&pet.trAtk==22&&pet.trDef==33&&pet.trSpe==44,"upgrade preserves newer verified training instead of replaying 12/23/34");
  pet.begin();ck(pet.trAtk==22&&pet.trDef==33&&pet.trSpe==44,"recovery is stable over a second restart");
 fresh();nvsFailKey()="dexn";party.swapActive(pet,false,0);nvsFailKey().clear();
 activeSwapBlocked=false;pet.trainStrength(40);strcpy(pet.nick,"RENAMED");pet.saveNow();auto renamed=nvs();pet.begin();
 ck(activeSwapBlocked&&nvs()==renamed&&pet.trAtk==22&&!strcmp(pet.nick,"RENAMED"),"unproven renamed legacy individual is preserved rather than overwritten");
 // If two individuals share the same immutable signature, do not guess.
 fresh();party.box[0]=mon();party.save();nvsFailKey()="dexn";party.swapActive(pet,false,0);nvsFailKey().clear();
 activeSwapBlocked=false;pet.trainStrength(40);pet.saveNow();auto before=nvs();pet.begin();
 ck(activeSwapBlocked&&nvs()==before&&pet.trAtk==22,"ambiguous old saves remain intact and visibly paused, never guessed or rolled back");
 return bad?1:0;
}
