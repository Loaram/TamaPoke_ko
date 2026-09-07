#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "wild.h"
#include "pokedex_progress.h"
#include <cstdio>
#include <vector>
uint32_t g_seed=361;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
uint32_t millis(){return 1;}void FakeESP::restart(){}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}void sfxPlay(uint8_t){}
static int bad=0;static void ck(bool x,const char*s){printf("%s %s\n",x?"PASS":"FAIL",s);bad+=!x;}
int main(){
  bool unsafe[DEX_COUNT+1]={};for(int d:NO_ART)unsafe[d]=true;
  bool changed=true;while(changed){changed=false;
    for(int d=1;d<=DEX_COUNT;d++){int to=DEX_TBL[d].evolvesTo;if(to>0&&unsafe[to]&&!unsafe[d])unsafe[d]=changed=true;}
    for(const auto&b:EVO_BRANCHES)for(int k=0;k<b.count;k++)if(unsafe[b.targets[k]]&&!unsafe[b.base])unsafe[b.base]=changed=true;
  }
  bool closure=true;int count=0,newLocks=0;
  for(int d=1;d<=DEX_COUNT;d++){closure &= speciesIsCollectible(d)==!unsafe[d];count+=!unsafe[d];newLocks+=unsafe[d]&&speciesHasArt(d);}
  ck(closure&&count==967&&newLocks==15,"all ordinary and branched evolution ancestors match unlock rule: 967 collectible, 15 additional locks");
  ck(!speciesIsCollectible(0)&&!speciesIsCollectible(1026)&&!speciesIsCollectible(-1),"invalid IDs are never collectible");
  gRegionArt=0xFFFF;bool reachable[DEX_COUNT+1]={};bool pool=true;
  for(int r=0;r<=REGION_ALL;r++)for(int tier=0;tier<=R_LEGENDARIO;tier++)for(int roll=0;roll<DEX_COUNT;roll++){
    int d=wildPickSpecies(r,tier,roll);if(d){pool &= !unsafe[d];reachable[d]=true;}
  }
  for(int d=1;d<=DEX_COUNT;d++)pool &= reachable[d]==!unsafe[d];
  ck(pool,"every region and rarity encounter pool excludes unsafe lines and covers all unlocked species");
  ck(speciesHasArt(953)&&speciesHasArt(955)&&!speciesIsCollectible(953)&&!speciesIsCollectible(955),"Rellor and Flittle remain drawable but are locked from new collection");
  nvs().clear();party.begin();Pet p;p.begin();
  for(int d:{953,955}){
    p.dbgHatchAs(d,false);p.ageMinutes=99UL*MINUTES_PER_LEVEL;p.fullness=p.hygiene=p.joy=p.energy=100;p.sleeping=false;p.careMistakes=0;
    p.ivAtk=7;p.ivDef=19;p.ivSpe=31;p.ivHp=0;p.saveNow();auto before=nvs();
    ck(!p.canEvolveNow(),"legacy owned unsafe base cannot offer a blank-art evolution");p.evolve();
    ck(p.speciesId==d&&nvs()==before,"blocked evolution changes neither owned Pokemon nor save");
    Pet loaded;loaded.begin();ck(loaded.speciesId==d&&loaded.ivAtk==7&&loaded.ivHp==0,"legacy creature and IVs survive reload");
  }
  memset(p.dexReg,0,sizeof(p.dexReg));for(int d=1;d<=DEX_COUNT;d++)p.dexReg[(d-1)>>3]|=1<<((d-1)&7);
  ck(p.registeredCount()==1025&&p.collectibleRegisteredCount()==967,"historical registrations preserved while collectible numerator excludes all locks");
  unsigned sum=0;for(int r=0;r<REGION_ALL;r++)sum+=pokedexCollectibleCountIn(REGIONS[r].lo,REGIONS[r].hi);
  ck(sum==967&&pokedexCollectibleCount()==967,"national and regional targets agree");
  p.saveNow();std::vector<uint8_t> blob(SAVE_MAX_BYTES);auto size=saveExport(blob.data(),blob.size());
  ck(size&&saveImport(blob.data(),size),"v7 save transfer with historical locked entries remains compatible");
  return bad?1:0;
}
