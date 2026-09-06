#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "battle.h"
#include "link.h"
#include "types.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=732;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;bool wasPressed=false;
uint32_t millis(){return 0;}void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);if(!b)bad++;}
static const FormEntry &byKey(const char *key) {
  for(uint16_t i=0;i<FORM_COUNT;i++) if(!strcmp(FORM_TBL[i].key,key)) return FORM_TBL[i];
  fprintf(stderr,"Missing form: %s\n",key);exit(2);
}
struct Pipe{Link *peer;uint32_t n=0;};
static void send(void *v,const uint8_t *p,uint8_t n){auto *x=(Pipe*)v;if(++x->n%4)x->peer->onPacket(p,n);}
int main(){
  ck(FORM_COUNT==176,"176 animated forms, no appearance category");
  ck(!formFind(6,10365)&&!formFind(358,10531)&&!formFind(869,10392),"three still-only forms are absent from the supported table");
  ck(!formEligible(6,10365,100)&&!formEligible(358,10531,100)&&!formEligible(869,10392,100),"removed forms cannot be selected even at level 100");
  ck(formCount(6)==1&&formCount(358)==0&&formCount(869)==0&&formFind(890,10359),"only still-only forms disappear; animated Eternamax remains");
  char sprite[40];formSpriteName(sprite,sizeof(sprite),6,10365,false);
  ck(!strcmp(sprite,"p006.bin"),"an old excluded form uses base art even if the former still file remains installed");
  bool valid=true;
  for(uint16_t i=0;i<FORM_COUNT;i++) {
    const auto &f=FORM_TBL[i];const auto d=formDex(f.dex,f.id);
    valid &= f.id && f.category<=FC_GIANT && f.type1<TYPE_COUNT &&
      !formEligible(f.dex,f.id,f.level-1) && formEligible(f.dex,f.id,f.level) &&
      d.type1==f.type1 && d.type2==f.type2 && d.bAtk==f.atk &&
      d.evolvesTo==DEX_TBL[f.dex].evolvesTo;
  }
  ck(valid,"every threshold, type, stat and ordinary evolution identity is valid");
  auto mega=byKey("charizard-mega-x"), mask=byKey("ogerpon-hearthflame-mask");
  ck(mega.level==70 && byKey("kyurem-black").level==80 && mask.level==60,"60 / 70 / 80 categories");
  ck(formDex(6,mega.id).type2==T_DRAGON && typeEffVsDex(T_ROCK,6,mega.id)==200 && typeEffVsDex(T_ROCK,6)==400,"Charizard X changes Flying to Dragon in the real type chart");
  ck(!formFind(25,mega.id) && formDex(25,65535).type1==DEX_TBL[25].type1,"wrong species and unknown IDs use safe base data");
  ck(formMoveType(1017,mask.id,MV_IVY_CUDGEL)==T_FIRE,"Hearthflame Ivy Cudgel is Fire");
  ck(formMoveType(493,byKey("arceus-water").id,MV_JUDGMENT)==T_WATER,"Judgment follows the Arceus form");
  ck(formMoveType(773,byKey("silvally-fairy").id,MV_MULTI_ATTACK)==T_FAIRY,"Multi-Attack follows the Silvally form");
  ck(formMoveType(649,byKey("genesect-chill").id,MV_TECHNO_BLAST)==T_ICE,"Techno Blast uses its drive type, not Bug/Steel");
  Combatant attacker,defender;attacker.dex=6;attacker.level=80;defender.dex=143;
  attacker.hp=defender.hp=attacker.maxHp=defender.maxHp=1000;
  for(auto &v:attacker.base)v=150;for(auto &v:defender.base)v=150;
  uint16_t baseDamage=battleDamage(attacker,defender,MV_DRAGON_CLAW,false,255);
  attacker.form=mega.id;
  ck(battleDamage(attacker,defender,MV_DRAGON_CLAW,false,255)>baseDamage,"Dragon STAB changes actual damage after Mega X selection");
  attacker.dex=1017;attacker.form=mask.id;defender.dex=1;TurnLog turn;
  battleAct(attacker,defender,MV_IVY_CUDGEL,turn);
  ck(turn.effPct==200 && turn.damage>0,"battle action uses Fire Ivy Cudgel against Grass, not the original Grass move type");
  nvs().clear();Pet p;p.begin();p.dbgHatchAs(6,true);p.ageMinutes=69*MINUTES_PER_LEVEL;
  ck(p.selectForm(mega.id),"level 70 can select Mega Charizard X");
  auto atk=p.atkStat();p.saveNow();Pet re;re.begin();
  ck(re.form==mega.id && re.shiny && re.atkStat()==atk,"live form, shiny and altered stats survive reload");
  p.ageMinutes=FAREWELL_AGE_MIN;
  ck(p.canFarewellNow(),"good farewell still opens at original 73 / 24-hour boundary");
  p.selectForm(0);ck(p.canFarewellNow(),"no form unlock or transformation is required for good farewell");
  p.dbgHatchAs(670,false);p.ageMinutes=FAREWELL_AGE_MIN;p.selectForm(byKey("floette-mega").id);
  ck(!p.canFarewellNow(),"Mega Floette does NOT count as ordinary final evolution");
  PartyMon m;m.dex=6;m.level=80;m.form=mega.id;m.shiny=1;m.moves[0]=MV_SURF;strcpy(m.nick,"FORM");
  party.begin();party.add(m);party.swapPartyBox(0,59);Party q;q.begin();
  ck(q.slots[0].empty() && q.box[59].form==mega.id && q.box[59].moves[0]==MV_SURF,"party-to-box form and moves survive authoritative roster reload");
  q.swapPartyBox(1,59);Party q2;q2.begin();ck(q2.slots[1].form==mega.id && q2.box[59].empty(),"box-to-party keeps the form with the individual");
  p.reviveFrom(q2.slots[1]);ck(p.form==mega.id && p.frozen,"reviving carries the selected form");
  Combatant c,d;combatantFromPet(c,p);LinkMon wire;linkMonFrom(wire,c);linkMonTo(d,wire);
  ck(d.form==mega.id && d.base[SI_ATK]==c.base[SI_ATK],"LAN wire retains form identity and derived stats");
  wire.level=50;linkMonTo(d,wire);ck(d.form==mega.id,"battle level caps do not undo a previously unlocked form");
  // A real full-save transfer, including both legacy copies and form roster.
  uint8_t backup[SAVE_MAX_BYTES];p.saveNow();size_t n=saveExport(backup,sizeof(backup));
  ck(n>4096 && n<=SAVE_MAX_BYTES && backup[4]==SAVE_VERSION,"current whole save fits the new bound, including legacy recovery copies");
  Link tx,rx;Pipe a{&rx},b{&tx};tx.send=send;tx.ctx=&a;rx.send=send;rx.ctx=&b;
  ck(tx.beginSave(true,"FORM",backup,n)&&rx.beginSave(false,"RECV"),"form save enters transport");
  tx.id=17;rx.id=29;tx.start();
  for(uint32_t t=0;t<180000 && rx.state!=LINK_SAVE_READY;t+=100){tx.tick(t);rx.tick(t);}
  ck(rx.state==LINK_SAVE_READY && rx.savePeerSize==n && !memcmp(rx.saveData,backup,n),"lossy multi-device transport preserves every form-save byte");
  nvs().clear();ck(saveImport(backup,n),"form save restores");Pet r;r.begin();Party z;z.begin();
  ck(r.form==mega.id && z.slots[1].form==mega.id,"live and party forms restored together");
  z.slots[1].form=65535;z.save();Party future;future.begin();ck(future.slots[1].form==65535,"unrecognized form ID is preserved, never rewritten to zero");
  nvs()["rosterF"][0]^=1;auto corrupt=nvs()["rosterF"];Party recovery;recovery.begin();
  ck(!recovery.selectForm(false,1,mega.id),"an unreadable/future roster cannot be overwritten through form selection");
  recovery.save();ck(nvs()["rosterF"]==corrupt,"a failed roster read preserves the authoritative bytes for recovery");
  p.newEgg();p.dbgHatchAs(25,false);ck(p.form==0,"a new individual never inherits the previous form");
  nvs().clear();Pet legacy;legacy.begin();legacy.dbgHatchAs(358,true);legacy.form=10531;
  strcpy(legacy.nick,"KEEP");legacy.saveNow();Party oldRoster;oldRoster.begin();
  PartyMon old=legacy.storageSnapshot();old.dex=869;old.form=10392;oldRoster.add(old);
  size_t oldSize=saveExport(backup,sizeof(backup));nvs().clear();
  ck(oldSize&&saveImport(backup,oldSize),"old selected-form records still pass full-save transfer");
  Pet kept;kept.begin();Party keptRoster;keptRoster.begin();
  ck(kept.speciesId==358&&kept.form==10531&&kept.shiny&&!strcmp(kept.nick,"KEEP")&&keptRoster.slots[0].dex==869&&keptRoster.slots[0].form==10392,"excluded-form save keeps both individuals and IDs without deleting or renumbering them");
  ck(formDex(358,10531).bSpA==DEX_TBL[358].bSpA&&kept.selectForm(0),"excluded form safely uses base stats and can be reset explicitly");
  return bad?1:0;
}
