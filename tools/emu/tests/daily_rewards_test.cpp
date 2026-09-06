// Daily player rewards and the shared, persistent voluntary-ending allowance.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "pokedex_progress.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=19283; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
static uint32_t now=1;
uint32_t millis(){return now;}
void FakeESP::restart(){exit(0);} int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");} void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*s){printf("%s %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}
static void finish(Pet&p){now+=CEREMONY_MS+1;p.update(now);p.acknowledgeEnding();}
static void hatch(Pet&p){p.dbgHatchAs(6,false);p.sleeping=false;p.frozen=false;while(p.hasLearnOffer())p.declineLearn();}
static void day(Pet&p,uint32_t d){p.setClock(d*86400+43200);}

int main(){
  nvs().clear(); Pet p;p.begin();hatch(p);
  for(int d=1;d<=25;d++)p.dbgHatchAs(d,false);
  const uint32_t total=pokedexCollectibleCount();
  auto shinyAt=[&](int step){return 450+190*step+(15390-630*step)*50/(9*total);};
  auto legendAt=[&](int step){return 27+11*step+(891-36*step)*50/(9*total);};
  ck(p.eggBonusDays()==0 && p.eggShinyWeight()==shinyAt(0) && p.eggLegendWeight()==legendAt(0),"unknown clock keeps only the earned 25-species dex bonus");
  for(int d=1;d<=11;d++){
    day(p,100+d);p.feed();
    int step=d>10?9:d-1;
    ck(p.streak==d && p.eggShinyWeight()==shinyAt(step) && p.eggLegendWeight()==legendAt(step),"daily care increases both weights, capped at day ten");
    p.feed();p.clean();p.caress();
    ck(p.streak==d,"multiple care actions do not count as multiple days");
  }
  ck(p.eggShinyWeight()==shinyAt(9) && p.eggLegendWeight()==legendAt(9),"day ten retains its full care bonus plus the 25-species contribution");
  {
    Pet sim;day(sim,200);sim.lastCareDay=200;memset(sim.dexReg,0xFF,sizeof(sim.dexReg));
    for(int careDay: {1,10}){
      sim.streak=careDay;int shiny=0;
      for(int i=0;i<5000;i++){sim.newEgg();sim.eggTap();sim.eggTap();sim.eggTap();shiny+=sim.shiny;}
      printf("  day %d: %d shiny / 5000 actual hatches\n",careDay,shiny);
      ck(careDay==1 ? (shiny>400 && shiny<600) : (shiny>650 && shiny<850),"actual egg creation and hatching use daily shiny odds");
    }
  }
  for(int end: {CER_NONE,CER_RELEASE,CER_FAREWELL}){
    p.lastEnd=end;p.bond=0;
    ck(p.eggShinyWeight()==shinyAt(9) && p.eggLegendWeight()==legendAt(9),"farewell classification and individual bond no longer set these bonuses");
  }
  int legends=0;
  for(int i=0;i<10000;i++)if(DEX_TBL[p.pickEggSpecies()].rarity==R_LEGENDARIO)legends++;
  ck(legends>1200 && legends<1600,"actual egg species lottery uses the day-ten legendary weight");
  p.lastEnd=CER_RUNAWAY;
  ck(p.eggLegendWeight()==0 && p.eggShinyWeight()==shinyAt(9),"runaway retains common-only restriction, not a shiny bonus penalty");
  p.lastEnd=CER_NONE;
  memset(p.dexReg,0,sizeof(p.dexReg));
  for(int d=1;d<=24;d++)p.dbgHatchAs(d,false);
  ck(p.eggLegendWeight()==0,"legendary gate still requires 25 registered species");
  p.dbgHatchAs(25,false);
  day(p,112);ck(p.eggBonusDays()==10,"yesterday's care remains valid before today's first action");
  day(p,113);ck(p.eggBonusDays()==0 && p.eggShinyWeight()==shinyAt(0) && p.eggLegendWeight()==legendAt(0),"missing a full care day removes only care bonus, retaining the dex contribution");
  p.feed();ck(p.streak==1 && p.eggBonusDays()==1,"care after a gap restarts at day one");
  day(p,112);p.feed();ck(p.streak==1 && p.lastCareDay==113 && p.eggBonusDays()==0,"clock rollback cannot farm care days");
  day(p,114);p.feed();ck(p.streak==2,"care resumes normally after the clock catches up");
  p.frozen=true;day(p,115);p.feed();ck(p.streak==3,"caring for a companion counts for the player too");p.frozen=false;

  nvs().clear();Pet q;q.begin();day(q,200);hatch(q);
  q.feed();q.startRetire();ck(q.retireIsEarly() && q.farewellsRemaining()==2,"early retire consumes exactly one allowance");
  q.startFarewell();q.release();ck(q.farewellsRemaining()==2,"duplicate requests during ceremony do not consume more");
  finish(q);hatch(q);q.ageMinutes=FAREWELL_AGE_MIN;q.startRetire();
  ck(!q.retireIsEarly() && q.farewellsRemaining()==1,"earned farewell shares the allowance and keeps correct classification");
  finish(q);hatch(q);q.release();ck(q.farewellsRemaining()==0,"long-press live release consumes the third allowance");
  finish(q);hatch(q);q.ageMinutes=FAREWELL_AGE_MIN;
  q.startRetire();q.startFarewell();q.release();
  ck(q.ceremony==CER_NONE && !q.canFarewellNow() && !q.canRetireNow(),"all three voluntary paths reject a fourth farewell");
  Pet reload;reload.begin();ck(reload.farewellsRemaining()==0,"restart does not refill the allowance");
  Party bank;bank.begin();PartyMon m;m.dex=25;m.level=80;bank.add(m);
  ck(bank.swapActive(q,false,0) && q.farewellsRemaining()==0 && q.streak==1,"free companion exchange preserves player streak and spent allowance");
  ck(bank.swapActive(q,false,0),"returning to the original creature is still allowed");
  bank.releaseAt(0);ck(q.farewellsRemaining()==0,"deleting banked creatures does not consume or reset live allowance");
  q.saveNow();uint8_t backup[SAVE_MAX_BYTES];size_t n=saveExport(backup,sizeof(backup));
  ck(n>0 && backup[4]==SAVE_VERSION && saveValidate(backup,n),"whole-save includes the daily counter");
  nvs().clear();ck(saveImport(backup,n),"cross-device payload imports successfully");
  Pet transferred;transferred.begin();ck(transferred.farewellsRemaining()==0 && transferred.streak==1,"save transfer retains both allowance and streak");
  day(transferred,199);ck(transferred.farewellsRemaining()==0,"backward clock does not refill allowance");
  transferred.setClock(201*86400-1);ck(transferred.farewellsRemaining()==0,"last second before midnight remains exhausted");
  transferred.setClock(201*86400);ck(transferred.farewellsRemaining()==3,"local midnight replenishes three farewells");
  hatch(transferred);nvsFailKey()="byeQuota";transferred.startRetire();
  ck(transferred.ceremony==CER_NONE && transferred.farewellsRemaining()==3 && !transferred.retireIsEarly(),"quota write failure does not start retirement or lose the creature");
  nvsFailKey().clear();transferred.startRetire();ck(transferred.farewellsRemaining()==2,"retry after storage recovers charges once");
  finish(transferred);int egg=transferred.eggPeek();transferred.saveNow();
  auto before=nvs()["eshy"];Pet eggReload;eggReload.begin();
  ck(eggReload.eggPeek()==egg && nvs()["eshy"]==before,"existing egg species and shiny roll are preserved on reload");

  nvs().erase("byeQuota");Pet legacy;legacy.begin();ck(legacy.farewellsRemaining()==3 && legacy.streak==1,"old saves without the new field preserve care history and start with three");
  nvs().clear();Pet timeless;timeless.begin();
  for(int i=0;i<3;i++){hatch(timeless);timeless.release();finish(timeless);}
  Pet timelessReload;timelessReload.begin();ck(timelessReload.farewellsRemaining()==0,"missing clock cannot refill three uses on restart");
  printf("%s\n",bad?"FAILURES":"all good");return bad?1:0;
}
