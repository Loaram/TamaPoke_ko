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
    p.acknowledgeEnding();
  }
}

static void young(Pet &p, int16_t dex, uint8_t lvl){
  p.dbgHatchAs(dex,false);
  p.ageMinutes = (uint32_t)(lvl-1)*MINUTES_PER_LEVEL;
  p.fullness=p.joy=p.energy=p.hygiene=100;
}

int main(){
  nvs().clear();party.begin();Pet p;p.begin();p.setClock(400*86400);
  for(int dex:{4,6,150}) for(int age:{0,1439,1440,10000}){
    young(p,dex,1);p.ageMinutes=age;
    auto before=p.storageSnapshot();
    p.startRetire();p.startFarewell();p.release();
    auto after=p.storageSnapshot();
    ck(!p.canFarewellNow()&&!p.canRetireNow()&&!p.wantFarewellButton(),
       "good farewell is absent for every age and evolution stage");
    ck(!memcmp(&before,&after,sizeof(before))&&p.ceremony==CER_NONE&&p.eggsRemaining()==2,
       "removed voluntary endings cannot lose a Pokemon or generate an extra egg");
  }
  return bad?1:0;
}
