#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "egg_rewards.h"
#include "pokedex_progress.h"
#include <cstdio>
#include <cmath>
uint32_t g_seed=19283; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
uint32_t millis(){return 1;} void FakeESP::restart(){}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*s){printf("%s %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}
static void fill(Pet&p,int count){
  memset(p.dexReg,0,sizeof(p.dexReg));
  for(int d=1;d<=DEX_COUNT && count;d++)if(speciesIsCollectible(d)){
    p.dexReg[(d-1)>>3]|=1<<((d-1)&7);count--;
  }
}
int main(){
  using namespace EggRewards;
  const uint16_t total=pokedexCollectibleCount(), half=(total+1)/2;
  ck(total==967 && half==484,"collectible target is 967 species, half is 484");
  for(bool shiny : {false,true}){
    const uint16_t base=shiny?SHINY_BASE:LEGEND_BASE,solo=shiny?SHINY_SOLO:LEGEND_SOLO,max=shiny?SHINY_MAX:LEGEND_MAX;
    ck(weight(base,solo,max,0,0,total)==base,"neither goal retains original base probability");
    ck(weight(base,solo,max,10,0,total)==solo,"care ten days alone retains 10% shiny / 14% legend");
    ck(weight(base,solo,max,0,half,total)==solo,"dex fifty percent alone gives 10% shiny / 14% legend");
    ck(weight(base,solo,max,10,half,total)==max,"both goals give exactly 15% shiny / 21% legend");
    bool sane=true;
    for(unsigned days=0;days<=12;days++)for(unsigned n=0;n<=total;n++){
      uint16_t got=weight(base,solo,max,days,n,total);
      double s=days>10?1.0:days<=1?0.0:(days-1)/9.0;
      double d=n*2.0/total;if(d>1)d=1;
      double expected=base*(1-s)*(1-d)+solo*(s*(1-d)+(1-s)*d)+max*s*d;
      sane &= got==(uint16_t)floor(expected+1e-8) && got>=base && got<=max;
      sane &= days>=10 && n>=half ? got==max : got<max;
      if(n)sane &= got>=weight(base,solo,max,days,n-1,total);
      if(days)sane &= got>=weight(base,solo,max,days-1,n,total);
    }
    ck(sane,"every care day and collection count is monotonic, bounded and reaches cap only with both goals");
    ck(weight(base,solo,max,255,65535,total)==max && weight(base,solo,max,10,0,0)==solo,
       "over-goal and empty-future-dataset inputs are bounded without division by zero");
    ck(weight(base,solo,max,10,491,983)<max && weight(base,solo,max,10,492,983)==max,
       "odd future collectible counts require the first species above exact 50%");
  }
  nvs().clear();Pet p;p.begin();p.dbgHatchAs(6,false);p.setClock(200*86400+43200);
  p.streak=10;p.bestStreak=10;p.lastCareDay=200;fill(p,half);
  ck(p.eggShinyWeight()==SHINY_MAX && p.eggLegendWeight()==LEGEND_MAX,
     "real Pet routes both-goal odds into the existing egg lottery");
  for(int dex:NO_HATCH)p.dexReg[(dex-1)>>3]|=1<<((dex-1)&7);
  ck(p.registeredCount()==half+NO_HATCH_COUNT && p.collectibleRegisteredCount()==half,
     "legacy no-art registrations do not inflate collectible completion");
  p.registerCaughtSpecies(25,true);p.registerCaughtSpecies(25,false);
  ck(p.collectibleRegisteredCount()==half,"shiny and duplicate registrations count the species only once");
  p.setClock(202*86400+43200);
  ck(!p.eggBonusDays() && p.eggShinyWeight()==SHINY_SOLO && p.eggLegendWeight()==LEGEND_SOLO,
     "care gap removes care bonus but leaves permanent fifty-percent dex bonus");
  p.lastEnd=CER_RUNAWAY;
  ck(p.eggLegendWeight()==0 && p.eggShinyWeight()==SHINY_SOLO,"runaway still blocks legendary eggs, not dex shiny bonus");
  p.lastEnd=CER_NONE;fill(p,24);ck(!p.eggLegendWeight(),"existing 25-registration legend gate remains");
  fill(p,25);ck(p.eggLegendWeight()>LEGEND_BASE,"25 registrations unlock legendary odds with dex contribution");
  fill(p,half);p.streak=10;p.lastCareDay=202;p.saveNow();
  p.newEgg();const int egg=p.eggPeek();const auto shinyRoll=nvs()["eshy"];
  fill(p,0);p.streak=0;p.saveNow();Pet oldEgg;oldEgg.begin();
  ck(oldEgg.eggPeek()==egg && nvs()["eshy"]==shinyRoll,"changing bonus does not reroll an existing egg");
  fill(oldEgg,half);oldEgg.streak=10;oldEgg.bestStreak=10;oldEgg.lastCareDay=202;oldEgg.saveNow();
  uint8_t payload[SAVE_MAX_BYTES];size_t n=saveExport(payload,sizeof(payload));
  nvs().clear();ck(n && saveImport(payload,n),"unchanged v7 save transfer carries existing dex and care fields");
  Pet imported;imported.begin();imported.setClock(202*86400+43200);
  ck(imported.collectibleRegisteredCount()==half && imported.eggShinyWeight()==SHINY_MAX &&
     imported.eggLegendWeight()==LEGEND_MAX,"reload/import recomputes identical bonuses without a new save field");
  printf("%s\n",bad?"FAILURES":"all good");return bad?1:0;
}
