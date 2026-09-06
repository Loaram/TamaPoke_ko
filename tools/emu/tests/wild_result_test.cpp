#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "wild.h"
#include "korean_text.h"
#include "i18n.h"
#include <cstdio>
uint32_t g_seed=331;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void renderBattle();void battleTap(int16_t,int16_t);void onSwipe(int);void onSwipeV(int);
void finishWildBattle();bool startWildBattle(bool);void startTrainerBattle(uint8_t,bool);
extern Pet pet;extern KoreanCanvas *gfx;extern Combatant btlYou,btlFoe;
extern bool battleOpen,btlWild,btlWon,btlOver,exploreOpen,wildShiny,wildResultRendered;
extern uint8_t wildResult,wildLevel,wildIvAtk,wildIvDef,wildIvSpe,wildIvHp,btlMsgCount,btlMenu,btlSquadN,btlSquadAt;
extern uint32_t wildResultReadyAt;extern int16_t wildDex;
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);bad+=!b;}
static void shot(const char*name){
  renderBattle();FILE*f=fopen(name,"wb");if(!f)abort();fprintf(f,"P6\n466 466\n255\n");
  for(int i=0;i<466*466;i++){uint16_t c=gfx->buffer()[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);
}
static void chooseRoll(bool caught){
  for(uint32_t seed=1;seed<100000;seed++){
    g_seed=seed;if(wildCaptureNow(wildCatchRateForDex(wildDex))==caught){g_seed=seed;return;}
  }abort();
}
static void fixture(bool won,int occupied=0){
  nvsFailKey().clear();nvs().clear();party.begin();pet.begin();pet.dbgHatchAs(6,false);
  pet.learnQCount=0;pet.energy=100;pet.sleeping=false;
  for(auto&m:party.slots)m=PartyMon();for(auto&m:party.box)m=PartyMon();
  for(int i=0;i<occupied;i++){party.slots[i].dex=25;party.slots[i].level=60;}
  party.save();battleOpen=btlWild=true;exploreOpen=false;btlWon=won;btlOver=true;
  btlMsgCount=6;btlMenu=1;wildResult=WILD_RESULT_NONE;wildResultRendered=false;
  wildDex=150;wildLevel=60;wildShiny=true;wildIvAtk=21;wildIvDef=22;wildIvSpe=23;wildIvHp=24;
  memset(btlFoe.moves,0,sizeof(btlFoe.moves));btlFoe.moves[0]=1;
}
static void confirm(){renderBattle();wildResultReadyAt=millis();battleTap(233,374);}
int main(){
  nvs().clear();setup();
  fixture(true);chooseRoll(true);finishWildBattle();
  ck(wildResult==WILD_RESULT_PARTY && !btlMsgCount && party.count()==1,"six-message win opens a separate captured-to-party modal");
  ck(party.slots[0].dex==150&&party.slots[0].shiny&&party.slots[0].ivHp==24&&pet.isShinyRegistered(150),"capture stores the exact individual and registers the shiny Dex");
  auto stored=nvs();uint32_t seed=g_seed;
  wildResultReadyAt=millis();battleTap(233,374);
  ck(battleOpen&&wildResult==WILD_RESULT_PARTY,"unrendered result cannot be dismissed");
  shot("wild-result-party.ppm");wildResultReadyAt=millis()+10000;battleTap(233,374);
  ck(battleOpen,"finishing-turn double tap cannot skip the result");
  wildResultReadyAt=millis();battleTap(233,150);onSwipe(1);onSwipeV(-1);
  ck(battleOpen&&wildResult==WILD_RESULT_PARTY,"outside taps and swipes leave the result open");
  for(int i=0;i<20;i++){finishWildBattle();renderBattle();}
  ck(nvs()==stored&&g_seed==seed&&party.count()==1,"redraws and repeated finish calls never reroll or duplicate a capture");
  ck(!startWildBattle(false),"a second exploration cannot replace an unread result");
  confirm();ck(!battleOpen&&!btlWild&&exploreOpen&&wildResult==WILD_RESULT_NONE,"only Confirm returns to Explore and clears result state");
  Party reloaded;reloaded.begin();ck(reloaded.slots[0].dex==150,"confirmed catch survives roster reload");
  fixture(true,PARTY_SLOTS);chooseRoll(true);finishWildBattle();
  ck(wildResult==WILD_RESULT_BOX&&party.boxCount()==1,"full party reports the actual box destination");shot("wild-result-box.ppm");confirm();
  fixture(true);chooseRoll(false);stored=nvs();finishWildBattle();
  ck(wildResult==WILD_RESULT_ESCAPED&&nvs()==stored&&!party.count(),"failed capture has its own modal and adds no Pokemon");
  shot("wild-result-failed.ppm");setLang(LANG_EN);shot("wild-result-failed-en.ppm");setLang(LANG_KO);confirm();
  fixture(false);seed=g_seed;finishWildBattle();
  ck(wildResult==WILD_RESULT_LOST&&g_seed==seed&&!party.count(),"defeat explicitly reports no capture attempt without using RNG");shot("wild-result-lost.ppm");confirm();
  fixture(true);chooseRoll(true);stored=nvs();nvsFailKey()="rosterF";finishWildBattle();
  ck(wildResult==WILD_RESULT_STORAGE_ERROR&&nvs()==stored&&!party.count()&&!party.boxCount(),"unverified write reports storage error, not success or escape, without a second destination attempt");
  shot("wild-result-storage.ppm");confirm();nvsFailKey().clear();party.begin();
  fixture(true);chooseRoll(true);finishWildBattle();confirm();
  battleOpen=false;btlOver=false;pet.energy=100;pet.learnQCount=0;
  ck(startWildBattle(false),"new exploration starts normally after confirmation");
  // Drive the real move-button -> battle-resolution -> result path, not just
  // the outcome helper. A fainted opponent makes this a deterministic last turn.
  btlMsgCount=0;btlMenu=1;btlFoe.hp=0;btlYou.hp=btlYou.maxHp;btlYou.moves[0]=1;
  battleTap(150,295);
  ck(btlOver&&btlWon&&wildResult!=WILD_RESULT_NONE,"real final-turn resolution opens the result modal");confirm();
  startTrainerBattle(0,false);ck(battleOpen&&!btlWild&&wildResult==WILD_RESULT_NONE,"trainer battles do not inherit a wild-result modal");
  return bad?1:0;
}
