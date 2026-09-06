#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "korean_text.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=301;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void render();void onTap(int16_t,int16_t);void onSwipe(int);void onSwipeV(int);
void uiConfirmRects(int*,int*,int*,int*);
extern Pet pet;extern KoreanCanvas *gfx;
extern bool partyOpen,boxOpen,releaseConfirm,boxPagePicker,movePickOpen;
extern uint8_t partyDetail,boxSwapFrom,boxPage;extern uint16_t boxDetail,boxSel;
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);bad+=!b;}
static PartyMon mon(int dex){PartyMon m;m.dex=dex;m.level=60;return m;}
static void resetUI(){partyOpen=true;boxOpen=releaseConfirm=boxPagePicker=movePickOpen=false;partyDetail=boxSwapFrom=boxPage=boxDetail=boxSel=0;}
static void shot(const char*name){
  render();FILE*f=fopen(name,"wb");if(!f)abort();fprintf(f,"P6\n466 466\n255\n");
  for(int i=0;i<466*466;i++){uint16_t c=gfx->buffer()[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);
}
int main(){
  nvs().clear();setup();pet.dbgHatchAs(6,false);while(pet.hasLearnOffer())pet.declineLearn();
  for(int i=0;i<PARTY_SLOTS;i++)party.slots[i]=mon(25+i);
  for(int i=0;i<BOX_SLOTS;i++)party.box[i]=mon(133+i);
  party.save();auto quota=pet.farewellsRemaining();int t,b,t2,b2;uiConfirmRects(&t,&b,&t2,&b2);
  resetUI();onTap(233,338);ck(boxOpen&&!boxSwapFrom,"box button opens ordinary browsing without a swap");
  onTap(118,115);ck(boxDetail==1&&party.box[0].dex==133&&party.slots[0].dex==25,"direct tap with full party and 300 boxes opens details without moving anything");
  shot("box-direct-detail.ppm");onTap(326,360);ck(releaseConfirm&&party.boxCount()==300,"direct release asks before deleting");shot("box-direct-confirm.ppm");
  onTap(233,(t2+b2)/2);ck(!releaseConfirm&&boxDetail==1&&party.box[0].dex==133,"No keeps the individual and its sheet");
  onTap(326,360);onTap(233,(t+b)/2);ck(party.box[0].empty()&&party.count()==5&&!boxDetail&&boxOpen,"Yes removes only the boxed Pokemon and returns to box");
  ck(pet.farewellsRemaining()==quota&&pet.speciesId==6,"boxed release does not consume a farewell or touch active Pokemon");
  Party saved;saved.begin();ck(saved.box[0].empty(),"direct release persists across reload");party.box[0]=mon(133);party.save();
  resetUI();onTap(118,115);onTap(233,430);onTap(118,115);
  ck(boxOpen&&boxSwapFrom==1,"party-side exchange is explicitly armed");
  onTap(118,115);ck(boxDetail==1&&party.slots[0].dex==25&&party.box[0].dex==133,"pending party exchange cannot bypass box details");
  shot("box-direct-swap.ppm");onTap(326,360);ck(releaseConfirm,"release remains reachable during pending exchange");
  onSwipeV(1);ck(!releaseConfirm&&boxDetail==1&&boxOpen,"back cancels release without changing the page");
  onTap(213,360);ck(party.slots[0].dex==133&&party.box[0].dex==25&&!boxSwapFrom&&!boxDetail,"only the explicit middle button completes the pending exchange");
  onTap(118,115);onSwipe(-1);ck(!boxDetail&&boxPage==0&&boxOpen,"horizontal back closes details before paging");
  onTap(118,115);onSwipeV(1);ck(!boxDetail&&boxOpen,"vertical back closes details before leaving box");
  onSwipeV(1);ck(!boxOpen&&!boxSwapFrom&&!partyDetail,"leaving box clears stale exchange state");
  resetUI();onTap(233,338);boxPage=49;onTap(300,278);ck(boxDetail==300,"last page supports the same direct menu");
  onTap(326,360);onTap(233,(t+b)/2);ck(party.box[299].empty()&&pet.farewellsRemaining()==quota,"300th slot direct release is independent of daily quota");
  boxPage=0;onTap(118,115);onTap(213,360);ck(!boxOpen&&boxSel==1,"ordinary TO PARTY still selects a slot when party is full");
  onTap(118,115);ck(party.slots[0].dex==25&&party.box[0].dex==133,"box-to-party full exchange preserves both Pokemon");
  party.box[0]=mon(133);party.save();resetUI();onTap(118,115);onTap(233,430);onTap(118,115);boxPage=49;
  onTap(300,278);ck(party.box[299].dex==25&&party.slots[0].empty()&&!boxSwapFrom,"empty destination still supports party deposit");
  resetUI();onTap(233,338);onTap(118,115);onTap(326,360);nvsFailKey()="rosterF";onTap(233,(t+b)/2);
  ck(party.box[0].dex==133&&releaseConfirm&&boxDetail==1,"failed storage write cannot pretend release succeeded");
  nvsFailKey().clear();party.begin();ck(party.box[0].dex==133,"failed release preserves the saved individual");
  return bad?1:0;
}
