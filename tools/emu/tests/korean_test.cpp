#include "Arduino.h"
#include "Preferences.h"
#include "i18n.h"
#include "korean_text.h"
#include "korean_names.h"
#include <cassert>
#include <cstdio>
#include <cstring>
uint32_t g_seed=1; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
uint32_t millis(){return 0;}void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
int main(){
  nvs().clear();loadLang();assert(gLang==LANG_KO);
  setLang(LANG_EN);gLang=LANG_KO;loadLang();assert(gLang==LANG_EN);
  assert(!strcmp(T(S_SETTINGS),"SETTINGS"));
  setLang(LANG_KO);gLang=LANG_EN;loadLang();assert(gLang==LANG_KO);
  assert(!strcmp(T(S_SETTINGS),"설정"));
  for(int l=0;l<LANG_COUNT;l++){setLang((Lang)l);for(int i=0;i<STR_COUNT;i++)assert(T((StrId)i)&&*T((StrId)i));}
  setLang(LANG_KO);
  for(const auto &n:KOREAN_NAMES)assert(!strcmp(localName(n.en),n.ko));
  assert(!strcmp(localName("myNickname"),"myNickname"));
  setLang(LANG_EN);assert(!strcmp(localName("PIKACHU"),"PIKACHU"));setLang(LANG_KO);
  assert(uiTextWidth("피카츄",2)==48);assert(uiTextWidth("ABC",2)==36);
  assert(uiTextWidth("피카츄 Lv.1",2)==108);
  const char *bad="\xEA\xB0";assert(nextCodepoint(bad)==0xFFFD);
  const char *four="\xF0\x9F\x98\x80";assert(nextCodepoint(four)==0x1F600);assert(!*four);
  KoreanCanvas canvas(466,466,nullptr);canvas.setTextColor(0xFFFF);canvas.setTextSize(2);canvas.setCursor(0,0);canvas.print("한국어");
  int ink=0;for(int i=0;i<466*16;i++)if(canvas.buffer()[i])ink++;
  assert(ink>100);assert(canvas.cx==48);
  puts("PASS: 7-language table, Korean preference reload, all display names, UTF-8 errors, widths and actual glyph drawing");
}
