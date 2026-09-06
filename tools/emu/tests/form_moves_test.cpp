#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "form_moves.h"
#include "save.h"
#include "korean_text.h"
#include <cstdio>
uint32_t g_seed=735;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void render();void onTap(int16_t,int16_t);void boxTap(int16_t,int16_t);
void formOpenTarget(uint16_t);void formClose();
extern Pet pet;extern uint8_t formPage,boxPage;extern uint16_t boxDetail,formTarget,movePickBox;
extern bool boxOpen,partyOpen,movePickOpen;
extern KoreanCanvas *gfx;
static void shot(const char *path){
  FILE *f=fopen(path,"wb");if(!f)abort();fprintf(f,"P6\n466 466\n255\n");
  for(int i=0;i<466*466;i++){uint16_t c=gfx->buffer()[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);
}
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);if(!b)bad++;}
static FormId id(const char*k){for(uint16_t i=0;i<FORM_COUNT;i++)if(!strcmp(FORM_TBL[i].key,k))return FORM_TBL[i].id;abort();}
static bool has(int dex,FormId f,MoveId mv,int level=100){MoveId all[128];int n=formLearnableList(dex,f,level,all,128);for(int i=0;i<n;i++)if(all[i]==mv)return true;return false;}
int main(){
  nvs().clear();setup();
  ck(BOX_SLOTS==300 && BOX_PAGES==50,"300 box slots and 50 numbered pages");
  ck(has(892,id("urshifu-rapid-strike"),MV_SURGING_STRIKES) && !has(892,id("urshifu-rapid-strike"),MV_WICKED_BLOW),"rapid strike has its own learnset, not single strike's");
  ck(has(479,id("rotom-wash"),MV_HYDRO_PUMP) && !has(479,id("rotom-heat"),MV_HYDRO_PUMP),"appliance signatures do not leak between forms");
  ck(has(646,id("kyurem-black"),MV_FREEZE_SHOCK) && has(646,id("kyurem-white"),MV_ICE_BURN),"both Kyurem fusion pools include their signatures");
  ck(has(80,id("slowbro-galar"),MV_SHELL_SIDE_ARM),"Galar Slowbro learns Shell Side Arm");
  bool valid=true;
  for(const auto &r:FORM_LEARN_INDEX){MoveId all[128];int n=formLearnableList(r.dex,r.form,100,all,128);valid &= n>0 && n<128;for(int i=0;i<n;i++)valid &= all[i]>0 && all[i]<MOVE_COUNT;}
  ck(valid,"all 69 overridden form pools are nonempty and bounded");
  pet.dbgHatchAs(479,false);pet.ageMinutes=79*MINUTES_PER_LEVEL;pet.learnQCount=0;
  pet.moves[0]=MV_TACKLE;pet.moves[1]=MV_GROWL;pet.moves[2]=MV_THUNDERBOLT;pet.moves[3]=MV_SHADOW_BALL;
  MoveId before[4];memcpy(before,pet.moves,sizeof(before));
  formOpenTarget(0);for(int i=0;i<formCount(479);i++)if(formAt(479,i)->id==id("rotom-wash"))formPage=i+1;
  render();onTap(230,348);render();
  shot("form-move-choice.ppm");
  ck(pet.form==id("rotom-wash") && !memcmp(before,pet.moves,sizeof(before)),"form selection opens an offer without replacing any selected move");
  onTap(200,222);
  ck(pet.moves[2]==MV_HYDRO_PUMP && pet.moves[0]==before[0] && pet.moves[1]==before[1] && pet.moves[3]==before[3],"user chooses exactly which of the four moves to replace");
  formClose();Pet re;re.begin();ck(re.moves[2]==MV_HYDRO_PUMP,"chosen form move persists after restart");
  formOpenTarget(0);for(int i=0;i<formCount(479);i++)if(formAt(479,i)->id==id("rotom-heat"))formPage=i+1;
  render();onTap(230,348);memcpy(before,pet.moves,sizeof(before));onTap(230,350);
  ck(!memcmp(before,pet.moves,sizeof(before)) && has(479,pet.form,MV_OVERHEAT),"declining leaves all four moves intact and allows later relearning");formClose();
  party.begin();for(int i=0;i<BOX_SLOTS;i++){PartyMon m;m.dex=1+i;m.level=80;m.moves[0]=MV_TACKLE;party.box[i]=m;}party.save();
  Party q;q.begin();ck(q.boxCount()==300 && q.box[255].dex==256 && q.box[299].dex==300,"all 300 individuals including indices beyond 255 survive reload");
  partyOpen=boxOpen=true;boxPage=49;boxDetail=0;render();shot("box-300-last-page.ppm");boxTap(280,265);
  ck(boxDetail==300,"last page opens slot 300, not wrapped slot 44");
  boxTap(200,90);ck(movePickOpen && movePickBox==300,"boxed move picker targets the 300th individual");movePickOpen=false;
  ck(party.swapActive(pet,true,299) && pet.speciesId==300,"last boxed individual can exchange with the live pet");
  party.box[299].dex=892;party.box[299].level=80;party.box[299].form=0;party.box[299].moves[0]=MV_TACKLE;party.save();
  ck(party.selectForm(true,299,id("urshifu-rapid-strike")),"form selection accepts box indices above 255");
  formOpenTarget(306);ck(formTarget==306,"form modal retains the full box target index");
  render();onTap(230,348);for(int i=0;i<128 && party.box[299].moves[2]!=MV_SURGING_STRIKES;i++)onTap(200,222);
  ck(party.box[299].moves[2]==MV_SURGING_STRIKES && party.box[299].moves[0]==MV_TACKLE,"boxed form offer changes only the user-selected slot on individual 300");formClose();
  Combatant atk,def;atk.dex=892;atk.form=id("urshifu-rapid-strike");atk.level=80;atk.hp=atk.maxHp=60000;
  def.dex=143;def.level=80;def.hp=def.maxHp=60000;for(auto &v:atk.base)v=150;for(auto &v:def.base)v=150;
  TurnLog log;battleAct(atk,def,MV_SURGING_STRIKES,log);ck(log.hits==3 && log.crit,"Surging Strikes is exactly three critical hits");
  atk.base[SI_ATK]=300;atk.base[SI_SPA]=50;auto phys=battleDamage(atk,def,MV_SHELL_SIDE_ARM,false,255);
  atk.base[SI_ATK]=50;auto spec=battleDamage(atk,def,MV_SHELL_SIDE_ARM,false,255);ck(phys>spec,"Shell Side Arm chooses the stronger damage category");
  ck(!formMoveUsable(892,0,MV_SURGING_STRIKES),"strict signatures cannot be used in the wrong form");
  std::vector<uint8_t> backup(SAVE_MAX_BYTES);pet.saveNow();size_t n=saveExport(backup.data(),backup.size());
  nvs().clear();ck(n>22000 && saveImport(backup.data(),n),"full 300-slot save transfers in version 5");
  Party imported;imported.begin();ck(imported.box[299].form==id("urshifu-rapid-strike"),"transfer retains the last boxed form");
  std::vector<uint8_t> legacy(12+72*67,0);memcpy(legacy.data(),"TFR2",4);legacy[4]=66;legacy[6]=72;
  PartyMon last=imported.box[299];memcpy(legacy.data()+8+72*65,&last,72);
  uint32_t hash=2166136261u;for(size_t i=0;i<legacy.size()-4;i++){hash^=legacy[i];hash*=16777619u;}memcpy(legacy.data()+legacy.size()-4,&hash,4);
  nvs()["rosterF"]=legacy;Party migrated;migrated.begin();
  ck(migrated.box[59].form==last.form && migrated.boxCount()==1 && migrated.box[299].empty(),"beta.3/4 TFR2 migration preserves all 60 old positions and clears only the new tail");
  return bad?1:0;
}
