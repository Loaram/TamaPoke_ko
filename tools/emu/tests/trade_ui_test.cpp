#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "trade.h"
#include "korean_text.h"
#include <cstdio>
uint32_t g_seed=351;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}String FakeSerial::readStringUntil(char){return String("");}
void setup();void render();void onTap(int16_t,int16_t);void onSwipe(int);void onSwipeV(int);
extern Pet pet;extern KoreanCanvas*gfx;extern bool lanOpen,tradeOpen;extern Link lan;
extern uint8_t tradeMenu,tradeDetail;extern uint16_t tradePage;
int bad=0;void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);bad+=!b;}
void shot(const char*name){render();FILE*f=fopen(name,"wb");fprintf(f,"P6\n466 466\n255\n");for(int i=0;i<466*466;i++){auto c=gfx->buffer()[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);}
PartyMon mon(int dex){PartyMon m;m.dex=dex;m.level=80;m.ivAtk=31;m.moves[0]=1;return m;}
int main(){
 nvs().clear();setup();pet.dbgHatchAs(6,false);while(pet.hasLearnOffer())pet.declineLearn();
 for(int i=0;i<5;i++)party.slots[i]=mon(25+i);party.box[299]=mon(133);party.save();auto quota=pet.farewellsRemaining();auto streak=pet.streak;
 lanOpen=true;lan.state=LINK_OFF;shot("trade-lan.ppm");onTap(233,355);ck(tradeOpen&&tradeMenu==0,"LAN menu opens individual transfer separately from whole-save");shot("trade-menu.ppm");
 onTap(233,270);ck(tradeMenu==1,"swap opens roster selection");shot("trade-picker.ppm");
 onTap(145,350);ck(tradePage==50,"previous button wraps to last page");onTap(100,259);
 ck(tradeMenu==2&&trade.j.slot==304&&trade.j.before.dex==133,"last box Pokemon selected without truncating index");
 trade.j.peer=55;trade.j.incoming=mon(25);strcpy(trade.j.peerName,"FRIEND");trade.j.phase=TX_MATCH;trade.online=true;
 shot("trade-preview.ppm");onTap(310,250);ck(tradeDetail==2,"receiver details shown before approval");shot("trade-detail.ppm");
 onSwipeV(1);ck(!tradeDetail&&trade.j.phase==TX_MATCH,"detail back does not approve");
 onTap(233,310);ck(trade.j.phase==TX_READY,"only explicit confirm persists approval");shot("trade-wait.ppm");
 onTap(233,405);onSwipe(1);ck(tradeOpen&&trade.j.phase==TX_READY,"confirmed trade cannot be closed by tap or swipe");
 ck(party.box[299].dex==133&&pet.speciesId==6&&pet.farewellsRemaining()==quota&&pet.streak==streak,"preview/approval leave active Pokemon and player rewards untouched");
 Link peerLink;peerLink.begin(false,"FRIEND");peerLink.send=[](void*,const uint8_t*b,uint8_t n){lan.onPacket(b,n);};
 Trade peer;peer.j.mine=trade.j.peer;peer.j.peer=trade.j.mine;peer.j.mode=TRADE_SWAP;peer.j.phase=TX_APPLIED;peer.j.before=trade.j.incoming;peer.attach(peerLink);peer.tick(millis()+500);
 ck(trade.j.phase==TX_DONE&&party.box[299].dex==25&&pet.isRegistered(25),"actual UI commit callback saves the received Pokemon and registers its Dex");
 ck(pet.speciesId==6&&pet.farewellsRemaining()==quota&&pet.streak==streak,"received Dex registration preserves active Pokemon, farewell quota and streak");shot("trade-done.ppm");
 Preferences p;p.begin("tamapoke",false);p.remove("txJournal");p.remove("txReceipts");trade.load(party);tradeOpen=false;lan.extension=nullptr;lan.state=LINK_OFF;
 for(auto&m:party.slots)m=mon(25);for(auto&m:party.box)m=mon(133);party.save();
 onTap(233,355);onTap(233,211);ck(tradeMenu==0&&!trade.pending(),"receiving with full party and box is refused before connection");
 return bad?1:0;
}
