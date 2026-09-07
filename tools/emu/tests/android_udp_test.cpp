#include "Arduino.h"
#include "Preferences.h"
#include "linkudp.h"
#include "linknow.h"
#define TAMAPOKE_UDP_TEST
#include "../../android/link_udp.cpp"

uint32_t g_seed=19;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
bool wasPressed=false;
uint32_t millis(){return (uint32_t)UdpTest::now;}
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
bool androidEnsureLocalNetworkPermission(){return UdpTest::permission;}
bool androidHasLocalNetworkPermission(){return UdpTest::permission;}
bool androidBeginLanNetwork(){++UdpTest::requests;return true;}
void androidEndLanNetwork(){++UdpTest::releases;}
int androidLanState(){return UdpTest::network;}
int androidLanEpoch(){return UdpTest::epoch;}
uint32_t androidLanIpv4(){return UdpTest::ip;}
uint32_t androidLanBroadcast(){return UdpTest::broadcast;}
static int bad=0;
static void check(bool ok,const char *label){printf("%s %s\n",ok?"PASS":"FAIL",label);bad+=!ok;}
static void enqueue(uint32_t sender,const std::vector<uint8_t>&frame,uint32_t ip=0xc0a8010b){
  uint8_t packet[LINK_UDP_MAX_PACKET];
  size_t n=linkUdpEncode(packet,sizeof(packet),sender,frame.data(),(uint8_t)frame.size());
  sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(LINK_UDP_PORT);address.sin_addr.s_addr=htonl(ip);
  UdpTest::incoming.push_back({address,std::vector<uint8_t>(packet,packet+n)});
}
static void transfer(bool watchSends, bool loss) {
  using namespace UdpTest;
  linkNowEnd(); sent.clear(); incoming.clear();
  network=2; failBind=failSend=false;
  uint8_t blob[SAVE_MAX_BYTES];
  nvs().clear();Preferences p;p.begin("tamapoke",false);
  uint8_t box[sizeof(PartyMon)*BOX_SLOTS];
  for(size_t i=0;i<sizeof(box);++i)box[i]=(uint8_t)(i*17+3);
  p.putBytes("box",box,sizeof(box));p.putString("tnam","SOURCE");p.end();
  size_t n=saveExport(blob,sizeof(blob));
  check(n>20000,"native transport fixture includes 300 box records");
  Link watch,phone;
  watch.beginSave(watchSends,"WATCH",watchSends?blob:nullptr,watchSends?(uint16_t)n:0);
  phone.beginSave(!watchSends,"PHONE",watchSends?nullptr:blob,watchSends?0:(uint16_t)n);
  linkNowBegin(&watch);linkNowPoll();phone.id=watch.id+1;
  unsigned counter=0;
  phone.ctx=&counter;
  phone.send=[](void *ctx,const uint8_t *data,uint8_t count){
    unsigned &sent=*(unsigned*)ctx;
    ++sent;
    // Drop every fourth phone packet, exercising actual UDP retries.
    if(sent%4)enqueue(0x98765432,std::vector<uint8_t>(data,data+count));
  };
  watch.start();phone.start();
  Link &sender=watchSends?watch:phone,&receiver=watchSends?phone:watch;
  unsigned outgoing=0;
  for(unsigned t=0;t<180000&&(sender.state!=LINK_SAVE_DONE||receiver.state!=LINK_SAVE_READY);t+=100){
    auto packets=sent;sent.clear();
    for(const auto &packet:packets){
      if(loss&&++outgoing%3==0)continue;
      uint32_t id;const uint8_t *frame;uint8_t length;
      if(linkUdpDecode(packet.data.data(),packet.data.size(),&id,&frame,&length))phone.onPacket(frame,length);
    }
    linkNowPoll();watch.tick(t);phone.tick(t);
  }
  check(sender.state==LINK_SAVE_DONE&&receiver.state==LINK_SAVE_READY,"native UDP + old peer completes save handshake/chunks/receipt");
  check(receiver.savePeerSize==n&&!memcmp(receiver.saveData,blob,n),"native UDP preserves full save byte for byte");
  linkNowEnd();
}
int main(){
  using namespace UdpTest;
  Link l;l.begin(false,"WATCH");
  permission=false;
  linkNowBegin(&l);l.start();linkNowPoll();
  check(opens==0&&requests==0&&linkNowUp(),"permission wait does not open an unbound socket");
  permission=true;now+=300;linkNowPoll();
  check(requests==1&&opens==0,"Wi-Fi acquisition precedes socket creation");
  network=2;linkNowPoll();
  check(opens==1&&!strcmp(androidLanAddress(),"192.168.1.10"),"Wi-Fi IPv4 ready opens socket and exposes address");
  l.start();
  check(sent.size()==2&&sent[0].address.sin_addr.s_addr==htonl(broadcast),"directed Wi-Fi broadcast works even with getifaddrs denied");
  check(linkNowStats().tx==1,"send counter measures frames, not duplicate destinations");
  enqueue(node,{LM_ACT,2,0,1});linkNowPoll();
  check(linkNowStats().rx==0,"self broadcast ignored");
  Link peer;peer.begin(true,"PHONE");peer.id=l.id+1;
  std::vector<uint8_t> hello;
  peer.ctx=&hello;peer.send=[](void *ctx,const uint8_t *p,uint8_t n){((std::vector<uint8_t>*)ctx)->assign(p,p+n);};peer.start();
  enqueue(0x98765432,hello);linkNowPoll();
  check(linkNowStats().rx==1&&l.peerId==peer.id,"real protocol handshake delivered through native UDP decoder");
  sent.clear();l.start();
  check(sent.size()==1&&sent[0].address.sin_addr.s_addr==htonl(0xc0a8010b),"paired packets use unicast");
  enqueue(0x45678912,hello,0xc0a8010c);linkNowPoll();
  check(linkNowStats().foreign==1&&linkNowStats().rx==1,"third peer cannot replace locked connection");
  ++epoch;linkNowPoll();
  check(l.state==LINK_LOST&&!linkNowUp()&&releases==1,"network replacement closes stale socket and releases Wi-Fi");
  l.begin(false,"WATCH");linkNowBegin(&l);linkNowPoll();
  check(opens==2&&linkNowStats().rx==0,"retry recreates socket and clears counters");
  failSend=true;l.start();check(linkNowStats().txFail==1&&androidLanLastError()==ENETUNREACH,"send errors observable");failSend=false;
  for(int i=0;i<100;i++)enqueue(0x98765432,hello);
  linkNowPoll();check(incoming.size()==36,"receive flood limited to 64 frames per game tick");incoming.clear();
  linkNowEnd();check(!linkNowUp()&&opens==closes,"leaving releases all sockets");
  l.begin(false,"WATCH");network=-1;linkNowBegin(&l);linkNowPoll();
  check(l.state==LINK_LOST&&!linkNowUp(),"unavailable Wi-Fi fails explicitly without cellular fallback");
  l.begin(false,"WATCH");network=2;failBind=true;linkNowBegin(&l);linkNowPoll();
  check(l.state==LINK_LOST&&androidLanLastError()==EADDRINUSE&&opens==closes,"socket bind failure cleans up and preserves error");
  failBind=false;network=2;l.begin(false,"WATCH");linkNowBegin(&l);linkNowPoll();
  network=0;linkNowPoll();
  check(l.state==LINK_LOST&&!linkNowUp(),"background Wi-Fi release terminates stale session on resume");
  transfer(false,false);transfer(true,false);transfer(false,true);transfer(true,true);
  printf("%s\n",bad?"FAILURES":"all good");return bad?1:0;
}
