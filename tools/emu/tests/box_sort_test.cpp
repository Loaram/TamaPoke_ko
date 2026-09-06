#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "i18n.h"
#include "korean_text.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
uint32_t g_seed=400;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();void render();void onTap(int16_t,int16_t);void onSwipe(int);void onSwipeV(int);
extern Pet pet;extern KoreanCanvas *gfx;
extern bool partyOpen,boxOpen,releaseConfirm,boxPagePicker,movePickOpen,boxSortOpen,boxSortFailed;
extern uint8_t partyDetail,boxSwapFrom,boxPage,boxSortChoice;extern uint16_t boxDetail,boxSel;
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);bad+=!b;}
static PartyMon mon(int dex,int level,int id){PartyMon m;m.dex=dex;m.level=level;m.medals=id;m.shiny=id%2;
  m.ivAtk=id%32;m.trSpe=id%7;m.moves[0]=1;m.moves[1]=2;snprintf(m.nick,sizeof(m.nick),"N%03d",300-id);m.care[31]=id%256;return m;}
static auto records(){std::vector<std::vector<uint8_t>> v;for(auto &m:party.box){auto p=(uint8_t*)&m;v.emplace_back(p,p+sizeof(m));}std::sort(v.begin(),v.end());return v;}
static void shot(const char*name){render();FILE*f=fopen(name,"wb");if(!f)abort();fprintf(f,"P6\n466 466\n255\n");
  for(int i=0;i<466*466;i++){uint16_t c=gfx->buffer()[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);}
static void open(){partyOpen=boxOpen=true;boxDetail=boxSel=partyDetail=0;releaseConfirm=boxPagePicker=movePickOpen=boxSortOpen=boxSortFailed=false;boxSortChoice=0;}
int main(){
  nvs().clear();setup();pet.dbgHatchAs(6,false);pet.learnQCount=0;
  for(auto &m:party.box)m=PartyMon();for(int i=0;i<PARTY_SLOTS;i++)party.slots[i]=mon(25+i,60,i);
  party.save();auto quota=pet.farewellsRemaining();auto active=pet.storageSnapshot();
  PartyMon team[PARTY_STORAGE_SLOTS];memcpy(team,party.slots,sizeof(team));
  auto empty=nvs();ck(party.sortBox(BOX_SORT_NAME)&&empty==nvs(),"empty sort does not write storage");
  party.box[299]=mon(25,90,299);party.save();ck(party.sortBox(BOX_SORT_LEVEL)&&party.box[0].dex==25&&party.box[299].empty(),"single last-slot individual moves to front");
  for(int i=0;i<BOX_SLOTS;i++)party.box[i]=mon((i*79)%1025+1,(i*13)%100+1,i);
  party.box[23]=mon(25,90,23);party.box[257]=mon(25,90,257);party.box[257].form=1;
  party.save();auto full=records();ck(party.sortBox(BOX_SORT_DEX),"full 300-slot dex sort succeeds");
  bool ordered=true;for(int i=1;i<BOX_SLOTS;i++)ordered &= party.box[i-1].dex<=party.box[i].dex;
  ck(ordered&&records()==full,"dex ascending preserves every byte of all 300 records, forms and care included");
  int a=-1,b=-1;for(int i=0;i<BOX_SLOTS;i++){if(party.box[i].medals==23)a=i;if(party.box[i].medals==257)b=i;}
  ck(a<b,"same dex and level retain original order across forms and shinies");
  ck(party.sortBox(BOX_SORT_LEVEL),"level sort succeeds");ordered=true;
  for(int i=1;i<BOX_SLOTS;i++)ordered &= party.box[i-1].level>=party.box[i].level;
  ck(ordered&&records()==full,"level descending preserves full records");
  auto same=nvs();ck(party.sortBox(BOX_SORT_LEVEL)&&same==nvs(),"repeated sort is idempotent");
  for(auto &m:party.box)m=PartyMon();int ids[]={25,4,1,7,58,4};
  for(int i=0;i<6;i++)party.box[299-i*51]=mon(ids[i],i==5?90:60,i);
  party.save();auto sparse=records();gLang=LANG_EN;
  ck(party.sortBox(BOX_SORT_NAME),"Korean canonical sort also works in English UI");
  int expected[]={58,7,1,4,4,25};ordered=true;for(int i=0;i<6;i++)ordered &= party.box[i].dex==expected[i];
  ck(ordered&&party.box[3].level==90&&records()==sparse,"Gadi, Kkobugi, Isanghaessi, Pairi, Pikachu order ignores nicknames; ties use level");
  ck(party.box[6].empty()&&party.box[299].empty(),"holes are packed to end across all 50 pages");
  Party reloaded;reloaded.begin();ck(!memcmp(reloaded.box,party.box,sizeof(party.box)),"sorted order survives reload");
  static uint8_t save[SAVE_MAX_BYTES];size_t n=saveExport(save,sizeof(save));auto sorted=std::vector<PartyMon>(party.box,party.box+BOX_SLOTS);
  nvs().clear();ck(n&&saveImport(save,n),"sorted v7 save exports and imports without a format change");party.begin();
  ck(!memcmp(sorted.data(),party.box,sizeof(party.box)),"whole-save import retains exact sorted record order");
  auto live=pet.storageSnapshot();ck(!memcmp(team,party.slots,sizeof(team))&&!memcmp(&live,&active,sizeof(live))&&pet.farewellsRemaining()==quota,"sorting does not change party, live care or farewell quota");
  ck(!party.sortBox(static_cast<BoxSortOrder>(255)),"invalid sort criterion is rejected");
  auto good=party.box[0];party.box[0].dex=1026;ck(!party.sortBox(BOX_SORT_NAME)&&party.box[0].dex==1026,"invalid species cannot index outside name table");party.box[0]=good;
  gLang=LANG_KO;open();boxPage=49;boxSwapFrom=1;auto before=nvs();
  onTap(170,411);ck(boxSortOpen&&!boxSortChoice&&boxPage==49,"sort button opens full-box menu on last page");shot("box-sort-menu.ppm");
  onTap(233,145);ck(boxSortChoice==1,"name row asks for confirmation");shot("box-sort-confirm.ppm");
  onTap(20,120);ck(boxSortChoice==1&&boxOpen,"confirmation margins cannot select underlying box");
  onTap(233,315);ck(!boxSortChoice&&boxSortOpen,"No returns to sort menu");
  onSwipe(-1);ck(!boxSortOpen&&boxPage==49&&boxSwapFrom==1&&nvs()==before,"cancel preserves current page, pending exchange and save");
  onTap(170,411);onTap(233,220);onSwipeV(1);ck(!boxSortChoice&&boxSortOpen,"swipe first cancels confirmation, not page");
  onTap(233,297);onTap(233,250);ck(!boxSortOpen&&boxPage==0&&!boxSwapFrom&&!boxDetail&&!boxSel&&!partyDetail,"confirmed sort resets page and stale slot selections");
  shot("box-sort-result.ppm");int first=party.box[0].dex;onTap(118,115);ck(boxDetail==1&&party.box[0].dex==first,"sorted slot opens correct individual");
  onTap(326,360);onTap(233,315);ck(party.box[0].dex==first&&!releaseConfirm,"cancelled release after sort preserves the selected individual");
  onTap(213,360);ck(!boxOpen&&boxSel==1,"sorted slot supports full-party exchange selection");onTap(118,115);ck(party.slots[0].dex==first,"exchange acts on sorted slot, not stale index");
  open();boxPage=7;boxSwapFrom=2;party.box[299]=mon(1,100,99);party.save();before=nvs();auto fail=std::vector<PartyMon>(party.box,party.box+BOX_SLOTS);
  nvsFailKey()="rosterF";onTap(170,411);onTap(233,220);onTap(233,250);
  ck(boxSortOpen&&boxSortFailed&&!boxSortChoice&&boxPage==7&&boxSwapFrom==2,"write failure displays error and retains UI context");
  ck(!memcmp(fail.data(),party.box,sizeof(party.box))&&nvs()==before,"write failure restores every RAM record and keeps durable snapshot");
  ck(!party.sortBox(BOX_SORT_LEVEL),"read-only storage rejects further sorting");nvsFailKey().clear();party.begin();
  ck(!memcmp(fail.data(),party.box,sizeof(party.box)),"failure recovery reloads the original order");
  auto &raw=nvs()["rosterF"];PartyMon pending=mon(1,60,0);pending.care[0]=1;raw[5]=1;
  memcpy(raw.data()+8+PARTY_RECORD_BYTES*(PARTY_STORAGE_SLOTS+BOX_SLOTS),&pending,sizeof(pending));
  uint32_t hash=2166136261u;for(size_t i=0;i<raw.size()-4;i++){hash^=raw[i];hash*=16777619u;}
  memcpy(raw.data()+raw.size()-4,&hash,4);party.begin();before=nvs();
  ck(party.writable()&&!party.sortBox(BOX_SORT_DEX)&&before==nvs(),"unfinished active-swap journal prevents sorting until recovery");
  return bad?1:0;
}
