#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=38017;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;bool wasPressed=false;
static uint32_t now=1;uint32_t millis(){return now;}
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}void sfxPlay(uint8_t){}
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);bad+=!b;}
static void hatch(Pet&p,int dex=4){p.dbgHatchAs(dex,true);p.learnQCount=0;p.sleeping=false;}
static void fresh(Pet&p){nvsFailKey().clear();nvs().clear();tradeStorageBlocked=false;party.begin();p.begin();p.setClock(400*86400+43200);hatch(p);}
static std::vector<NvsStore> cuts;
static void capture(){cuts.push_back(nvs());}
int main(){
  Pet p;fresh(p);p.ageMinutes=7;p.trAtk=9;p.trDef=8;p.trSpe=7;p.bond=44;p.energy=13;p.streak=10;p.lastCareDay=400;p.rename("KEPT");
  auto before=p.storageSnapshot();
  ck(p.eggsRemaining()==2 && p.receiveEgg(),"young non-final Pokemon can receive the first egg");
  auto stored=party.slots[0];before.care[1]|=1;memcpy(before.care+32,stored.care+32,4);
  ck(!memcmp(&before,&stored,sizeof(before)),"deposit keeps exact age, form, training, IVs, moves, name, shiny, bond and care");
  ck(p.isEgg() && p.energy==13 && p.eggsRemaining()==1 && p.streak==10,"egg preserves shared energy/streak and charges one use");
  ck(!p.receiveEgg() && p.eggsRemaining()==1,"an existing egg cannot be replaced or charged");
  Pet reload;reload.begin();ck(reload.eggsRemaining()==1 && reload.eggPeek()==p.eggPeek(),"restart retains the same egg and daily allowance");
  hatch(reload);ck(reload.receiveEgg() && reload.eggsRemaining()==0 && party.count()==2,"second claim banks the pet and exhausts today's allowance");
  hatch(reload);ck(!reload.receiveEgg(),"third claim is rejected without losing the active Pokemon");
  ck(party.swapActive(reload,false,0) && reload.eggsRemaining()==0,"bringing a companion cannot refill allowance");
  party.releaseAt(0);ck(reload.eggsRemaining()==0,"releasing stored Pokemon cannot refill allowance");
  reload.saveNow();static uint8_t backup[SAVE_MAX_BYTES];size_t n=saveExport(backup,sizeof(backup));
  nvs().clear();ck(n && saveImport(backup,n),"whole-save round trip includes the daily egg quota");
  Pet imported;imported.begin();ck(imported.eggsRemaining()==0 && imported.streak==10,"import keeps quota and streak");
  imported.setClock(399*86400);ck(imported.eggsRemaining()==0,"clock rollback does not refill quota");
  imported.setClock(401*86400-1);ck(imported.eggsRemaining()==0,"quota remains exhausted before midnight");
  imported.setClock(401*86400);ck(imported.eggsRemaining()==2,"local midnight grants two, not accumulated days");
  imported.setClock(410*86400);ck(imported.eggsRemaining()==2,"unused days do not accumulate");
  Pet space;fresh(space);PartyMon filler;filler.dex=25;
  for(int i=0;i<PARTY_SLOTS;i++)party.slots[i]=filler;party.save();
  ck(space.receiveEgg() && party.box[0].dex==4,"a full party deposits into the first free box slot");
  hatch(space);for(auto&m:party.box)m=filler;party.save();auto full=space.storageSnapshot();
  ck(!space.receiveEgg() && space.eggsRemaining()==1 && !space.isEgg(),"full party and box deny without spending or erasing");
  for(const char *key:{"eggClaim","rosterF","eggQuota","dexn","eggT2","eshy","age","sleep","eggR"}){
    Pet fail;fresh(fail);fail.ageMinutes=77;fail.sleeping=true;fail.saveNow();
    nvsFailKey()=key;bool accepted=fail.receiveEgg();nvsFailKey().clear();
    bool pending=fail.eggClaimPending();
    if(pending){ck(activeSwapBlocked,"incomplete claim blocks normal gameplay");party.begin();ck(activeSwapBlocked,"roster reload cannot clear an egg recovery block");}
    Pet retry;retry.begin();party.begin();
    ck(!activeSwapBlocked && (pending || accepted ? retry.isEgg() && retry.eggsRemaining()==1 && party.count()==1 : !retry.isEgg() && retry.eggsRemaining()==2 && party.count()==0),"storage failure recovers exactly once or leaves the original untouched");
  }
  Pet legacy;fresh(legacy);Preferences raw;raw.begin("tamapoke");raw.remove("eggQuota");raw.putUInt("byeQuota",400*4+3);
  Pet migrated;migrated.begin();ck(migrated.eggsRemaining()==2 && migrated.speciesId==4,"old farewell quota does not consume the new allowance or alter the pet");
  nvs().clear();party.begin();Pet timeless;timeless.begin();
  for(int i=0;i<2;i++){hatch(timeless);ck(timeless.receiveEgg(),"unknown-clock claim is bounded and persistent");}
  Pet timelessReload;timelessReload.begin();ck(timelessReload.eggsRemaining()==0,"missing clock cannot refill uses on restart");
  hatch(timelessReload);timelessReload.dbgRunawayReady();
  ck(timelessReload.canRunawayNow(),"the explicitly retained neglect/runaway rule is still available");
  timelessReload.startRunaway();now+=CEREMONY_MS+1;timelessReload.update(now);
  ck(timelessReload.isEgg() && timelessReload.eggsRemaining()==0 && timelessReload.lastEnd==CER_RUNAWAY,
     "automatic runaway egg is separate and never refills menu claims");
  // Replay a crash after EVERY atomic key write, including roster commit,
  // quota, each live key and final acknowledgement. Never lose or duplicate.
  for(int claimNo:{1,2}){
    Pet cut;fresh(cut);if(claimNo==2){cut.receiveEgg();hatch(cut,25);}
    cuts.clear();cuts.push_back(nvs());nvsAfterWrite()=capture;bool completed=cut.receiveEgg();nvsAfterWrite()=nullptr;
    ck(completed,"reference claim completes before crash replay");
    auto snapshots=cuts;int failures=0;
    for(size_t i=0;i<snapshots.size();i++){
      nvs()=snapshots[i];party.begin();Pet recovered;recovered.begin();party.begin();
      bool committed=i>0;
      if(activeSwapBlocked || (committed ? !recovered.isEgg() || recovered.eggsRemaining()!=2-claimNo || party.count()!=claimNo : recovered.isEgg() || recovered.eggsRemaining()!=3-claimNo || party.count()!=claimNo-1))failures++;
      auto first=nvs();Pet again;again.begin();party.begin();
      if(activeSwapBlocked || again.eggPeek()!=recovered.eggPeek() || party.count()!=(committed?claimNo:claimNo-1))failures++;
    }
    printf("Crash replay: claim %d, %u write boundaries, %d failures\n",claimNo,(unsigned)snapshots.size(),failures);
    ck(!failures,"all interrupted writes recover without duplicated Pokemon, egg rerolls or extra charges");
  }
  return bad?1:0;
}
