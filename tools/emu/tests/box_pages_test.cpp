#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "korean_text.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=21984;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void renderBox();void boxTap(int16_t,int16_t);void onSwipe(int);void onSwipeV(int);
extern Pet pet;extern KoreanCanvas *gfx;
extern bool partyOpen,boxOpen,boxPagePicker;extern uint8_t boxPage,boxPickerGroup,boxSwapFrom;
extern uint16_t boxDetail,boxSel;
static int bad=0;
static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);if(!b)bad++;}
static void shot(const char *name){
  FILE*f=fopen(name,"wb");if(!f)abort();fprintf(f,"P6\n466 466\n255\n");
  for(int i=0;i<466*466;i++){uint16_t c=gfx->buffer()[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);
}
int main(){
  nvs().clear();setup();pet.dbgHatchAs(6,false);pet.learnQCount=0;
  for(int i=0;i<BOX_SLOTS;i++){PartyMon m;m.dex=25+i;m.level=60;party.box[i]=m;}
  PartyMon m;m.dex=4;m.level=60;party.slots[0]=m;party.save();
  static PartyMon original[BOX_SLOTS];memcpy(original,party.box,sizeof(original));
  partyOpen=boxOpen=true;boxPage=0;boxDetail=0;
  renderBox();shot("box-pages-first.ppm");
  boxTap(116,360);ck(boxPage==0 && boxOpen,"previous on first page stays in box");
  boxTap(348,360);ck(boxPage==1 && boxOpen,"next button advances to page two");
  boxTap(116,360);ck(boxPage==0,"previous button returns to page one");
  for(int page=0;page<BOX_PAGES;page++){
    boxTap(233,360);ck(boxPagePicker,"page number opens a modal page picker");
    if(boxPickerGroup!=page/10)boxTap(348,360);
    int tile=page%10;boxTap(88+(tile%2)*150+70,100+(tile/2)*48+21);
    ck(!boxPagePicker && boxPage==page && boxOpen && boxDetail==0,"each of fifty numbered pages is directly reachable");
  }
  renderBox();shot("box-pages-last.ppm");
  boxTap(348,360);ck(boxPage==49 && boxOpen,"next on last page is inert, not an accidental exit");
  boxTap(233,360);renderBox();shot("box-pages-picker.ppm");
  boxTap(348,360);ck(boxPickerGroup==4 && boxPagePicker,"picker cannot advance past the final group");
  boxTap(70,120);ck(boxPagePicker && boxDetail==0,"picker margins do not tap the box underneath");
  onSwipe(1);ck(boxPickerGroup==3 && boxPage==49,"swipe pages the picker group without moving stored creatures");
  onSwipeV(1);ck(!boxPagePicker && boxOpen && boxPage==49,"vertical swipe cancels picker without leaving the box");
  boxTap(233,360);boxTap(233,405);ck(!boxPagePicker && boxOpen,"picker back button returns to the same box page");
  ck(!memcmp(original,party.box,sizeof(original)),"all paging operations leave all 300 stored individuals unchanged");
  boxSwapFrom=1;boxTap(116,360);boxTap(233,360);boxTap(308,313);
  ck(boxPage==49 && boxSwapFrom==1 && boxOpen,"direct page jump preserves a pending party-to-box exchange");
  boxTap(300,278);ck(boxDetail==300 && party.box[299].dex==324,"pending exchange first opens the 300th slot sheet");
  boxTap(213,360);ck(party.box[299].dex==4 && party.slots[0].dex==324 && boxSwapFrom==0,"explicit exchange on page fifty affects exactly the 300th slot");
  boxTap(300,278);ck(boxDetail==300,"last stored creature still opens its detail sheet");
  boxTap(233,430);ck(boxDetail==0 && boxOpen,"closing detail returns to box without entering navigation");
  boxTap(233,400);ck(!boxOpen,"box back still closes the box");
  return bad?1:0;
}
