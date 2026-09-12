#include "sd_upload.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

struct Card;
struct File {
  Card *card=nullptr;bool reading=false;size_t pos=0;
  operator bool()const{return card;}
  size_t write(const uint8_t*,size_t);size_t read(uint8_t*,size_t);size_t size();
  void flush(){}void close(){card=nullptr;}
};
struct Card {
  std::vector<uint8_t> data;
  bool failOpen=false,failReopen=false,shortWrite=false,shortRead=false,truncate=false,corrupt=false;
  unsigned opens=0;
  File open(const char*,const char *mode) {
    opens++;
    if(mode[0]=='w') {if(failOpen)return {};data.clear();return {this,false,0};}
    if(failReopen)return {};
    if(truncate&&!data.empty())data.pop_back();
    if(corrupt&&!data.empty())data[data.size()/2]^=1;
    return {this,true,0};
  }
};
size_t File::write(const uint8_t *b,size_t n){if(card->shortWrite)n/=2;card->data.insert(card->data.end(),b,b+n);return n;}
size_t File::read(uint8_t *b,size_t n){n=std::min(n,card->data.size()-pos);if(card->shortRead)n/=2;memcpy(b,card->data.data()+pos,n);pos+=n;return n;}
size_t File::size(){return card->data.size();}
struct Port {
  std::vector<uint8_t> input;size_t pos=0;int cut=-1;std::string output;unsigned timeout=1000;
  void println(const char*s){output+=s;output+='\n';}
  void printf(const char *fmt,...) {char b[512];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);output+=b;}
  void setTimeout(unsigned t){timeout=t;}
  size_t readBytes(uint8_t *b,size_t n){n=std::min(n,input.size()-pos);if(cut>=0)n=std::min(n,(size_t)cut);memcpy(b,input.data()+pos,n);pos+=n;return n;}
};
static int bad=0,checks=0;
void ck(bool ok,const char*s){checks++;printf("%s %s\n",ok?"PASS":"FAIL",s);bad+=!ok;}
Port source(size_t n=5000){Port p;p.input.resize(n);for(size_t i=0;i<n;i++)p.input[i]=(i*37+11)%256;return p;}
bool receive(Card &c,Port &p){return sdReceiveSprite(c,p,true,"/mons/p047.bin",(uint32_t)p.input.size());}
int main(){
  const uint8_t known[]="123456789";
  ck((sdUploadCrc(0xffffffff,known,9)^0xffffffff)==0xcbf43926,"CRC32 matches standard check vector");
  for(size_t n:{1,952,2048,3000,4096,5000}) {
    Card c;auto p=source(n);bool ok=receive(c,p);
    ck(ok&&c.data==p.input&&p.output.substr(p.output.size()-5)=="DONE\n"&&p.timeout==1000,
       "valid full/final partial-size chunks reopen and verify before DONE");
  }
  for(int kind=0;kind<6;kind++) {
    Card c;auto p=source();const char*code="";
    switch(kind){case 0:c.failOpen=true;code="OPEN_WRITE";break;
      case 1:c.shortWrite=true;code="WRITE_SHORT";break;
      case 2:c.failReopen=true;code="VERIFY_OPEN";break;
      case 3:c.truncate=true;code="VERIFY_SIZE";break;
      case 4:c.shortRead=true;code="VERIFY_READ";break;
      case 5:c.corrupt=true;code="VERIFY_CRC";break;}
    ck(!receive(c,p)&&p.output.find(std::string("ERR ")+code)!=std::string::npos&&p.output.find("DONE")==std::string::npos&&p.timeout==1000,code);
    if(kind<2)ck(p.output.find('#')==std::string::npos,"failed open/write never acknowledges a block");
    if(kind==1)ck(p.output.find("offset=0 expected=2048 written=1024")!=std::string::npos,"short write reports actual requested and written bytes");
  }
  for(int cut:{0,37,2047}) {
    Card c;auto p=source();p.cut=cut;
    ck(!receive(c,p)&&p.output.find("ERR RX_TIMEOUT offset=0 expected=2048 received="+std::to_string(cut))!=std::string::npos&&p.output.find('#')==std::string::npos&&c.data.empty(),
       "missing/partial USB chunk aborts without ACK or false success");
  }
  for(const char*path:{"/save/roster.bin","/mons/../save.bin","/mons/sub/p001.bin","/mons/.bin","/mons/a.txt"}) {
    Card c;auto p=source();ck(!sdReceiveSprite(c,p,true,path,5000)&&!c.opens&&p.output=="ERR INVALID_PATH\n","PUT cannot address saves, directories or invalid sprite names");
  }
  for(uint32_t n:{0u,4194305u}) {Card c;auto p=source();ck(!sdReceiveSprite(c,p,true,"/mons/p001.bin",n)&&!c.opens&&p.output=="ERR INVALID_SIZE\n","invalid size rejected before opening SD file");}
  {Card c;auto p=source();ck(!sdReceiveSprite(c,p,false,"/mons/p001.bin",5000)&&!c.opens&&p.output=="ERR SD_NOT_READY\n","missing SD is distinguished from file open and transport errors");}
  for(int failAt:{47,50}) {
    int completed=0;bool stopped=false;
    for(int i=1;i<=303;i++) {Card c;auto p=source(3000);c.failOpen=i==failAt;
      if(!receive(c,p)){stopped=i==failAt&&p.output=="ERR OPEN_WRITE errno=0\n";break;}completed++;}
    ck(stopped&&completed==failAt-1,"intermittent open failure at 47 or 50 stops precisely at that file");
  }
  {int completed=0;for(int i=0;i<344;i++){Card c;auto p=source(3000);if(receive(c,p))completed++;}
    ck(completed==344,"344 successive files complete with independent CRC state");}
  printf("%d checks, %d failures\n",checks,bad);return bad?1:0;
}
