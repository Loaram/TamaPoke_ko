#include "Arduino.h"
#include "Preferences.h"
#include "trade.h"
#include "save.h"
#include <cstdio>
#include <deque>
#include <vector>
#include <cstring>
#include <new>
uint32_t g_seed=350,clockNow=0;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;bool wasPressed=false;
uint32_t millis(){return clockNow;}void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}void sfxPlay(uint8_t){}
static int bad=0;void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);fflush(stdout);bad+=!b;}
struct PowerCut{};
struct Node {Party party;Trade tx;Link link;NvsStore ram,disk;bool fail=false;int cuts=0,cutAt=0;};
struct Frame{int to;std::vector<uint8_t>b;};std::deque<Frame>queue;Node nodes[2];int active=0,sent=0,drop=0;
bool flush(){auto&n=nodes[active];if(n.fail)return false;n.disk=nvs();if(n.cutAt&&++n.cuts==n.cutAt)throw PowerCut();return true;}
void select(int i){active=i;nvs()=nodes[i].ram;tradeStorageBlocked=nodes[i].tx.pending();}
void park(){nodes[active].ram=nvs();}
void send(void*c,const uint8_t*b,uint8_t n){if(drop&&++sent%drop==0)return;queue.push_back({1-(int)(intptr_t)c,{b,b+n}});}
PartyMon mon(int dex){PartyMon m;m.dex=dex;m.level=70;m.ivAtk=31;m.ivDef=20;m.moves[0]=1;m.shiny=dex==25;strcpy(m.nick,"TEST");return m;}
void boot(int i,bool fresh=false){
 select(i);auto&n=nodes[i];if(!fresh)nvs()=n.disk;
 tradeStorageBlocked=false;n.party.~Party();new(&n.party)Party();n.party.begin();n.tx=Trade();n.tx.checkpoint=flush;n.tx.load(n.party);
 n.link.begin(false,i?"B":"A");n.link.send=send;n.link.ctx=(void*)(intptr_t)i;n.tx.attach(n.link);park();
}
void init(uint8_t a=TRADE_SWAP,uint8_t b=TRADE_SWAP,uint16_t slot=0){
 queue.clear();drop=sent=0;clockNow=0;
 for(int i=0;i<2;i++){
  nodes[i].~Node();new(&nodes[i])Node();select(i);nvs().clear();nodes[i].party.begin();
  auto&m=slot<5?nodes[i].party.slots[slot]:nodes[i].party.box[slot-5];m=(i?b:a)==TRADE_RECEIVE?PartyMon():mon(i?133:25);
  nodes[i].party.save();nodes[i].ram=nodes[i].disk=nvs();boot(i);
  select(i);ck(nodes[i].tx.begin(i?b:a,slot),"begin validates and durably locks only selected slot");park();
 }
}
void pump(int rounds=12){for(int t=0;t<rounds;t++){
 clockNow+=500;for(int i=0;i<2;i++){select(i);nodes[i].tx.tick(clockNow);park();}
 int count=queue.size();while(count--&&!queue.empty()){auto f=queue.front();queue.pop_front();select(f.to);nodes[f.to].link.onPacket(f.b.data(),f.b.size());park();}
}}
void approve(int i){select(i);ck(nodes[i].tx.confirm(),"explicit local approval is durable");park();}
int main(){
 init();pump();ck(nodes[0].tx.j.phase==TX_MATCH&&nodes[1].tx.j.phase==TX_MATCH,"both previews wait without exchanging");
 ck(nodes[0].tx.code()==nodes[1].tx.code(),"matching confirmation codes");
 select(0);uint8_t backup[SAVE_MAX_BYTES];ck(saveExport(backup,sizeof(backup))==0,"whole-save backup is blocked during a transaction");
 auto before=nodes[0].party.slots[0];nodes[0].party.releaseAt(0);ck(!memcmp(&before,&nodes[0].party.slots[0],72),"release cannot change reserved Pokemon");park();
 approve(0);pump();ck(nodes[0].party.slots[0].dex==25&&nodes[1].party.slots[0].dex==133,"one approval cannot commit");
 approve(1);drop=3;pump(60);ck(nodes[0].tx.j.phase==TX_DONE&&nodes[1].tx.j.phase==TX_DONE,"lossy swap reaches durable completion");
 ck(nodes[0].party.slots[0].dex==133&&nodes[1].party.slots[0].dex==25&&nodes[1].party.slots[0].ivAtk==31&&nodes[1].party.slots[0].shiny,"exact selected records exchanged without IV/shiny reroll");
 for(int i=0;i<2;i++)boot(i);pump();ck(nodes[0].party.slots[0].dex==133&&nodes[1].party.slots[0].dex==25,"restarts and repeated completion do not duplicate/reverse trade");
 init(TRADE_SEND,TRADE_RECEIVE,304);pump();approve(0);approve(1);pump(30);
 ck(nodes[0].party.box[299].empty()&&nodes[1].party.box[299].dex==25,"gift moves exactly one Pokemon into 300th box slot");
 init();pump();select(0);ck(nodes[0].tx.cancel(),"cancel is allowed before local confirmation");park();pump(20);
 ck(nodes[0].tx.j.phase==TX_ABORT&&nodes[1].tx.j.phase==TX_ABORT&&nodes[0].party.slots[0].dex==25&&nodes[1].party.slots[0].dex==133,"cancel keeps both originals");
 init();pump();approve(0);select(0);ck(!nodes[0].tx.cancel(),"confirmed transaction cannot be unilaterally cancelled");park();
 queue.clear();boot(0);boot(1);pump();approve(1);pump(25);ck(nodes[0].tx.terminal()&&nodes[1].tx.terminal(),"prepared trade resumes after both devices reboot");
 // Durable checkpoint failure must not send approval. Simulate process death
 // losing RAM writes, then reconnect with the last actual disk snapshot.
 init();pump();select(0);nodes[0].fail=true;ck(!nodes[0].tx.confirm()&&nodes[0].tx.failed,"disk flush failure blocks confirmation");park();pump(4);
 ck(nodes[1].party.slots[0].dex==133,"peer cannot commit an unpersisted approval");nodes[0].fail=false;boot(0);pump();approve(0);approve(1);pump(25);
 ck(nodes[0].tx.terminal()&&nodes[1].tx.terminal(),"failed checkpoint recovers from durable snapshot");
 // Crash at each durable phase, including after a roster write and before its
 // phase acknowledgement. The selected slot must be replayed in place.
 for(int phase=TX_READY;phase<=TX_APPLIED;phase++){
  init();pump();approve(0);approve(1);bool restarted=false;
  for(int step=0;step<80;step++){
   pump(1);if(!restarted&&nodes[0].tx.j.phase>=phase){boot(0);restarted=true;}
  }
  ck(restarted&&nodes[0].party.slots[0].dex==133&&nodes[1].party.slots[0].dex==25,"phase-boundary restart preserves exactly one of each individual");
 }
 for(int at=1;at<=5;at++){
  init();pump();approve(1);nodes[0].cutAt=at;nodes[0].cuts=0;bool cut=false;
  try{approve(0);pump(30);}catch(PowerCut&){cut=true;nodes[0].cutAt=0;boot(0);pump(35);}
  ck(cut&&nodes[0].tx.j.phase==TX_DONE&&nodes[1].tx.j.phase==TX_DONE&&nodes[0].party.slots[0].dex==133&&nodes[1].party.slots[0].dex==25,"power cut after each durable commit/receipt write resumes exactly once");
 }
 // Uncertain roster write: coordinator decision survives but neither a bad
 // slot nor a missing SD write may be advertised as APPLIED.
 init();pump();approve(0);approve(1);select(0);nvsFailKey()="rosterF";park();pump(10);
 ck(nodes[0].tx.failed||nodes[1].tx.failed,"roster storage fault blocks completion");nvsFailKey().clear();boot(0);boot(1);pump(35);
 ck(nodes[0].party.slots[0].dex==133&&nodes[1].party.slots[0].dex==25,"roster fault recovery retains both individuals");
 // Completed transaction is device-local metadata, not a Pokemon backup.
 select(0);tradeStorageBlocked=false;size_t len=saveExport(backup,sizeof(backup));
 ck(len>0,"whole-save backup is available after completion");
 bool journalFound=false;for(size_t i=0;i+9<len;i++)if(!memcmp(backup+i,"txJournal",9))journalFound=true;
 ck(!journalFound,"device transaction journal never travels in a whole-save backup");park();
 init(TRADE_SEND,TRADE_SEND);pump();ck(nodes[0].tx.incompatible&&nodes[1].tx.incompatible,"same send roles refused without mutations");
 PartyMon invalid=mon(25);invalid.ivAtk=32;ck(!Trade::validMon(invalid),"invalid IV rejected");invalid=mon(25);invalid.moves[0]=65535;ck(!Trade::validMon(invalid),"unknown move rejected");
 printf("%s: individual transfer transaction tests\n",bad?"FAIL":"PASS");return bad?1:0;
}
