// The private -shiny emulator exposes every normal and shiny Pokedex entry.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include <cstdio>

uint32_t g_seed=43; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
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

int main(){
  Pet p;
  p.begin();
  ck(p.registeredCount()==DEX_COUNT,"the -shiny build completes the normal Pokedex");
  bool allNormal=true, allShiny=true;
  for(int16_t d=1;d<=DEX_COUNT;d++){
    if(!p.isRegistered(d)) allNormal=false;
    if(!p.isShinyRegistered(d)) allShiny=false;
  }
  ck(allNormal,"all 1025 normal species entries are unlocked");
  ck(allShiny,"all 1025 shiny species entries are unlocked");
  ck(p.awaitingStarter(),"a fresh test save still offers the normal starter choice");
  printf("%s\n",bad?"FAILURES":"all good");
  return bad?1:0;
}
