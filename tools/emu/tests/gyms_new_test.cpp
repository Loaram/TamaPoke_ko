#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
#include "gym_art.h"
#include "noart.h"
#include "save.h"
#include <cstdio>
uint32_t g_seed=875;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void render();void onTap(int16_t,int16_t);void onSwipe(int);
void startTrainerBattle(uint8_t,bool);extern Pet pet;
extern bool gymOpen,gymShield,btlShield,battleOpen,btlOver,btlWon,gymHard;
extern uint8_t gymRegion,gymPage,btlRegion,btlFoeAt,btlMsgCount,btlMenu;
extern uint16_t squadMask;extern uint32_t btlWinUntil;
extern Combatant btlYou,btlFoe;
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);if(!b)bad++;}
int main(){
  nvs().clear();setup();pet.dbgHatchAs(9,false);pet.ageMinutes=99*MINUTES_PER_LEVEL;
  while(pet.hasLearnOffer())pet.declineLearn();
  ck(GYM_REGIONS==9,"nine gym regions without reindexing the first seven");
  Preferences old;old.begin("tamapoke",false);
  uint16_t easy[6]={1,3,7,15,31,63},hard[6]={2,4,8,16,32,64};
  old.putBytes("badgX",easy,sizeof(easy));old.putBytes("badhX",hard,sizeof(hard));old.end();
  {Pet migrated;migrated.begin();bool ok=true;for(int i=0;i<6;i++)ok &= migrated.badgesX[i]==easy[i]&&migrated.badgesHardX[i]==hard[i];
   ck(ok,"short seven-region save preserves every old badge in both modes");
   ck(!migrated.badgeMask(7,false)&&!migrated.badgeMask(8,true),"new regions start with no forged wins");}
  gymOpen=true;gymRegion=7;gymShield=false;onTap(233,40);
  ck(gymShield&&gymOpen,"Galar title toggles Sword/Shield without leaving gyms");
  onTap(233,80);ck(gymShield&&gymHard,"difficulty control does not toggle the version");
  gymHard=false;gymOpen=false;pet.badgesX[6]=0;
  pet.winBadge(7,trainerBadgeIndex(7,3,false),false);
  ck(!pet.hasBadge(7,trainerBadgeIndex(7,3,true),false),"Bea win does not award Allister badge");
  pet.winBadge(7,15,true);{Pet q;q.begin();ck(q.hasBadge(7,15,true),"Shield final bit 15 survives saving and loading");}
  ck(pet.gymBadgeCountIn(7,false,false)==1&&pet.gymBadgeCountIn(7,false,true)==0,"course badge count excludes the other version and Cup wins");
  bool all=true;unsigned battles=0,mons=0;
  for(int course=0;course<3;course++)for(uint8_t ti=0;ti<TRAINER_COUNT;ti++){
    uint8_t region=course==2?8:7;bool shield=course==1;
    gymRegion=region;gymShield=shield;battleOpen=false;btlWinUntil=0;squadMask=1;
    startTrainerBattle(ti,false);
    const auto&t=trainerAt(region,ti,shield);
    all &= battleOpen&&btlRegion==region&&btlShield==shield;
    // Change the menu state mid-fight to prove the battle captured its route.
    gymRegion=0;gymShield=!shield;
    for(int k=0;k<t.count;k++){
      all &= btlFoe.dex==t.team[k].dex&&btlFoe.form==t.team[k].form&&btlFoe.level==t.team[k].level;
      all &= btlFoe.npcType1==t.team[k].type1&&btlFoe.npcType2==t.team[k].type2;
      all &= speciesHasArt(btlFoe.dex)||gymNpcHasArt(btlFoe.dex);
      if(t.team[k].form) all &= formFind(t.team[k].dex,t.team[k].form)!=nullptr;
      mons++;
      // A deterministic one-hit test drive through the real tap/turn/swap code.
      int guard=0;
      do {
        if(btlMsgCount) onTap(233,320);
        else {
          btlYou.hp=btlYou.maxHp=60000;btlYou.ailment=AIL_NONE;btlYou.confuseTurns=0;
          btlYou.moves[0]=MV_SURF;btlYou.base[SI_SPA]=60000;btlYou.base[SI_SPE]=60000;
          btlFoe.hp=1;btlMenu=1;onTap(110,306);
        }
      }while(!btlOver&&btlFoeAt==k&&++guard<50);
      all &= guard<50;
    }
    all &= btlWon&&pet.hasBadge(region,trainerBadgeIndex(region,ti,shield),false);
    battles++;
  }
  printf("Checked %u battles, %u team members\n",battles,mons);
  ck(all,"all new teams chain, retain their region/form and award correct course wins");
  battleOpen=false;btlWinUntil=0;gymOpen=true;gymRegion=8;gymPage=0;
  render();onSwipe(-1);render();ck(gymPage==1,"Paldea second page remains reachable");
  onSwipe(-1);render();ck(gymPage==2,"Paldea Elite Four and champion page remains reachable");
  ck(saveExportSize()<=SAVE_MAX_BYTES,"expanded badges fit the existing v3 transfer limit");
  pet.saveNow(); uint8_t blob[SAVE_MAX_BYTES];size_t len=saveExport(blob,sizeof(blob));
  uint16_t galar=pet.badgeMask(7,false),paldea=pet.badgeMask(8,false);
  nvs().clear();ck(len&&saveImport(blob,len),"expanded save passes the actual transfer importer");
  {Pet q;q.begin();ck(q.badgeMask(7,false)==galar&&q.badgeMask(8,false)==paldea&&q.hasBadge(7,15,true),
    "all new course wins survive full export/import, including high bit 15");}
  Pet oricorio;oricorio.dbgHatchAs(741,false);oricorio.ageMinutes=49*MINUTES_PER_LEVEL;
  Combatant base,pompom,water;combatantFromPet(base,oricorio);pompom=base;
  pompom.npcType1=T_ELECTRIC;pompom.npcType2=T_FLYING;combatantFromPet(water,pet);
  uint16_t before=battleDamage(water,base,MV_SURF,false,255),after=battleDamage(water,pompom,MV_SURF,false,255);
  ck(before>after&&before>=after*2,"NPC Pom-Pom defense uses electric/flying, not the base fire/flying type");
  ck(battleDamage(pompom,water,MV_THUNDERBOLT,false,255)>battleDamage(base,water,MV_THUNDERBOLT,false,255),
    "NPC Pom-Pom electric attacks receive the matching-type bonus");
  return bad?1:0;
}
