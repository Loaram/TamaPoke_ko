#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=734; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);} int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void partyTap(int16_t,int16_t); void boxTap(int16_t,int16_t);
extern Pet pet; extern bool partyOpen,boxOpen,releaseConfirm;
extern uint8_t partyDetail,boxSwapFrom,boxPage;extern uint16_t boxDetail,boxSel;
static int bad=0;
static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);if(!b)bad++;}
static PartyMon mon(int dex){PartyMon m; m.dex=dex;m.level=80;m.ivAtk=m.ivDef=m.ivSpe=m.ivHp=20;m.moves[0]=1;return m;}
static void checksum(std::vector<uint8_t>& b){uint32_t h=2166136261u;for(size_t i=0;i<b.size()-4;i++){h^=b[i];h*=16777619u;}memcpy(b.data()+b.size()-4,&h,4);}
int main(){
  nvs().clear();setup();pet.dbgHatchAs(6,true);pet.ageMinutes=1399;pet.lastLearnLevel=pet.level();
  while(pet.hasLearnOffer())pet.declineLearn();
  pet.form=10134;pet.energy=13;pet.fullness=48;pet.joy=57;pet.hygiene=66;
  pet.bond=37;pet.careMistakes=2;pet.trAtk=4;pet.trDef=5;pet.trSpe=6;
  strcpy(pet.nick,"GROWER");pet.medals=17;pet.badgesX[8]=0xA5;
  pet.streak=8;pet.totalMedals=123;pet.saveNow();
  PartyMon before=pet.storageSnapshot();
  for(int i=0;i<PARTY_SLOTS;i++)party.slots[i]=mon(25+i);
  for(int i=0;i<BOX_SLOTS;i++)party.box[i]=mon(100+i);
  party.save();
  partyOpen=true;partyDetail=1;partyTap(160,360);
  ck(pet.speciesId==25 && party.slots[0].dex==6 && !partyOpen,"party button exchanges a living pet without deleting either");
  ck(!memcmp(&before,&party.slots[0],sizeof(before)),"outgoing individual preserves exact minute, energy, bond, IVs, training, moves, nickname, shiny and form");
  ck(pet.frozen && pet.badgesX[8]==0xA5 && pet.streak==8 && pet.totalMedals==123,"legacy companion behavior and player-wide achievements stay intact");
  ck(party.swapActive(pet,false,0),"returning the growing individual is allowed");
  PartyMon after=pet.storageSnapshot();
  ck(!memcmp(&before,&after,sizeof(before)) && !pet.frozen,"the original grower resumes rather than becoming a retired companion");
  ck(party.count()==5 && party.boxCount()==BOX_SLOTS,"full storage stays full after reversible exchange");
  Pet restored;restored.begin();PartyMon reload=restored.storageSnapshot();
  ck(!memcmp(&before,&reload,sizeof(before)),"restored live care survives reopening the save");
  partyOpen=boxOpen=true;partyDetail=0;boxDetail=0;boxSwapFrom=0;boxPage=9;
  boxTap(280,265);render();
  ck(boxDetail==60 && boxOpen,"a full party still opens the last box slot's detail sheet");
  boxTap(118,360);
  ck(pet.speciesId==159 && party.box[59].dex==6 && !boxOpen,"box bring-back button swaps directly with active pet at full capacity");
  ck(party.swapActive(pet,true,59) && pet.speciesId==6,"box round trip restores the grower");
  partyOpen=boxOpen=true;boxDetail=60;boxTap(213,360);
  ck(!boxOpen&&boxSel==60,"TO PARTY still offers a storage-slot exchange when the party is full");
  int was=party.slots[0].dex;partyTap(118,115);
  ck(party.slots[0].dex==159&&party.box[59].dex==was&&pet.speciesId==6,"box-to-party exchange leaves the live pet untouched");
  party.swapPartyBox(0,59);boxSel=boxSwapFrom=0;partyDetail=0;partyOpen=false;
  pet.learnQCount=1;pet.learnQueue[0]=1;
  auto unchanged=nvs();ck(!party.swapActive(pet,false,0)&&nvs()==unchanged,"pending move decision blocks exchange without changing storage");
  pet.learnQCount=0;pet.ceremony=CER_FAREWELL;
  ck(!party.swapActive(pet,true,59),"an ending cannot be interrupted by exchanging pets");pet.ceremony=CER_NONE;
  ck(!party.swapActive(pet,false,PARTY_SLOTS)&&!party.swapActive(pet,true,BOX_SLOTS),"out-of-range indices are rejected");
  uint8_t backup[SAVE_MAX_BYTES];pet.saveNow();size_t n=saveExport(backup,sizeof(backup));
  ck(n>22000&&saveValidate(backup,n)&&backup[4]==SAVE_VERSION,"full roster, care metadata and recovery copies fit the v5 transport envelope");
  nvs().clear();ck(saveImport(backup,n),"whole-save import accepts the enlarged care-aware save");
  pet.begin();party.begin();after=pet.storageSnapshot();
  ck(!memcmp(&before,&after,sizeof(before))&&party.box[59].dex==159,"cross-device save round trip keeps both active and boxed individuals");
  // Simulate power loss after the atomic roster commit but before any live key write.
  auto committed=nvs()["rosterF"];PartyMon next=party.slots[0];
  Pet normalized;normalized.energy=pet.energy;normalized.reviveFrom(next);next=normalized.storageSnapshot();
  memcpy(committed.data()+8,&before,PARTY_RECORD_BYTES);
  memcpy(committed.data()+8+PARTY_RECORD_BYTES*(PARTY_STORAGE_SLOTS+BOX_SLOTS),&next,PARTY_RECORD_BYTES);
  committed[5]=1;checksum(committed);nvs()["rosterF"]=committed;
  pet.begin();party.begin();
  ck(pet.speciesId==next.dex&&party.slots[0].dex==6&&nvs()["rosterF"][5]==0,"interrupted exchange replays the committed incoming pet and clears recovery only after verified saving");
  pet.begin();party.begin();ck(pet.speciesId==next.dex&&party.slots[0].dex==6,"recovery is idempotent across another restart");
  unchanged=nvs();int oldDex=pet.speciesId;PartyMon oldSlot=party.slots[0];
  nvsFailKey()="rosterF";
  ck(!party.swapActive(pet,false,0)&&pet.speciesId==oldDex&&party.slots[0].dex==oldSlot.dex&&nvs()==unchanged,"failed atomic roster write rolls back RAM and retains both saved individuals");
  nvsFailKey().clear();party.begin();nvsFailKey()="dexn";
  ck(party.swapActive(pet,false,0)&&nvs()["rosterF"][5]==1,"failed live-key write retains the committed recovery record");
  nvsFailKey().clear();pet.begin();party.begin();
  ck(pet.speciesId==6&&party.slots[0].dex==oldDex&&nvs()["rosterF"][5]==0,"reboot repairs a partially written live save without duplicating or losing a Pokemon");
  // The previous form beta's 36-byte tagged roster migrates without losing form IDs.
  std::vector<uint8_t> old(12+36*66,0);
  memcpy(old.data(),"TFR1",4);old[4]=66;old[6]=36;
  memcpy(old.data()+8,&before,36);checksum(old);nvs()["rosterF"]=old;party.begin();
  ck(party.slots[0].dex==6&&party.slots[0].form==10134&&!party.slots[0].care[0],"beta.1/2 TFR1 records retain form and legacy companion policy");
  pet.newEgg();ck(party.swapActive(pet,false,0)&&party.slots[0].empty(),"egg-time bring-back still frees the selected slot");
  return bad?1:0;
}
