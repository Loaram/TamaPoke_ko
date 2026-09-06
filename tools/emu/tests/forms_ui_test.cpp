#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "sdmon.h"
#include <cstdio>
uint32_t g_seed=733;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void render();void onTap(int16_t,int16_t);void onSwipe(int);void onSwipeV(int);
void formOpenTarget(uint16_t);extern Pet pet;extern bool formsOpen;extern uint8_t formPage;
void drawFormMini(int16_t,FormId,bool,int,int);
void emuSetSpriteDir(const char*);extern Arduino_Canvas *gfx;
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);if(!b)bad++;}
int main(){
  nvs().clear();setup();pet.dbgHatchAs(6,false);pet.ageMinutes=69*MINUTES_PER_LEVEL;
  while(pet.hasLearnOffer())pet.declineLearn();
  formOpenTarget(0);render();onTap(355,298);render();
  ck(formsOpen&&formPage==1,"form page arrow navigates without leaving the modal");
  onTap(230,348);ck(pet.form==10134,"visible select button applies the displayed form");
  onSwipe(-1);render();ck(formPage==0,"page wrap skips the removed Charizard Gmax entry");
  onTap(230,348);ck(pet.form==0,"base form is still selectable after the list shrinks");
  pet.dbgHatchAs(890,false);pet.ageMinutes=69*MINUTES_PER_LEVEL;
  while(pet.hasLearnOffer())pet.declineLearn();
  formOpenTarget(0);onSwipe(-1);render();onTap(230,348);
  ck(pet.form==0,"animated Eternamax still requires level 80");
  pet.ageMinutes=79*MINUTES_PER_LEVEL;onTap(230,348);
  ck(pet.form==10359,"animated Eternamax remains selectable at level 80");
  onSwipeV(-1);ck(!formsOpen,"vertical back closes the form preview");
  bool loaded=true;uint32_t largest=0;
  for(uint16_t i=0;i<FORM_COUNT;i++) {
    PmdMon m;const auto &f=FORM_TBL[i];
    loaded &= m.loadForm(f.dex,f.id,false)&&m.has(PMD_IDLE);m.unload();
    loaded &= m.loadForm(f.dex,f.id,true)&&m.has(PMD_IDLE);m.unload();
  }
  ck(loaded,"all 176 normal and shiny-or-same-form-fallback sprites load through the real parser");
  for(auto dex:{6,358,869}) {
    FormId removed=dex==6?10365:dex==358?10531:10392;
    PmdMon fallback,base;bool ok=fallback.loadForm(dex,removed,false)&&base.load(dex,false);
    if(ok) {
      const auto&a=fallback.acts[PMD_IDLE];const auto&b=base.acts[PMD_IDLE];
      ok=a.w==b.w&&a.h==b.h&&a.frames==b.frames&&
         fallback.palCount==base.palCount&&!memcmp(fallback.pal,base.pal,base.palCount*2)&&
         !memcmp(a.data,b.data,(size_t)b.w*b.h*b.frames);
    }
    ck(ok,"old installed stills cannot reappear through the actual sprite loader");
    fallback.unload();base.unload();
  }
  emuSetSpriteDir("build/intentionally-missing-art");
  gfx->fillScreen(0);drawFormMini(6,10134,false,230,220);auto first=gfx->fb;
  gfx->fillScreen(0);drawFormMini(6,10134,false,230,220);
  ck(first==gfx->fb && std::any_of(first.begin(),first.end(),[](uint16_t p){return p!=0;}),
     "missing form thumbnail placeholder persists on cached redraws");
  return bad?1:0;
}
