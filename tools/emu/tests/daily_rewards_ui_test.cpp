#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "korean_text.h"
#include "pokedex_progress.h"
#include "i18n.h"
#include <cstdio>
uint32_t g_seed=19743;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void render();void renderPlayer();void drawMenu();void drawChoiceDialog();void onTap(int16_t,int16_t);
extern Pet pet;extern KoreanCanvas *gfx;extern bool menuOpen;
extern uint8_t choiceKind,playerPage;extern uint32_t choiceUntil;
static int bad=0;
static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);if(!b)bad++;}
static void shot(const char *name){
  FILE*f=fopen(name,"wb");if(!f)abort();fprintf(f,"P6\n466 466\n255\n");
  for(int i=0;i<466*466;i++){uint16_t c=gfx->buffer()[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);
}
int main(){
  nvs().clear();setup();pet.setClock(86400*200+43200);pet.dbgHatchAs(6,false);
  pet.learnQCount=0;pet.streak=10;pet.bestStreak=10;pet.lastCareDay=200;
  pet.fullness=pet.joy=pet.energy=pet.hygiene=100;pet.ageMinutes=1440;
  playerPage=0;renderPlayer();shot("daily-rewards-player.ppm");
  memset(pet.dexReg,0,sizeof(pet.dexReg));
  int need=(pokedexCollectibleCount()+1)/2;
  for(int d=1;d<=DEX_COUNT && need;d++)if(speciesIsCollectible(d)){
    pet.dexReg[(d-1)>>3]|=1<<((d-1)&7);--need;
  }
  renderPlayer();shot("egg-bonus-max.ppm");
  ck(pet.collectibleRegisteredCount()==484 && pet.eggShinyWeight()==3240 && pet.eggLegendWeight()==189,
     "trainer screen displays collectible goal with both capped probabilities");
  pet.streak=0;renderPlayer();shot("egg-bonus-dex-only.ppm");
  ck(pet.eggShinyWeight()==2160 && pet.eggLegendWeight()==126,"dex-only screen retains the standalone goal odds");
  pet.streak=10;setLang(LANG_EN);renderPlayer();shot("egg-bonus-max-en.ppm");setLang(LANG_KO);
  menuOpen=true;drawMenu();shot("daily-rewards-menu.ppm");onTap(233,303);
  ck(choiceKind==3,"retire menu opens confirmation while allowance remains");
  drawChoiceDialog();shot("daily-rewards-confirm.ppm");choiceKind=0;
  for(int i=0;i<3;i++){pet.release();pet.newEgg();pet.dbgHatchAs(6,false);}
  pet.learnQCount=0;pet.ageMinutes=1440;menuOpen=true;drawMenu();shot("daily-rewards-exhausted.ppm");
  onTap(233,303);ck(choiceKind==0 && pet.ceremony==CER_NONE,"exhausted menu cannot open a fourth farewell");
  ck(pet.farewellsRemaining()==0,"rendering and opening menus do not refill allowance");
  return bad?1:0;
}
