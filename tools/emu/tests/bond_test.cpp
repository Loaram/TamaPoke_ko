#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include <cstdio>
uint32_t g_seed=873; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 1;}
void FakeESP::restart(){exit(0);} int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");} void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);if(!b)++bad;}
static void fresh(Pet&p){nvs().clear();p.begin();p.dbgHatchAs(25,false);p.sleeping=false;p.setClock(200*86400+43200);p.lastCareDay=200;p.bond=0;p.saveNow();}
static void taps(Pet&p,int n){while(n--)p.caress();}
int main(){
  {Pet p;fresh(p);p.bond=43;p.caress();ck(p.bond==44,"a caress increases bond below the daily limit");
   Pet reload;reload.begin();ck(reload.bond==44,"same-day caress is saved immediately");}
  {Pet p;fresh(p);p.bond=24;taps(p,20);ck(p.bond==44,"twenty daily action points can stop exactly at 44");
   taps(p,3);for(int c=0;c<3;++c)p.feedBerry(c);p.feedCandy();ck(p.bond==44,"daily limit is intentional; candy is not a direct bond reward");
   p.setClock(201*86400+43200);p.caress();ck(p.bond==49,"first next-day caress gives daily four plus action one");}
  {Pet p;fresh(p);taps(p,19);p.trainStrength(80);ck(p.bond==20,"training cannot overshoot the twenty-point daily action limit");}
  {Pet p;fresh(p);p.lastCareDay=199;p.caress();taps(p,30);ck(p.bond==24,"first care action counts toward the new day's twenty-point limit");}
  {Pet p;fresh(p);taps(p,20);p.setClock(201*86400+43200);
   PartyMon overnight=p.storageSnapshot();
   // Another companion receives the player's first care today before return.
   p.dbgHatchAs(6,false);p.caress();p.reviveFrom(overnight);p.sleeping=false;
   int before=p.bond;p.caress();ck(p.bond==before+1,"overnight storage before first care does not retain yesterday's exhausted quota");}
  {Pet p;fresh(p);taps(p,20);p.saveNow();Pet q;q.begin();q.caress();ck(q.bond==20,"restart on the same date cannot refill the quota");
   PartyMon stored=q.storageSnapshot();q.reviveFrom(stored);q.caress();ck(q.bond==20,"same-day bring-back cannot refill the quota");}
  {Pet p;fresh(p);p.bond=99;p.caress();ck(p.bond==100,"bond remains capped at one hundred");
   ck(p.hasMedal(MED_BOND),"the final caress awards the bond medal without waiting for a minute tick");}
  printf("%s\n",bad?"FAILURES":"all good");return bad?1:0;
}
