#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "trade.h"
#include "link.h"
#include "nvs_file.h"
#include "../../android/game_lifecycle.h"
#include <cstdio>
#include <new>
#include <vector>
uint32_t g_seed=390;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
static bool restarted=false;
void FakeESP::restart(){restarted=true;}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void onTap(int16_t,int16_t);void clockTap(int16_t,int16_t);void openClock();
void loop();void onSwipe(int);void onSwipeV(int);
extern Pet pet;extern Link lan;extern bool clockOpen,battleOpen,lanOpen,tradeOpen;
extern uint8_t resetPanel;extern uint32_t resetConfirmAt;
static int bad=0;
static void ck(bool ok,const char*s){printf("%s %s\n",ok?"PASS":"FAIL",s);bad+=!ok;}
static void fresh(){
  nvs().clear();nvsFailKey().clear();saveResetLocked=activeSwapBlocked=tradeStorageBlocked=false;
  pet.~Pet();new(&pet)Pet();party.~Party();new(&party)Party();
  trade=Trade();party.begin();pet.begin();pet.dbgHatchAs(6,true);
  while(pet.hasLearnOffer())pet.declineLearn();
  pet.streak=10;pet.bestStreak=12;pet.trAtk=25;pet.badges=255;pet.saveNow();
  PartyMon m;m.dex=25;m.level=60;
  for(auto &s:party.slots)s=m;for(auto &s:party.box)s=m;party.save();
  Preferences p;p.begin("tamapoke");p.putUChar("lang",6);p.putBool("snd",false);p.putUChar("vol",3);
  p.putUInt("eggQuota",400*4+2);p.putUInt("aseen",12345);p.putULong64("rosterSD",123);
  trade.load(party);battleOpen=lanOpen=tradeOpen=false;lan=Link();resetPanel=0;restarted=false;
}
static bool onlySettings(){
  for(auto &e:nvs())if(e.first!="lang"&&e.first!="snd"&&e.first!="vol"&&e.first!="txJournal"&&e.first!="txReceipts")return false;
  return nvs().at("lang")[0]==6 && !nvs().at("snd")[0] && nvs().at("vol")[0]==3;
}
static std::vector<NvsStore> cuts;
static void capture(){cuts.push_back(nvs());}
static int checkpoints=0,failAt=0;static NvsStore disk;
static bool checkpoint(){if(++checkpoints==failAt)return false;disk=nvs();return true;}
int main(){
  setup();fresh();openClock();auto original=nvs();
  clockTap(233,405);ck(resetPanel==1&&nvs()==original,"settings reset button only opens warning");
  clockTap(233,350);ck(resetPanel==0&&nvs()==original,"cancel preserves every byte");
  clockTap(233,405);clockTap(233,290);clockTap(233,290);
  ck(resetPanel==2&&!saveResetLocked&&nvs()==original,"double tap cannot accept final confirmation");
  battleOpen=true;resetConfirmAt=millis()-1001;clockTap(233,290);
  ck(!saveResetLocked&&nvs()==original,"battle blocks reset");battleOpen=false;
  trade.j.phase=TX_READY;clockTap(233,290);ck(!saveResetLocked,"pending trade blocks reset");trade.j.phase=TX_NONE;
  lanOpen=true;clockTap(233,290);ck(!saveResetLocked,"save transfer menu blocks reset");lanOpen=false;
  activeSwapBlocked=true;ck(!saveResetGame(true),"unfinished swap blocks reset backend");activeSwapBlocked=false;
  clockTap(233,290);ck(restarted&&saveResetLocked&&onlySettings(),"confirmed reset clears gameplay and keeps settings");
  auto cleared=nvs();pet.saveNow();party.save();
  AndroidGameLifecycle life;life.start();life.checkpoint(pet,millis(),200000,200000);life.suspend(pet,millis(),200000,200000);life.resume(pet,millis(),200000,200000);
  ck(life.canRun(),"foreground return keeps reset recovery UI responsive without resuming pet writes");
  onSwipe(1);onSwipeV(-1);loop();
  ck(nvs()==cleared,"stale objects, lifecycle and gestures cannot resurrect reset save");
  uint8_t backup[SAVE_MAX_BYTES];ck(saveExport(backup,sizeof(backup))==0&&!saveImport(backup,0),"backup/import blocked during reset shutdown");
  // Re-enter real setup WITHOUT discarding native globals, like NativeActivity
  // recreation in a process Android kept alive after finish().
  setup();
  ck(!saveResetLocked&&!clockOpen&&resetPanel==0,"warm Activity recreation leaves closing screen and unlocks the fresh game");
  ck(pet.isEgg()&&pet.awaitingStarter()&&pet.energy==80&&pet.streak==0&&pet.badges==0&&pet.registeredCount()==0&&party.count()==0&&party.boxCount()==0&&pet.eggsRemaining()==2,"fresh boot has initial egg, empty collection and fresh daily allowance");
  fresh();cuts.clear();nvsAfterWrite()=capture;bool ok=saveResetGame(true);nvsAfterWrite()=nullptr;
  ck(ok,"reference reset completes");int failed=0;
  for(auto snapshot:cuts){nvs()=snapshot;saveResetLocked=false;
    if(saveResetPending()&&!saveResetGame(false))failed++;
    if(!onlySettings())failed++;
  }
  printf("Reset crash boundaries: %u, failures: %d\n",(unsigned)cuts.size(),failed);
  ck(!failed,"every interrupted key write resumes to empty gameplay without orphan SD pointer");
  for(const char*k:{"resetGame","dexn","rosterF","rosterSD","aseen"}){
    fresh();auto before=nvs();nvsFailKey()=k;bool result=saveResetGame(true);nvsFailKey().clear();
    ck(!result&&saveResetLocked,"write/remove fault keeps game locked");
    ck(saveResetGame(!saveResetPending())&&onlySettings(),"retry completes after storage fault");
  }
  for(failAt=1;failAt<=3;failAt++){
    fresh();disk=nvs();checkpoints=0;
    ck(!saveResetGame(true,checkpoint),"failed durable checkpoint never reports success");
    nvs()=disk;saveResetLocked=false;int oldFail=failAt;failAt=0;
    if(saveResetPending())ck(saveResetGame(false,checkpoint)&&onlySettings(),"disk restart resumes committed reset intent");
    else ck(nvs().count("rosterF")&&nvs().count("dexn"),"uncommitted intent leaves original disk save untouched");
    failAt=oldFail;
  }
  fresh();Preferences boot;boot.begin("tamapoke");boot.putBool("resetGame",true);
  nvsFailKey()="dexn";setup();
  ck(saveResetLocked&&resetPanel==3&&saveResetPending(),"real startup stops before game loading when reset recovery fails");
  nvsFailKey().clear();onTap(233,330);
  ck(restarted&&onlySettings(),"real recovery retry completes reset and requests restart");
  fresh();TradeReceipts r;r.entries[0].mine=123;r.entries[0].peer=456;
  Preferences p;p.begin("tamapoke");p.putBytes("txReceipts",&r,sizeof(r));
  auto receipt=nvs().at("txReceipts");ck(saveResetGame(true)&&nvs().at("txReceipts")==receipt,"peer recovery receipts survive gameplay reset");
  // Exercise the actual Android/desktop file implementation, not just a map.
  NvsFile file;NvsStore read;std::string path="reset-owned-test.nvs";
  remove(path.c_str());ck(file.load(path.c_str(),read),"new test file loads");
  ck(file.save(path.c_str(),nvs(),true),"reset settings commit through actual file backend");
  ck(file.load(path.c_str(),read)&&read==nvs(),"actual file reopen retains only reset result");remove(path.c_str());
  saveResetLocked=false;return bad?1:0;
}
