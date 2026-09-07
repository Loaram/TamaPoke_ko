#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "noart.h"
#include "save.h"
#include <cstdio>
uint32_t g_seed=340; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
uint32_t millis(){return 0;} void FakeESP::restart(){}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*s){printf("%s %s\n",ok?"PASS":"FAIL",s);bad+=!ok;}
int main(){
  gRegionArt=0xFFFF;
  // A complete Charmander family but an incomplete Kanto Dex reproduces the
  // reported case. Test actual newEgg -> hatch, not just a forced shiny flag.
  Pet p;p.dbgHatchAs(4,false);p.dbgHatchAs(5,false);p.dbgHatchAs(6,false);
  p.region=0;p.lastEnd=CER_RUNAWAY; // common-only, removes unrelated rarity noise
  int shinyCount=0,shinyCharmander=0,normalCount=0;bool normalFresh=true,valid=true;
  for(int i=0;i<16000;i++){
    memset(p.dexReg,0,sizeof(p.dexReg));
    for(int dex:{4,5,6})p.dexReg[(dex-1)>>3]|=1<<((dex-1)&7);
    // Even a completed shiny Dex must not exclude a favourite shiny species.
    memset(p.dexShinyReg,0xFF,sizeof(p.dexShinyReg));
    p.newEgg();int dex=p.eggPeek();bool fresh=p.lineHasUnregistered(dex);
    valid &= dex>=1 && dex<=151 && DEX_TBL[dex].rarity==R_COMUN && speciesCanHatch(dex);
    p.eggTap();p.eggTap();p.eggTap();
    if(p.shiny){++shinyCount;if(dex==4)++shinyCharmander;}
    else {++normalCount;normalFresh &= fresh && dex!=4;}
  }
  ck(shinyCount>100 && normalCount>1000 && shinyCharmander>0,
     "actual shiny eggs can hatch Charmander before normal Kanto completion, even with a full shiny Dex");
  ck(normalFresh,"normal eggs still prefer incomplete evolution families");
  ck(valid,"both colours retain common-only runaway, region and hatch-art restrictions");
  printf("shiny eggs %d, shiny Charmander %d, normal eggs %d\n",shinyCount,shinyCharmander,normalCount);
  // Persist one of each colour and prove load / region revisit / transfer do
  // not reroll the egg selected under the new policy.
  for(bool colour:{false,true}){
    nvs().clear();Pet saved;saved.begin();saved.dbgHatchAs(6,false);
    for(int i=0;i<2000;i++){saved.newEgg();if(nvs()["eshy"][0]==colour)break;}
    const auto shiny=nvs()["eshy"];int dex=saved.eggPeek();
    ck(!shiny.empty() && (shiny[0]!=0)==colour,"fixture finds requested persisted egg colour");
    Pet reloaded;reloaded.begin();
    ck(reloaded.eggPeek()==dex && nvs()["eshy"]==shiny,"reload preserves existing species and shiny result");
    reloaded.setRegion(1);int johto=reloaded.eggPeek();reloaded.setRegion(0);reloaded.setRegion(1);
    ck(reloaded.eggPeek()==johto && nvs()["eshy"]==shiny,"region revisit preserves cached species and colour");
    uint8_t payload[SAVE_MAX_BYTES];size_t size=saveExport(payload,sizeof(payload));
    nvs().clear();ck(size && saveImport(payload,size),"unchanged whole-save transfers the waiting egg");
    Pet imported;imported.begin();
    ck(imported.eggPeek()==johto && nvs()["eshy"]==shiny,"import preserves species and colour without reroll");
  }
  nvs().clear();Pet first;first.begin();
  ck(first.awaitingStarter(),"new players still select their first starter");
  return bad?1:0;
}
