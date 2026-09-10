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

  // The former three-farewell quota is replaced by daily_egg_test's two-egg
  // transaction/clock/backup matrix. Rewards above remain unchanged.
  printf("%s\n",bad?"FAILURES":"all good");return bad?1:0;
}
