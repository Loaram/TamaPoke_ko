#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

// Shared by the real ESP receiver and fault-injection tests. This protocol
// writes sprite files only; never allow PUT to address saves or directories.
inline bool sdUploadPathValid(const char *path) {
  size_t n = strlen(path);
  if(n < 11 || n > 63 || strncmp(path,"/mons/",6) || strcmp(path+n-4,".bin")) return false;
  for(size_t i=6;i<n;i++) {
    char c=path[i];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.')) return false;
  }
  return true;
}

inline uint32_t sdUploadCrc(uint32_t crc,const uint8_t *data,size_t n) {
  while(n--) {
    crc ^= *data++;
    for(unsigned b=0;b<8;b++) crc=(crc>>1)^((crc&1)?0xedb88320UL:0);
  }
  return crc;
}

template<class Card,class Port>
bool sdReceiveSprite(Card &card,Port &serial,bool ready,const char *path,uint32_t size) {
  if(!ready) {serial.println("ERR SD_NOT_READY");return false;}
  if(!sdUploadPathValid(path)) {serial.println("ERR INVALID_PATH");return false;}
  if(!size || size>4UL*1024*1024) {serial.println("ERR INVALID_SIZE");return false;}
  errno=0;
  auto f=card.open(path,"w");
  if(!f) {serial.printf("ERR OPEN_WRITE errno=%d\n",errno);return false;}
  serial.println("OK");
  static uint8_t buf[2048];
  uint32_t received=0,crc=0xffffffffUL;
  serial.setTimeout(10000);
  while(received<size) {
    size_t want=size-received>sizeof(buf)?sizeof(buf):size-received;
    size_t n=serial.readBytes(buf,want);
    // A partial timeout must not ACK a whole host chunk: that shifts every
    // subsequent chunk and used to produce misleading progress or truncation.
    if(n!=want) {
      f.close();serial.setTimeout(1000);
      serial.printf("ERR RX_TIMEOUT offset=%lu expected=%u received=%u\n",
        (unsigned long)received,(unsigned)want,(unsigned)n);
      return false;
    }
    errno=0;
    size_t written=f.write(buf,n);
    if(written!=n) {
      int error=errno;
      f.close();serial.setTimeout(1000);
      serial.printf("ERR WRITE_SHORT offset=%lu expected=%u written=%u errno=%d\n",
        (unsigned long)received,(unsigned)n,(unsigned)written,error);
      return false;
    }
    crc=sdUploadCrc(crc,buf,n);received+=n;
    serial.println("#"); // only ACK a completely received AND written chunk
  }
  f.flush();f.close();serial.setTimeout(1000);
  errno=0;
  auto check=card.open(path,"r");
  if(!check) {serial.printf("ERR VERIFY_OPEN errno=%d\n",errno);return false;}
  size_t actual=check.size();
  if(actual!=size) {
    check.close();serial.printf("ERR VERIFY_SIZE expected=%lu actual=%lu\n",
      (unsigned long)size,(unsigned long)actual);return false;
  }
  uint32_t verified=0,storedCrc=0xffffffffUL;
  while(verified<size) {
    size_t want=size-verified>sizeof(buf)?sizeof(buf):size-verified;
    size_t n=check.read(buf,want);
    if(n!=want) {
      check.close();serial.printf("ERR VERIFY_READ offset=%lu expected=%u received=%u\n",
        (unsigned long)verified,(unsigned)want,(unsigned)n);return false;
    }
    storedCrc=sdUploadCrc(storedCrc,buf,n);verified+=n;
  }
  check.close();
  if(storedCrc!=crc) {serial.println("ERR VERIFY_CRC");return false;}
  serial.println("DONE");
  return true;
}
