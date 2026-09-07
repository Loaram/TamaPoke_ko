#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "tools/android/game_lifecycle.h"
#include <cstdio>
#include <new>
uint32_t g_seed=353;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
uint32_t millis(){return 0;}void FakeESP::restart(){}
int FakeSerial::available(){return 0;}String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;static Pet live;
static constexpr uint32_t epoch=500*86400UL+20*3600UL;
static void ck(bool ok,const char*s){printf("%s %s\n",ok?"PASS":"FAIL",s);bad+=!ok;}
static void clearOffers(Pet&p){while(p.hasLearnOffer())p.declineLearn();}
static void fresh(){
 nvsFailKey().clear();nvs().clear();activeSwapBlocked=tradeStorageBlocked=false;
 live.~Pet();new(&live)Pet();party.~Party();new(&party)Party();party.begin();live.begin();
 live.dbgHatchAs(6,false);clearOffers(live);live.ivAtk=31;live.rename("OUTGOING");
 live.updateDeviceClock(0,epoch,epoch);live.saveNow();
 PartyMon m;m.dex=25;m.level=60;m.ivAtk=20;m.ivDef=21;m.ivSpe=22;m.ivHp=23;
 m.trAtk=12;m.trDef=23;m.trSpe=34;m.moves[0]=1;strcpy(m.nick,"INCOMING");
 party.slots[0]=m;party.save();
}
static void rebaseStartupClock(int offsetHours){
 // Same ordering as Android startup, including UTC persistence before setup().
 Preferences clock;uint32_t utcNow=epoch+7200,localNow=utcNow+offsetHours*3600;
 auto savedUtc=clock.getUInt("aseen",0);uint32_t elapsed=utcNow-savedUtc;
 clock.putUInt("seen",localNow-elapsed);clock.putUInt("aseen",utcNow);
}
int main(){
 for(int offset:{0,1,-9,9,24,-24}){
  fresh();nvsFailKey()="dexn";party.swapActive(live,false,0);nvsFailKey().clear();
  activeSwapBlocked=false; // legacy 3.5.1 permitted training after journal failure
  live.trainStrength(40);live.playResult(20);live.trainSpeed(20);live.saveNow();
  rebaseStartupClock(offset);live.begin();party.begin();
  ck(!activeSwapBlocked&&live.trAtk==22&&live.trDef==33&&live.trSpe==44,
     "timezone/date changes retain newer legacy training when ordinary keys agree");
  live.begin();party.begin();
  ck(!activeSwapBlocked&&live.trAtk==22&&live.trDef==33&&live.trSpe==44,
     "timezone recovery remains stable over another restart");
 }
 // Date normalization must NOT mask an actual partial ordinary-key write.
 fresh();auto complete=nvs();PartyMon stored;
 for(const char*key:{"tatk","tdef","tspe","ivat","ivdf","ivsp","ivhp","age","dexn","nick","mvs","sleep"}){
  nvs()=complete;auto &bytes=nvs()[key];if(!bytes.empty())bytes[0]^=1;
  ck(!live.readStoredSnapshot(stored),"snapshot validation still rejects a mismatched individual field");
 }
 nvs()=complete;ck(live.readStoredSnapshot(stored),"complete snapshot still verifies after fault matrix");
 fresh();nvsFailKey()="dexn";party.swapActive(live,false,0);live.begin();party.begin();
 ck(activeSwapBlocked,"persistent fault survives restart as a blocking recovery state");
 auto before=nvs();auto care=live.storageSnapshot();auto seen=live.lastSeenEpoch;
 live.syncClock(epoch+3600);live.updateDeviceClock(0,epoch+3600,epoch+3600,true);live.dbgTick();
 AndroidGameLifecycle app;app.start();
 ck(!app.checkpoint(live,0,epoch+3600,epoch+3600),"Android checkpoint refuses unresolved recovery");
 app.suspend(live,0,epoch+3600,epoch+3600);app.resume(live,0,epoch+7200,epoch+7200);
 auto after=live.storageSnapshot();
 ck(nvs()==before&&!memcmp(&care,&after,sizeof(care))&&live.lastSeenEpoch==seen,
    "startup, offline, pause/resume and checkpoints cannot progress or rewrite blocked recovery");
 nvsFailKey().clear();live.begin();party.begin();
 ck(!activeSwapBlocked&&live.trAtk==12&&live.trDef==23&&live.trSpe==34,"repair preserves committed incoming training");
 auto age=live.ageMinutes;live.resumeProgressClock(1000,epoch+7200,epoch+7200);
 live.updateDeviceClock(1000,epoch+7200,epoch+7200);
 ck(live.ageMinutes==age,"time in recovery is not replayed after a successful Retry");
 live.updateDeviceClock(61000,epoch+7260,epoch+7260);
 ck(live.ageMinutes==age+1,"normal clock resumes with exactly one new minute");
 return bad?1:0;
}
