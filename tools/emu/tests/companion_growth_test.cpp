#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "i18n.h"
#include "korean_text.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=310;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void renderMonSheet(const PartyMon&,bool);void storedStatLine(const PartyMon&,uint8_t,char*,size_t);
void emuSetTimeScale(uint32_t);
extern Pet pet;extern KoreanCanvas *gfx;extern bool releaseConfirm;extern uint8_t boxSwapFrom;
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);bad+=!b;}
static void clearOffers(){while(pet.hasLearnOffer())pet.declineLearn();}
static void finishAnimation(uint32_t ms){uint32_t start=millis();emuSetTimeScale(10000);while(millis()-start<=ms){}emuSetTimeScale(1);}
static void minutes(int n){for(int i=0;i<n;i++){pet.sleeping=false;pet.fullness=pet.joy=pet.energy=pet.hygiene=100;pet.dbgTick();}clearOffers();}
static void shot(const char *name){FILE*f=fopen(name,"wb");if(!f)abort();fprintf(f,"P6\n466 466\n255\n");
  for(int i=0;i<466*466;i++){uint16_t c=gfx->buffer()[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);}
int main(){
  nvs().clear();setup();pet.setClock(200*86400+12*3600);pet.dbgHatchAs(6,false);clearOffers();
  PartyMon caught;caught.dex=4;caught.level=15;caught.ivAtk=31;caught.ivDef=0;caught.ivSpe=17;caught.ivHp=24;caught.moves[0]=1;
  party.slots[0]=caught;party.save();ck(party.swapActive(pet,false,0)&&pet.frozen&&pet.level()==15,"legacy captured record starts active at its stored level with protection");
  minutes(19);ck(pet.level()==15,"captured level does not increase before 20 minutes");minutes(1);
  ck(pet.level()==16&&pet.canEvolveNow(),"captured companion reaches normal evolution gate after 20 minutes");
  pet.evolve();clearOffers();finishAnimation(EVOLVE_ANIM_MS+1);ck(pet.speciesId==5&&pet.frozen,"evolution changes species and keeps ending protection");
  ck(pet.ivAtk==31&&pet.ivDef==0&&pet.ivSpe==17&&pet.ivHp==24,"evolution keeps all four IVs unchanged");
  minutes(7);auto paused=pet.storageSnapshot();ck(party.swapActive(pet,false,0),"growing captured companion can be parked again");
  minutes(60);ck(!memcmp(&paused,&party.slots[0],sizeof(paused)),"parked companion does not age while another Pokemon is active");
  ck(party.swapActive(pet,false,0)&&pet.ageMinutes==307,"bringing it back restores partial progress within a level");
  minutes(13);ck(pet.level()==17,"remaining 13 minutes complete the next level without rerolling IVs");
  pet.saveNow();Pet reload;reload.begin();ck(reload.level()==17&&reload.frozen&&reload.ivAtk==31,"growth and protection persist across an ordinary save reload");
  // An actual good farewell produces the same legacy companion record as old saves.
  pet.dbgHatchAs(6,true);pet.ageMinutes=1440;pet.sleeping=false;clearOffers();pet.startFarewell();finishAnimation(CEREMONY_MS+1);pet.update(millis());
  ck(pet.endedKind==CER_FAREWELL&&pet.endedMon.level==73,"good farewell produces a level 73 stored individual");
  auto farewell=pet.endedMon;party.box[299]=farewell;party.save();pet.acknowledgeEnding();
  ck(party.swapActive(pet,true,299)&&pet.level()==73&&pet.frozen,"farewelled individual can be brought back from the final box slot");
  auto remaining=pet.farewellsRemaining();minutes(20);
  ck(pet.level()==74&&!pet.canFarewellNow()&&!pet.canRetireNow()&&pet.farewellsRemaining()==remaining,"farewelled companion grows without repeat farewell or quota use");
  for(int i=0;i<RUNAWAY_TICKS;i++){pet.fullness=pet.joy=pet.energy=pet.hygiene=0;pet.dbgTick();}
  ck(!pet.canRunawayNow(),"growth does not remove runaway protection");
  pet.ageMinutes=99*MINUTES_PER_LEVEL;minutes(40);ck(pet.level()==100,"companion level remains capped at 100");
  // Both detail surfaces use the same compact raw-IV formatter.
  PartyMon shown=caught;shown.dex=6;shown.form=10134;shown.level=100;shown.shiny=1;shown.trAtk=12;shown.trDef=8;shown.trSpe=9;
  char line[120],expected[120];gLang=LANG_KO;storedStatLine(shown,0,line,sizeof(line));
  snprintf(expected,sizeof(expected),"공격 %u(31)  방어 %u(0)",party.atkOf(shown),party.defOf(shown));ck(!strcmp(line,expected),"attack/defense use form-adjusted stats with raw IVs in compact parentheses");
  storedStatLine(shown,1,line,sizeof(line));snprintf(expected,sizeof(expected),"속도 %u(17)  체력 %u(24)",party.speOf(shown),party.vitOf(shown));ck(!strcmp(line,expected),"speed/HP pair uses the right IVs with no extra label in the UI");
  pet.fullness=pet.joy=pet.energy=pet.hygiene=100;pet.sleeping=false;clearOffers();releaseConfirm=false;boxSwapFrom=0;
  renderMonSheet(shown,false);shot("companion-party-ivs.ppm");renderMonSheet(shown,true);shot("companion-box-ivs.ppm");
  gLang=LANG_EN;storedStatLine(shown,0,line,sizeof(line));ck(strstr(line,"ATK ")&&strstr(line,"(31)")&&strstr(line,"DEF "),"English detail UI uses the same unambiguous IV mapping");
  return bad?1:0;
}
