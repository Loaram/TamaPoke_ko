#include "Arduino.h"
#include "pet.h"
#include "save.h"
#include <cstdio>
#include <vector>
uint32_t g_seed=391;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
uint32_t millis(){return 1;}void FakeESP::restart(){}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}void sfxPlay(uint8_t){}
int bad=0;
void ck(bool ok,const char*s){printf("%s %s\n",ok?"PASS":"FAIL",s);bad+=!ok;}
constexpr uint32_t JAN1=1767225600UL, SEP16=1789516800UL;
std::vector<uint8_t> snapshot(){std::vector<uint8_t>b(SAVE_MAX_BYTES);b.resize(saveExport(b.data(),b.size()));return b;}
int main(){
  nvs().clear();Pet p;p.begin();p.dbgHatchAs(25,false);p.sleeping=false;
  for(int i=9;i>=0;--i){p.setClock(SEP16-i*86400+43200);p.feed();}
  for(int i=0;i<20;i++)p.caress();p.saveNow();
  Preferences quota;quota.begin("tamapoke");quota.putUInt("eggQuota",((SEP16/86400)<<2)|2);
  quota.putUInt("aseen",SEP16-86400*5);quota.putBool("andclk1",true);p.begin();
  party.begin();PartyMon stored=p.storageSnapshot();party.box[299]=stored;ck(party.boxSave(),"full-size roster fixture");
  auto original=snapshot();uint32_t age=p.ageMinutes;uint16_t weight=p.eggShinyWeight();
  for(uint32_t receiver:{JAN1+43200,SEP16+3*86400+43200,SEP16+43200}) {
    nvs().clear();quota.putUInt("aseen",SEP16-86400*5);quota.putBool("andclk1",true);
    ck(saveImportAtTime(original.data(),original.size(),receiver),"clock-aware import into earlier/later/equal RTC");
    ck(!quota.isKey("aseen"),"import clears destination Android UTC baseline to prevent replayed offline growth");
    Pet dst;dst.begin();party.begin();dst.syncClock(receiver);dst.sleeping=false;dst.feed();
    ck(dst.streak==10&&dst.eggBonusDays()==10&&dst.ageMinutes==age,"streak and bonus preserved without instant growth");
    ck(dst.eggsRemaining()==0&&dst.storageSnapshot().care[10]==20,"import does not refill eggs or individual bond allowance");
    ck(dst.eggShinyWeight()==weight,"egg odds unchanged by clock rebasing");
    ck(!memcmp(&party.box[299],&stored,sizeof(stored)),"stored individual remains byte-identical");
    ck(dst.storedIndividualMatches(),"durable active snapshot comparison uses logical calendar");
    PartyMon check;ck(dst.readStoredSnapshot(check),"recovery snapshot remains readable with offset");
    while(dst.hasLearnOffer())dst.declineLearn();
    ck(party.swapActive(dst,true,299),"bring companion after cross-calendar transfer");
    ck(dst.storageSnapshot().care[10]==20&&dst.eggsRemaining()==0&&dst.streak==10,"swap cannot refill allowance or reset streak");
    // Repeated bidirectional whole-save transfers preserve the same logical day.
    for(int i=0;i<8;i++) {
      auto b=snapshot();uint32_t now=i%2?receiver:SEP16+43200;
      ck(saveImportAtTime(b.data(),b.size(),now),"round-trip accepted");
      Pet back;back.begin();back.syncClock(now);back.sleeping=false;back.feed();
      ck(back.streak==10&&back.eggsRemaining()==0&&back.storageSnapshot().care[10]==20,"round trip cannot mint a day, egg or bond allowance");
    }
    dst.begin();dst.syncClock(receiver+86400);dst.sleeping=false;dst.feed();
    ck(dst.streak==11&&dst.eggsRemaining()==2,"next recipient midnight grants normal next day");
    auto saved=snapshot();Pet reboot;reboot.begin();reboot.syncClock(receiver+86400);reboot.feed();
    ck(reboot.streak==11&&reboot.eggsRemaining()==2,"restart does not change the earned day");
    reboot.syncClock(receiver+3*86400);reboot.feed();
    ck(reboot.streak==1&&reboot.bestStreak==11,"genuine missed recipient day still resets current streak");
  }
  nvs().clear();saveImport(original.data(),original.size());
  Preferences pref;pref.begin("tamapoke");pref.remove("dayOff");pref.putUInt("seen",JAN1+43200);
  Pet legacy;legacy.begin();legacy.syncClock(JAN1+43200);legacy.sleeping=false;legacy.feed();
  ck(legacy.streak==10&&legacy.eggBonusDays()==10&&legacy.eggsRemaining()==0,"old ESP stranded behind future care date recovers once without quota refill");
  Pet again;again.begin();again.syncClock(JAN1+86400+43200);again.sleeping=false;again.feed();
  ck(again.streak==11,"legacy recovery persists across restart");
  ck(recoverRtcEpoch(0,SEP16)==SEP16&&recoverRtcEpoch(JAN1,SEP16)==SEP16,"invalid/backward RTC recovers from saved timestamp");
  ck(recoverRtcEpoch(SEP16+86400,SEP16)==SEP16+86400,"healthy forward RTC keeps real offline time");
  ck(recoverRtcEpoch(0,0)==JAN1,"only virgin RTC uses initial seed");
  auto before=nvs();ck(!saveImportAtTime(original.data(),original.size(),0)&&nvs()==before,"invalid destination clock rejects without writes");
  auto corrupt=original;corrupt[10]^=1;
  ck(!saveImportAtTime(corrupt.data(),corrupt.size(),SEP16)&&nvs()==before,"corrupt import rejects without writes");
  nvsFailKey()="dayOff";
  ck(!saveImportAtTime(original.data(),original.size(),SEP16+4*86400)&&nvs()==before,"offset write failure rolls back complete snapshot");
  nvsFailKey().clear();return bad?1:0;
}
