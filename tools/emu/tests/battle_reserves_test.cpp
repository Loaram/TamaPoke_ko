#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
#include "link.h"
#include "wild.h"
#include <cstdio>
uint32_t g_seed=3401;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){} int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void render();void battleTap(int16_t,int16_t);void startTrainerBattle(uint8_t,bool);
extern Pet pet;extern Link lan;
extern Combatant btlYou,btlFoe,btlSquad[],btlFoeSquad[];
extern uint8_t btlSquadN,btlSquadAt,btlFoeSquadN,btlFoeAt,btlMsgCount,btlMenu,btlMyAct,wildResult;
extern int8_t btlSwapWho,btlTrainer;
extern bool battleOpen,btlLink,btlLinkHost,btlOver,btlWon,btlWild,btlPetIn;
extern uint32_t btlWinUntil;
static int bad=0,results=0;static LinkResult packet;
static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);bad+=!b;}
static void capture(void*,const uint8_t*b,uint8_t n){
  if(b[0]==LM_RESULT && n>=3+sizeof(LinkResult)){memcpy(&packet,b+3,sizeof(packet));++results;}
}
static Combatant mon(){Pet p;p.dbgHatchAs(6,false);Combatant c;combatantFromPet(c,p);c.hp=c.maxHp=160;memset(c.moves,0,sizeof(c.moves));return c;}
static void fixture(bool linked=false){
  battleOpen=true;btlLink=linked;btlLinkHost=linked;btlOver=btlWon=btlWild=btlPetIn=false;
  btlWinUntil=0;wildResult=WILD_RESULT_NONE;btlSwapWho=-1;btlTrainer=-1;btlMsgCount=0;btlMenu=1;btlMyAct=0;
  btlSquadN=btlFoeSquadN=6;btlSquadAt=btlFoeAt=5;
  for(int i=0;i<6;i++){btlSquad[i]=btlFoeSquad[i]=mon();btlSquad[i].hp=btlFoeSquad[i].hp=0;}
  btlYou=btlFoe=mon();btlYou.moves[0]=1;results=0;
  if(linked){lan.begin(true,"HOST");lan.state=LINK_READY;lan.send=capture;lan.pendingAct=LINK_ACT_MOVE(0);}
}
static void resolve(){btlMsgCount=0;btlMenu=1;if(btlLink)lan.pendingAct=LINK_ACT_MOVE(0);battleTap(150,295);}
static void dismiss(){if(!btlMsgCount)btlMsgCount=1;battleTap(233,250);}
int main(){
  nvs().clear();setup();bool allSlots=true;
  for(int active=0;active<6;active++)for(int survivor=0;survivor<6;survivor++)if(active!=survivor){
    fixture();btlSquadAt=active;btlYou.hp=0;btlSquad[active]=mon(); // stale active cache is deliberately alive
    btlSquad[survivor].hp=37;resolve();
    allSlots &= !btlOver && btlSwapWho==0;dismiss();
    allSlots &= btlSquadAt==survivor && btlYou.hp==37 && btlSquad[active].hp==0 && btlSwapWho<0;
  }
  ck(allSlots,"all 30 active/reserve arrangements survive, wrap around, skip fainted slots and preserve HP");
  fixture();btlSquadAt=0;btlYou.hp=0;btlSquad[0]=mon();resolve();
  ck(btlOver&&!btlWon&&btlSwapWho<0,"all reserves fainted ends battle even when later slots exist and active cache looks alive");
  fixture(true);btlFoe.hp=0;btlFoeSquad[5]=mon();btlFoeSquad[1].hp=29;resolve();
  ck(!btlOver&&btlSwapWho==1,"LAN rival in last slot survives with an earlier reserve");dismiss();
  ck(btlFoeAt==1&&btlFoe.hp==29&&btlFoeSquad[5].hp==0&&packet.guestIdx==1&&packet.guestHp==29,
     "host replacement immediately reports the correct rival slot and HP");
  fixture(true);btlYou.hp=0;btlSquad[2].hp=41;resolve();dismiss();
  ck(!btlOver&&btlSquadAt==2&&packet.hostIdx==2&&packet.hostHp==41,"LAN host wraps to its living reserve and reports it");
  fixture(true);btlFoe.hp=0;btlFoeSquad[5]=mon();resolve();
  ck(btlOver&&btlWon&&lan.state==LINK_DONE,"LAN win requires no live rival reserve, not the active index");
  fixture(true);btlYou.hp=btlFoe.hp=1;btlYou.ailment=btlFoe.ailment=AIL_POISON;
  // Harmless status action allows both end-of-turn poison ticks to run.
  MoveId harmless=0;for(MoveId i=1;i<MOVE_COUNT;i++)if(MOVE_TBL[i].cat==MC_STATUS&&!MOVE_TBL[i].ailment){harmless=i;break;}
  btlYou.moves[0]=harmless;btlSquad[0].hp=51;btlFoeSquad[0].hp=53;resolve();
  ck(!btlOver&&btlSwapWho==1&&packet.hostHp==0&&packet.guestHp==0,"simultaneous poison KO uses final HP and does not end two living teams");
  dismiss();ck(!btlOver&&btlSwapWho==0,"second fainted side remains queued after the first replacement");dismiss();
  ck(btlSwapWho<0&&btlYou.hp==51&&btlFoe.hp==53&&packet.hostIdx==0&&packet.guestIdx==0,
     "both replacements finish without an extra attack and synchronize to the guest");
  fixture(true);btlYou.hp=0;btlFoe.hp=0;btlFoeSquad[0].hp=50;resolve();
  ck(btlOver&&!btlWon,"simultaneous active KO loses when only the rival has reserves");
  fixture();btlYou.hp=0;btlFoe.hp=0;btlSquad[0].hp=50;resolve();
  ck(btlOver&&btlWon,"defeating the sole wild/one-off opponent wins without an unnecessary replacement");
  fixture();pet.dbgHatchAs(6,false);pet.energy=100;startTrainerBattle(0,false);
  btlSquadN=6;btlSquadAt=5;for(int i=0;i<6;i++){btlSquad[i]=mon();btlSquad[i].hp=0;}
  btlYou=mon();btlYou.hp=0;btlYou.moves[0]=1;btlSquad[0].hp=42;resolve();dismiss();
  ck(!btlOver&&btlSquadAt==0&&btlYou.hp==42,"real gym entry also returns from the last slot to an earlier survivor");
  return bad?1:0;
}
