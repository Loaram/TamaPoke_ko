#include "trade.h"
#include "dex.h"
#include "moves.h"
#include <stddef.h>
#include <string.h>

bool tradeStorageBlocked=false;
Trade trade;
static constexpr uint32_t MAGIC=0x31585454; // TTX1 device journal
static constexpr uint8_t MESSAGE=80, WIRE=1, PACKET=112;
static uint32_t hash(const void *v,size_t n){auto*p=(const uint8_t*)v;uint32_t h=2166136261u;while(n--){h^=*p++;h*=16777619u;}return h;}
static void wr64(uint8_t*p,uint64_t v){for(int i=0;i<8;i++)p[i]=v>>(i*8);}
static uint64_t rd64(const uint8_t*p){uint64_t v=0;for(int i=7;i>=0;i--)v=(v<<8)|p[i];return v;}
static bool equal(const PartyMon&a,const PartyMon&b){return !memcmp(&a,&b,sizeof(a));}
static bool terminalPhase(uint8_t p){return p==TX_DONE||p==TX_ABORT;}

bool Trade::validMon(const PartyMon&m,bool empty){
  if(m.empty())return empty && equal(m,PartyMon());
  if(m.dex>DEX_COUNT || m.level<1 || m.level>100 || m.shiny>1 ||
     m.ivAtk>31||m.ivDef>31||m.ivSpe>31||m.ivHp>31 || !memchr(m.nick,0,sizeof(m.nick)))return false;
  if(m.form && (!formFind(m.dex,m.form)||!formEligible(m.dex,m.form,m.level)))return false;
  for(auto id:m.moves)if(id>=MOVE_COUNT)return false;
  if(m.care[0]>1)return false;
  if(m.care[0]==1){
    const auto*c=m.care;uint32_t age=0;memcpy(&age,c+20,4);
    if((c[1]&~7)||c[2]>100||c[3]>100||c[4]>100||c[5]>100||c[9]>100||
       (age/20+1<100?age/20+1:100)!=m.level)return false;
  }
  return true;
}
PartyMon &Trade::slot(){return j.slot<PARTY_SLOTS?roster->slots[j.slot]:roster->box[j.slot-PARTY_SLOTS];}
bool Trade::persist(){
  j.magic=MAGIC;j.checksum=hash(&j,offsetof(TradeJournal,checksum));
  Preferences p;p.begin("tamapoke",false);p.putBytes("txJournal",&j,sizeof(j));
  TradeJournal check;bool ok=p.getBytes("txJournal",&check,sizeof(check))==sizeof(check)&&!memcmp(&check,&j,sizeof(j));p.end();
  if(ok && checkpoint)ok=checkpoint();
  if(!ok)failed=true;
  tradeStorageBlocked=pending();return ok;
}
bool Trade::persistReceipts(){
  receipts.checksum=hash(&receipts,offsetof(TradeReceipts,checksum));
  Preferences p;p.begin("tamapoke",false);p.putBytes("txReceipts",&receipts,sizeof(receipts));
  TradeReceipts check;bool ok=p.getBytes("txReceipts",&check,sizeof(check))==sizeof(check)&&!memcmp(&check,&receipts,sizeof(check));p.end();
  if(ok&&checkpoint)ok=checkpoint();if(!ok){failed=true;tradeStorageBlocked=true;}return ok;
}
bool Trade::load(Party&p){
  roster=&p;failed=incompatible=online=false;j=TradeJournal();receipts=TradeReceipts();
  Preferences s;s.begin("tamapoke",true);
  if(s.isKey("txJournal") && (s.getBytes("txJournal",&j,sizeof(j))!=sizeof(j)||j.magic!=MAGIC||
     j.checksum!=hash(&j,offsetof(TradeJournal,checksum))||j.phase>TX_ABORT||j.slot>=PARTY_SLOTS+BOX_SLOTS))failed=true;
  if(s.isKey("txReceipts")&&(s.getBytes("txReceipts",&receipts,sizeof(receipts))!=sizeof(receipts)||
     receipts.checksum!=hash(&receipts,offsetof(TradeReceipts,checksum))))failed=true;
  s.end();tradeStorageBlocked=pending();
  if(!failed && (j.phase==TX_COMMIT||j.phase==TX_APPLIED))apply();
  return !failed;
}
bool Trade::begin(uint8_t mode,uint16_t at){
  if(pending()||!roster||!roster->writable()||at>=PARTY_SLOTS+BOX_SLOTS||mode<1||mode>3)return false;
  // Never evict an unacknowledged completion: the old peer may be offline.
  bool room=false;for(auto&r:receipts.entries)if(!r.mine)room=true;if(!room)return false;
  TradeJournal old=j;j=TradeJournal();j.mode=mode;j.slot=at;j.before=slot();
  if(!validMon(j.before,mode==TRADE_RECEIVE)||(mode==TRADE_RECEIVE&&!j.before.empty())){j=old;return false;}
  for(int i=0;i<4;i++)j.mine=(j.mine<<16)|(uint16_t)random(65536L);
  if(!j.mine)j.mine=1;j.phase=TX_OFFER;incompatible=false;online=false;return persist();
}
void Trade::attach(Link&l){
  link=&l;l.extensionContext=this;l.extension=[](void*c,const uint8_t*b,uint8_t n){((Trade*)c)->packet(b,n);};
  lastTx=lastRx=0;online=false;
}
uint32_t Trade::code()const{uint8_t b[16];wr64(b,j.mine<j.peer?j.mine:j.peer);wr64(b+8,j.mine<j.peer?j.peer:j.mine);return hash(b,16)%1000000u;}
void Trade::emit(uint64_t mine,uint64_t peer,uint8_t phase,bool offer){
  if(!link||!link->send||failed)return;
  uint8_t b[2+PACKET]={MESSAGE,PACKET};auto*p=b+2;p[0]=WIRE;p[1]=phase;
  p[2]=offer?j.mode:0;uint16_t tag=linkBuildTag();p[3]=tag;p[4]=tag>>8;
  wr64(p+5,mine);wr64(p+13,peer);
  if(offer){memcpy(p+21,&j.before,72);memcpy(p+93,link->myName,12);}
  uint32_t crc=hash(p,108);memcpy(p+108,&crc,4);link->send(link->ctx,b,sizeof(b));
}
bool Trade::finish(uint8_t phase){
  // Durable receipt is saved BEFORE unlocking a slot. A later trade cannot
  // erase proof needed by a peer that missed the final packet.
  TradeReceipt *free=nullptr;
  for(auto&r:receipts.entries){if(r.mine==j.mine){free=&r;break;}if(!r.mine&&!free)free=&r;}
  if(!free){failed=true;tradeStorageBlocked=true;return false;}
  free->mine=j.mine;free->peer=j.peer;free->phase=phase;
  if(!persistReceipts())return false;j.phase=phase;return persist();
}
bool Trade::apply(){
  if(failed||!roster||j.slot>=PARTY_SLOTS+BOX_SLOTS)return false;
  // A crash after roster commit but before phase commit is replayed in place.
  if(!equal(slot(),j.incoming)){
    if(!equal(slot(),j.before)){failed=true;tradeStorageBlocked=true;return false;}
    auto old=slot();slot()=j.incoming;
    tradeStorageBlocked=false;bool ok=roster->save();tradeStorageBlocked=true;
    if(!ok){slot()=old;failed=true;return false;}
  }
  if(!j.incoming.empty()&&registered&&!registered(j.incoming)){failed=true;tradeStorageBlocked=true;return false;}
  j.phase=TX_APPLIED;return persist();
}
bool Trade::confirm(){if(failed||j.phase!=TX_MATCH||!online||incompatible)return false;j.phase=TX_READY;return persist();}
bool Trade::cancel(){if(failed||j.phase<TX_OFFER||j.phase>TX_MATCH)return false;return finish(TX_ABORT);}
void Trade::tick(uint32_t now){
  if(failed||!link)return;
  if(online&&(uint32_t)(now-lastRx)>12000)online=false;
  if((uint32_t)(now-lastTx)<400)return;lastTx=now;
  if(j.phase)emit(j.mine,j.peer,j.phase,true);
}
void Trade::packet(const uint8_t*b,uint8_t n){
  if(failed||n!=2+PACKET||b[0]!=MESSAGE||b[1]!=PACKET)return;
  auto*p=b+2;uint32_t crc=0;memcpy(&crc,p+108,4);
  if(crc!=hash(p,108))return;
  if(p[0]!=WIRE||(uint16_t)(p[3]|p[4]<<8)!=linkBuildTag()){incompatible=true;return;}
  uint64_t peer=rd64(p+5),target=rd64(p+13);uint8_t phase=p[1],mode=p[2];
  if(!peer||peer==j.mine||phase<TX_OFFER||phase>TX_ABORT)return;
  // Service old receipts even while a newer transaction is open.
  for(auto&r:receipts.entries)if(r.mine && target==r.mine && (!r.peer||r.peer==peer)){
    if(terminalPhase(phase)){
      // The peer is durably terminal; its acknowledgement cannot strand it.
      r=TradeReceipt();persistReceipts();
    }else emit(r.mine,peer,r.phase,false);
    return;
  }
  if(!j.phase||terminal())return;
  if(j.peer && (peer!=j.peer||target!=j.mine))return;
  if(!j.peer && target && target!=j.mine)return;
  if(phase==TX_ABORT){
    if(j.phase>=TX_COMMIT)return; // never undo a committed decision
    j.peer=peer;finish(TX_ABORT);return;
  }
  if(!j.peer){
    bool roles=(j.mode==TRADE_SWAP&&mode==TRADE_SWAP)||(j.mode==TRADE_SEND&&mode==TRADE_RECEIVE)||(j.mode==TRADE_RECEIVE&&mode==TRADE_SEND);
    PartyMon incoming;memcpy(&incoming,p+21,72);
    if(!roles||!validMon(incoming,j.mode==TRADE_SEND)||(j.mode==TRADE_SEND&&!incoming.empty())||
       (!incoming.empty()&&acceptMon&&!acceptMon(incoming))){incompatible=true;return;}
    j.peer=peer;j.incoming=incoming;memcpy(j.peerName,p+93,12);j.peerName[11]=0;j.phase=TX_MATCH;
    if(!persist())return;
  } else if(mode){
    PartyMon offered;memcpy(&offered,p+21,72);
    if(!equal(j.incoming,offered)||(mode!=TRADE_SWAP&&j.mode==TRADE_SWAP)){incompatible=true;return;}
  }
  online=true;lastRx=millis();
  bool host=j.mine<j.peer;
  if(j.phase==TX_READY && ((host&&phase>=TX_READY&&phase<=TX_DONE)||(!host&&phase>=TX_COMMIT&&phase<=TX_DONE))){
    j.phase=TX_COMMIT;if(!persist())return;
  }
  if(j.phase==TX_COMMIT&&!apply())return;
  if(j.phase==TX_APPLIED && (phase==TX_APPLIED||phase==TX_DONE))finish(TX_DONE);
}
