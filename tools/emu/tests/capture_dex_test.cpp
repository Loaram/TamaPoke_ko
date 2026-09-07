#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "wild.h"
#include "pokedex_progress.h"
#include <cstdio>
#include <cmath>
#include <random>
uint32_t g_seed=360;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void finishWildBattle();extern Pet pet;
extern bool battleOpen,btlWild,btlWon,btlOver,exploreOpen,wildShiny;
extern uint8_t wildResult,wildLevel,wildIvAtk,wildIvDef,wildIvSpe,wildIvHp;
extern int16_t wildDex;
static int bad=0;static void ck(bool x,const char*s){printf("%s %s\n",x?"PASS":"FAIL",s);bad+=!x;}
static uint32_t reference(int rate,int count){
  int a=(int)floor(floor(rate*(1.0+0.2*(count/100))+1e-9)*14.0/15.0+1e-9);
  if(a>=255)return 65536;if(!a)return 0;
  int root=(int)floor(sqrt(floor(sqrt(floor(16711680.0/a)))));
  return 1048560/root;
}
static void fill(int count){
  memset(pet.dexReg,0,sizeof(pet.dexReg));memset(pet.dexShinyReg,0,sizeof(pet.dexShinyReg));
  for(int d=1;d<=DEX_COUNT&&count;d++)if(d!=150&&speciesHasArt(d)){
    pet.dexReg[(d-1)>>3]|=1<<((d-1)&7);count--;
  }
}
static void fixture(int count){
  nvs().clear();party.begin();pet.begin();pet.dbgHatchAs(6,false);pet.learnQCount=0;
  for(auto&m:party.slots)m=PartyMon();for(auto&m:party.box)m=PartyMon();party.save();
  fill(count);pet.saveNow();battleOpen=btlWild=btlWon=btlOver=true;exploreOpen=false;
  wildResult=WILD_RESULT_NONE;wildDex=150;wildLevel=60;wildShiny=false;
  wildIvAtk=wildIvDef=wildIvSpe=wildIvHp=25;
}
int main(){
  nvs().clear();setup();const int total=pokedexCollectibleCount();
  ck(total==982,"current collectible catalog has 982 species");
  bool exact=true,monotone=true,bounds=true;int cases=0;
  for(int r=0;r<=255;r++){
    uint32_t previous=0;
    for(int n=0;n<=total;n++){
      uint32_t t=wildCaptureShakeThreshold(r,n);exact&=t==reference(r,n);monotone&=t>=previous;previous=t;cases++;
      if(r==3)continue;
      uint16_t rolls[4]={0,0,0,0};
      bounds&=wildCaptureCheck(r,0,rolls,n)==(t!=0);
      if(t&&t<65536){
        for(auto&v:rolls)v=t-1;bounds&=wildCaptureCheck(r,0,rolls,n);
        for(int j=0;j<4;j++){rolls[j]=t;bounds&=!wildCaptureCheck(r,0,rolls,n);rolls[j]=t-1;}
      }
    }
  }
  ck(exact&&monotone&&bounds,"all 256 rates x 983 counts match independent integer reference and shake boundaries");
  printf("MATRIX rate_count_cases=%d\n",cases);
  bool rare=true;
  for(int n=0;n<=total;n++){
    int caught=0;uint16_t rolls[4]={65535,65535,65535,65535};
    for(int r=0;r<1000;r++)caught+=wildCaptureCheck(3,r,rolls,n);
    rare&=caught==25+5*(n/100);
  }
  ck(rare,"rate-3 bonus is exactly 2.5 to 7 percent at every count and ignores shakes");
  ck(wildCaptureBallTenths(99)==10&&wildCaptureBallTenths(100)==12&&wildCaptureBallTenths(899)==26&&wildCaptureBallTenths(900)==28,
     "100-species boundaries add precisely 0.2");
  ck(wildCaptureBallTenths(65535)==28,"out-of-range counts clamp to the collectible catalog");
  std::mt19937 rng(360);std::uniform_int_distribution<unsigned> shake(0,65535),rareRoll(0,999);
  bool sampled=true;const int trials=100000;
  for(int n=0;n<=900;n+=100)for(int r:{255,190,120,45,3}){
    double expected=r==3?wildCaptureRarePermille(n)/1000.0:pow(wildCaptureShakeThreshold(r,n)/65536.0,4);
    int caught=0;for(int i=0;i<trials;i++){uint16_t rolls[4];for(auto&v:rolls)v=shake(rng);caught+=wildCaptureCheck(r,rareRoll(rng),rolls,n);}
    double sigma=sqrt(trials*expected*(1-expected));sampled&=fabs(caught-trials*expected)<=7*sigma+5;
    printf("SIM dex=%d rate=%d expected=%.8f%% observed=%.4f%%\n",n,r,100*expected,100.0*caught/trials);
  }
  ck(sampled,"5000000 independent draws agree with the five guide examples at all ten tiers");
  fixture(99);pet.registerCaughtSpecies(1,false);pet.registerCaughtSpecies(1,true);
  ck(pet.collectibleRegisteredCount()==99,"duplicate and shiny registration do not increase the tier");
  for(int d=1;d<=DEX_COUNT;d++)if(!speciesHasArt(d))pet.dexReg[(d-1)>>3]|=1<<((d-1)&7);
  ck(pet.collectibleRegisteredCount()==99,"unavailable species bits do not grant capture bonus");
  pet.saveNow();Pet reloaded;reloaded.begin();ck(reloaded.collectibleRegisteredCount()==99,"count derives from existing save without new schema");
  uint8_t data[SAVE_MAX_BYTES];size_t size=saveExport(data,sizeof(data));
  ck(size&&saveImport(data,size),"v7 transfer remains accepted with collection-derived capture bonus");
  fixture(99);uint32_t between=0;
  for(uint32_t s=1;s<100000&&!between;s++){g_seed=s;bool old=wildCaptureNow(3,99);g_seed=s;bool next=wildCaptureNow(3,100);if(!old&&next)between=s;}
  ck(between!=0,"live random fixture distinguishes neighboring bonus tiers");
  g_seed=between;finishWildBattle();ck(wildResult==WILD_RESULT_ESCAPED&&pet.collectibleRegisteredCount()==99,"99-species capture cannot borrow the unregistered target's bonus");
  fixture(100);g_seed=between;finishWildBattle();ck(wildResult==WILD_RESULT_PARTY&&pet.collectibleRegisteredCount()==101,"real battle finish uses current registered count before storage");
  auto stored=nvs();auto seed=g_seed;finishWildBattle();ck(nvs()==stored&&g_seed==seed,"result redraw cannot reroll or duplicate a bonus capture");
  fixture(99);for(uint32_t s=1;s<100000;s++){g_seed=s;if(wildCaptureNow(3,99)){g_seed=s;break;}}
  finishWildBattle();ck(pet.collectibleRegisteredCount()==100&&wildCaptureBallTenths(pet.collectibleRegisteredCount())==12,"successful hundredth species unlocks the next capture tier");
  fixture(900);btlWon=false;seed=g_seed;finishWildBattle();ck(wildResult==WILD_RESULT_LOST&&g_seed==seed,"maximum bonus never captures after a lost battle");
  return bad?1:0;
}
