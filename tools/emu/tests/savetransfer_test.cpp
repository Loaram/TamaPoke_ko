// Whole-save transfer over the shared Link protocol. This covers the part that
// Android, Wear OS and ESP32 all compile from the same source: discovery,
// chunking, retries, integrity checks and the explicit apply boundary.
#include "Arduino.h"
#include "Preferences.h"
#include "link.h"
#include "save.h"
#include <cstdio>
#include <cstring>
#include <vector>

uint32_t g_seed=23; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); fflush(stdout); if(!ok)bad++;}

struct Pipe { Link *peer; unsigned sent=0; unsigned dropEvery=0; bool dropFirst=false; };
static void pipeSend(void *v,const uint8_t *b,uint8_t n){
  Pipe *p=(Pipe*)v;
  p->sent++;
  if(p->dropFirst && p->sent==1) return;
  if(p->dropEvery && p->sent%p->dropEvery==0) return;
  p->peer->onPacket(b,n);
}

static size_t makeSave(uint8_t *out,size_t cap){
  nvs().clear();
  Preferences p; p.begin("tamapoke",false);
  p.putString("tnam","SENDER"); p.putUChar("lang",6); p.putUInt("age",123456);
  uint8_t box[sizeof(PartyMon) * BOX_SLOTS],party[480];
  for(size_t i=0;i<sizeof(box);i++) box[i]=(uint8_t)(i*17u+3u);
  for(size_t i=0;i<sizeof(party);i++) party[i]=(uint8_t)(i*29u+7u);
  p.putBytes("box",box,sizeof(box)); p.putBytes("party",party,sizeof(party)); p.end();
  return saveExport(out,cap);
}

static void runTransfer(bool lossy,bool receiverStarts=false){
  uint8_t original[SAVE_MAX_BYTES];
  size_t n=makeSave(original,sizeof(original));
  ck(n>LINK_SAVE_CHUNK*4,"the representative save spans many chunks");

  // This is the receiver's current save. Merely receiving must not touch it.
  nvs().clear(); Preferences cur; cur.begin("tamapoke",false);
  cur.putString("tnam","RECEIVER"); cur.end();

  Link send,recv;
  Pipe a{&recv,0,lossy?3u:0u,receiverStarts};
  Pipe b{&send,0,lossy?4u:0u,false};
  send.send=pipeSend; send.ctx=&a; recv.send=pipeSend; recv.ctx=&b;
  ck(send.beginSave(true,"SENDER",original,(uint16_t)n),"sender accepts a valid save");
  ck(recv.beginSave(false,"RECEIVER"),"receiver enters save mode");
  send.id=0x1111; recv.id=0x8888;
  if(receiverStarts) recv.start(); else send.start();
  for(uint32_t now=0; now<180000 &&
      (send.state!=LINK_SAVE_DONE || recv.state!=LINK_SAVE_READY); now+=100){
    send.tick(now); recv.tick(now);
  }
  if(send.state!=LINK_SAVE_DONE || recv.state!=LINK_SAVE_READY)
    printf("      transfer stopped: send=%u recv=%u chunks=%u/%u frames=%u/%u\n",
           send.state,recv.state,recv.saveChunk,recv.saveChunks,a.sent,b.sent);
  ck(send.state==LINK_SAVE_DONE,"sender finishes after final receipt");
  ck(recv.state==LINK_SAVE_READY,"receiver validates the complete save");
  ck(recv.saveProgress()==100 && send.saveProgress()==100,"both report 100 percent");
  ck(recv.savePeerSize==n && !memcmp(recv.saveData,original,n),"every byte arrives in order");
  ck(send.saveCode==recv.saveCode && send.saveCode<1000000,"both show the same six-digit code");
  char before[32]={0}; Preferences q; q.begin("tamapoke",true);
  q.getString("tnam",before,sizeof(before)); q.end();
  ck(!strcmp(before,"RECEIVER"),"receiving alone does not overwrite the current save");
  ck(saveImport(recv.saveData,recv.savePeerSize),"the confirmed save can be applied");
  char after[32]={0}; q.begin("tamapoke",true); q.getString("tnam",after,sizeof(after)); q.end();
  ck(!strcmp(after,"SENDER"),"confirmation replaces it with the sender's save");
}

int main(){
  runTransfer(false);
  runTransfer(true);
  runTransfer(false,true);  // receiver opens first and the sender's first hello is lost

  uint8_t blob[SAVE_MAX_BYTES]; size_t n=makeSave(blob,sizeof(blob));
  blob[SAVE_HDR+5]^=0x80;
  Link badSend;
  ck(!badSend.beginSave(true,"BAD",blob,(uint16_t)n) &&
     badSend.state==LINK_SAVE_INVALID,"a corrupt source is rejected before radio use");

  Link a,b; Pipe pa{&b},pb{&a}; a.send=pipeSend;a.ctx=&pa;b.send=pipeSend;b.ctx=&pb;
  n=makeSave(blob,sizeof(blob));
  a.beginSave(true,"A",blob,(uint16_t)n); b.beginSave(true,"B",blob,(uint16_t)n);
  a.id=1;b.id=2;a.start();
  ck(a.state==LINK_REFUSED || b.state==LINK_REFUSED,"two senders refuse instead of overwriting either side");

  printf("%s\n",bad?"FAILURES":"all good");
  return bad?1:0;
}
