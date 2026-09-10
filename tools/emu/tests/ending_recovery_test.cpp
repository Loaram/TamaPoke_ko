#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=32417;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;bool wasPressed=false;
static uint32_t now=1;uint32_t millis(){return now;}
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}void sfxPlay(uint8_t){}
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);if(!b)bad++;}
static void finish(Pet&p){now+=CEREMONY_MS+1;p.update(now);}
static void ready(Pet&p){p.begin();p.setClock(300*86400);p.dbgHatchAs(6,true);p.ageMinutes=1440;p.learnQCount=0;p.rename("SAFE");}
// Simulate a 3.7.3 save already inside an ending. New UI cannot start one.
static void legacyEnding(Pet&p,bool early=false){
  Preferences old;old.begin("tamapoke");old.putUChar("cer",CER_FAREWELL);
  old.putUChar("lend",early?CER_RELEASE:CER_FAREWELL);
  old.putBool("rtpn",early);old.putUInt("byeQuota",300*4+1);p.begin();
}
int main(){
  nvs().clear();Pet p;ready(p);legacyEnding(p);
  Pet during;during.begin();
  ck(during.ceremony==CER_FAREWELL && during.farewellsRemaining()==2,"restart resumes ceremony without charging another farewell");
  finish(during);ck(during.isEgg() && during.endedKind==CER_FAREWELL,"ceremony commits a durable bank handoff before creating an egg");
  Pet waiting;waiting.begin();
  ck(waiting.isEgg() && waiting.endedMon.dex==6 && waiting.endedMon.shiny && !strcmp(waiting.endedMon.nick,"SAFE"),"restart before banking keeps the waiting individual and egg");
  Party bank;bank.begin();ck(bank.add(waiting.endedMon),"waiting individual can be banked normally");
  Pet afterBank;afterBank.begin();Party reloaded;reloaded.begin();
  ck(reloaded.hasEndedMon(afterBank.endedMon),"restart between banking and acknowledgement detects the exact already-banked individual");
  ck(afterBank.acknowledgeEnding(),"handoff acknowledgement persists");
  Pet done;done.begin();ck(done.endedKind==CER_NONE && reloaded.count()==1,"another restart neither duplicates nor drops the retired Pokemon");
  nvs().clear();Pet crash;ready(crash);legacyEnding(crash);auto old=nvs();finish(crash);
  auto waitRecord=nvs()["endWait"];nvs()=old;nvs()["endWait"]=waitRecord;
  Pet interrupted;interrupted.begin();ck(interrupted.isEgg() && interrupted.endedKind==CER_FAREWELL,"crash after handoff but before egg save cannot leave both live and banked copies");
  static uint8_t backup[SAVE_MAX_BYTES];interrupted.saveNow();size_t n=saveExport(backup,sizeof(backup));
  nvs().clear();ck(n && saveImport(backup,n),"pending farewell survives whole-save transport");
  Pet imported;imported.begin();ck(imported.endedMon.dex==6 && imported.isEgg(),"import resumes the same pending handoff");
  nvs().clear();Pet failed;ready(failed);legacyEnding(failed);nvsFailKey()="endWait";finish(failed);
  ck(!failed.isEgg() && failed.endedKind==CER_NONE && failed.ceremony==CER_FAREWELL,"handoff storage failure keeps the live Pokemon instead of erasing it");
  nvsFailKey().clear();finish(failed);Party q;q.begin();q.add(failed.endedMon);nvsFailKey()="endWait";
  ck(!failed.acknowledgeEnding() && failed.endedKind!=CER_NONE && q.hasEndedMon(failed.endedMon),"failed acknowledgement retains a retryable handoff without another insertion");
  nvsFailKey().clear();failed.acknowledgeEnding();
  nvs().clear();Pet early;ready(early);early.ageMinutes=10;legacyEnding(early,true);Pet earlyReload;earlyReload.begin();finish(earlyReload);
  ck(earlyReload.isEgg() && earlyReload.endedKind==CER_NONE,"early retirement remains unbanked after a ceremony restart");
  for(int dex: {0,-2,32767}){
    nvs().clear();Preferences raw;raw.begin("tamapoke");raw.putBool("init",true);raw.putShort("dexn",dex);
    Pet invalid;invalid.begin();ck(invalid.isEgg() && invalid.eggPeek()>=1 && invalid.eggPeek()<=DEX_COUNT,"invalid saved live species cannot index outside the Pokedex");
  }
  return bad?1:0;
}
