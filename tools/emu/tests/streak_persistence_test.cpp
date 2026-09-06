// Reproduce the released 3.0.0 APK reader failure and separate it from swapping.
#include "Arduino.h"
#include "pet.h"
#include "party.h"
#include "save.h"
#include "nvs_file.h"
#include <chrono>
#include <filesystem>
#include <cstdio>
uint32_t g_seed=19283; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
uint32_t millis(){return 1;}
void FakeESP::restart(){} int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");} void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*s){printf("%s %s\n",ok?"PASS":"FAIL",s);if(!ok)bad++;}
static void day(Pet&p,uint32_t d){p.setClock(d*86400+43200);}
// Frozen reproduction of tools/android/android_main.cpp at 4fc1def (3.0.0).
// DO NOT use this partial reader in production.
static NvsStore readReleased300(const char *path) {
  FILE *f=fopen(path,"rb"); NvsStore loaded;
  if(!f)return loaded;
  uint32_t count=0;
  if(fread(&count,4,1,f)!=1 || count>1024){fclose(f);return loaded;}
  for(uint32_t i=0;i<count;++i){
    uint32_t ks=0,vs=0;
    if(fread(&ks,4,1,f)!=1 || !ks || ks>64)break;
    std::string key(ks,'\0');
    if(fread(key.data(),1,ks,f)!=ks || fread(&vs,4,1,f)!=1 || vs>4096)break;
    std::vector<uint8_t> value(vs);
    if(vs && fread(value.data(),1,vs,f)!=vs)break;
    loaded[key]=std::move(value);
  }
  fclose(f);return loaded;
}
static bool history(const Pet&p,uint16_t n,uint32_t d){
  return p.streak==n && p.bestStreak==n && p.lastCareDay==d;
}
int main(){
  std::string path="streak-restart-"+std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count())+".nvs";
  NvsFile disk; ck(disk.load(path.c_str(),nvs()),"new isolated fixture");
  Pet p;p.begin();p.dbgHatchAs(6,false);
  for(uint32_t d=201;d<=210;++d){day(p,d);p.feed();}
  while(p.hasLearnOffer())p.declineLearn();
  ck(history(p,10,210) && p.eggBonusDays()==10,"ten actual daily care actions establish a ten-day streak");
  Party bank;bank.begin();PartyMon m;m.dex=25;m.level=80;m.moves[0]=1;
  bank.box[299]=m;bank.boxSave();
  const auto keys=nvs();
  ck(bank.swapActive(p,true,299) && p.speciesId==25 && history(p,10,210),
     "bringing a protected companion from box slot 300 retains current/best/date");
  ck(nvs()["strk"]==keys.at("strk") && nvs()["bstrk"]==keys.at("bstrk") &&
     nvs()["cday"]==keys.at("cday"),"box exchange does not rewrite player history to zero");
  ck(bank.swapActive(p,true,299) && p.speciesId==6 && history(p,10,210),
     "returning the original growing individual also retains player history");
  ck(bank.swapActive(p,true,299) && disk.save(path.c_str(),nvs(),true),
     "save a genuine post-box-exchange snapshot in the unchanged disk format");
  const auto whole=nvs();
  nvs()=readReleased300(path.c_str());
  ck(nvs().count("bstrk") && nvs().count("cday") && !nvs().count("strk") && !nvs().count("rosterF"),
     "3.0.0 disk reader reproduces loss of current streak but retains best/date");
  Pet old;old.begin();day(old,210);old.feed();
  ck(old.streak==0 && old.bestStreak==10 && old.lastCareDay==210,
     "old restart shows 0/10 and same-day care cannot increment it");
  day(old,211);old.feed();
  ck(old.streak==1 && old.bestStreak==10,"old next-day care incorrectly restarts at one");
  nvs().clear();NvsFile fixed;
  ck(fixed.load(path.c_str(),nvs()) && nvs()==whole,"3.1.1 reader restores the identical file in full");
  Pet current;current.begin();bank.begin();day(current,210);
  ck(history(current,10,210) && current.eggBonusDays()==10 && current.speciesId==25,
     "3.1.1 cold restart keeps ten-day history, bonus and incoming companion");
  current.feed();ck(history(current,10,210),"same-day companion care keeps ten days");
  day(current,211);current.feed();ck(history(current,11,211),"next-day companion care advances to eleven");
  // An interrupted swap must not replace history with the scratch incoming Pet.
  nvsFailKey()="dexn";
  ck(bank.swapActive(current,true,299) && nvs()["rosterF"][5]==1,
     "simulate interruption while committing the active creature");
  nvsFailKey().clear();ck(fixed.save(path.c_str(),nvs(),true),"persist interrupted swap fixture");
  nvs().clear();disk.load(path.c_str(),nvs());Pet recovered;recovered.begin();
  ck(history(recovered,11,211) && recovered.speciesId==6 && nvs()["rosterF"][5]==0,
     "disk restart and swap recovery preserve all player history");
  uint8_t payload[SAVE_MAX_BYTES];size_t n=saveExport(payload,sizeof(payload));
  nvs().clear();ck(n && saveImport(payload,n),"whole-save transfer imports streak fixture");
  Pet transferred;transferred.begin();
  ck(history(transferred,11,211),"Android/Wear/ESP transfer codec keeps streak, best and date");
  day(transferred,213);ck(transferred.eggBonusDays()==0,"genuinely missing a full day still removes the bonus");
  transferred.feed();ck(transferred.streak==1 && transferred.bestStreak==11,
     "real care gap resets only current streak, not best history");
  remove(path.c_str());
  printf("%s\n",bad?"FAILURES":"all good");return bad?1:0;
}
