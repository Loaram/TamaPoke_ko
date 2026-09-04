// Every multi-result evolution in the 1..809 Pokedex follows the same
// collection rule: prefer an incomplete path, then randomise when complete.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "dex.h"
#include <cstdio>
#include <cstring>

uint32_t g_seed=109; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
uint32_t millis(){return 0;}
void sfxPlay(uint8_t){}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void FakeESP::restart(){}

static int bad=0;
static void ck(bool ok,const char *what){
  printf("%s  %s\n",ok?"PASS":"FAIL",what);
  if(!ok) bad++;
}

static const int16_t WANT[][EVO_BRANCH_MAX+2] = {
  {44,2,45,182}, {61,2,62,186}, {79,2,80,199},
  {133,8,134,135,136,196,197,470,471,700},
  {236,3,106,107,237}, {265,2,266,268}, {281,2,282,475},
  {290,2,291,292}, {361,2,362,478}, {366,2,367,368},
  {412,2,413,414}, {790,2,791,792},
};

static void fillDex(Pet &p){
  memset(p.dexReg,0xFF,sizeof(p.dexReg));
  if(DEX_COUNT&7) p.dexReg[sizeof(p.dexReg)-1]=(uint8_t)((1u<<(DEX_COUNT&7))-1u);
}
static void clearDex(Pet &p,int16_t d){
  p.dexReg[(d-1)>>3]&=(uint8_t)~(1u<<((d-1)&7));
}

int main(){
  ck(EVO_BRANCH_COUNT==sizeof(WANT)/sizeof(WANT[0]),
     "all 12 branched families are generated");
  bool exact=true, evolutionOnly=true;
  for(uint8_t b=0;b<EVO_BRANCH_COUNT;b++){
    if(EVO_BRANCHES[b].base!=WANT[b][0] || EVO_BRANCHES[b].count!=WANT[b][1]) exact=false;
    for(uint8_t i=0;i<EVO_BRANCHES[b].count;i++){
      if(EVO_BRANCHES[b].targets[i]!=WANT[b][i+2]) exact=false;
      if(DEX_TBL[EVO_BRANCHES[b].targets[i]].rarity!=R_EVO) evolutionOnly=false;
    }
  }
  ck(exact,"the generated branch map matches every 1..809 split evolution");
  ck(evolutionOnly,"alternate evolutions no longer hatch as unrelated base forms");

  gRegionArt=0xFFFF;
  bool prefersMissing=true;
  for(uint8_t b=0;b<EVO_BRANCH_COUNT;b++){
    Pet p; p.begin(); p.factoryReset(); p.begin();
    p.dbgHatchAs(EVO_BRANCHES[b].base,false);
    fillDex(p);
    int16_t want=EVO_BRANCHES[b].targets[EVO_BRANCHES[b].count-1];
    clearDex(p,want);
    p.ageMinutes=99UL*MINUTES_PER_LEVEL;
    p.fullness=p.joy=p.energy=p.hygiene=100;
    if(!p.canEvolveNow()){ prefersMissing=false; break; }
    p.evolve();
    if(p.speciesId!=want){
      printf("      base %d chose %d instead of missing %d\n",
             EVO_BRANCHES[b].base,p.speciesId,want);
      prefersMissing=false;
    }
  }
  ck(prefersMissing,"every family prefers its one missing branch");

  // Cosmoem was the reported failure: its table row points at Solgaleo, so
  // explicitly prove that either version counterpart wins when it is missing.
  for(int16_t want=791;want<=792;want++){
    Pet p; p.begin(); p.factoryReset(); p.begin(); p.dbgHatchAs(790,false);
    fillDex(p); clearDex(p,want);
    p.ageMinutes=99UL*MINUTES_PER_LEVEL;
    p.fullness=p.joy=p.energy=p.hygiene=100;
    p.evolve();
    char msg[72]; snprintf(msg,sizeof(msg),"Cosmoem reaches missing dex %d",want);
    ck(p.speciesId==want,msg);
  }

  // Wurmple's choice continues into two different final forms. If Silcoon and
  // Cascoon are both registered but Beautifly is missing, choose Silcoon's path.
  {
    Pet p; p.begin(); p.factoryReset(); p.begin(); p.dbgHatchAs(265,false);
    fillDex(p); clearDex(p,267);
    p.ageMinutes=99UL*MINUTES_PER_LEVEL;
    p.fullness=p.joy=p.energy=p.hygiene=100;
    ck(p.lineHasUnregistered(265),"a missing descendant keeps the Wurmple family incomplete");
    p.evolve();
    ck(p.speciesId==266,"Wurmple chooses the branch leading to missing Beautifly");
  }

  printf("%s\n",bad?"FAILURES":"all good");
  return bad?1:0;
}
