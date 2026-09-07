#pragma once
// Fake OS boundary for executing the REAL Android UDP transport on a desktop.
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <vector>
#include <deque>
using ssize_t = intptr_t;
using socklen_t = unsigned;
struct in_addr { uint32_t s_addr; };
struct sockaddr { unsigned short sa_family; char data[14]; };
struct sockaddr_in { unsigned short sin_family, sin_port; in_addr sin_addr; char pad[8]; };
struct ifaddrs { ifaddrs *ifa_next; sockaddr *ifa_addr, *ifa_broadaddr; unsigned ifa_flags; };
#define AF_INET 2
#define SOCK_DGRAM 2
#define SOL_SOCKET 1
#define SO_BROADCAST 6
#define INADDR_ANY 0u
#define INADDR_BROADCAST 0xffffffffu
#define INET_ADDRSTRLEN 16
#define IFF_UP 1
#define IFF_BROADCAST 2
#define IFF_LOOPBACK 8
#define O_RDONLY 0
#define O_CLOEXEC 0
#define O_NONBLOCK 2048
#define F_SETFL 4
#define F_GETFL 3
#define CLOCK_MONOTONIC 1
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_WARN 5
namespace UdpTest {
struct Datagram { sockaddr_in address; std::vector<uint8_t> data; };
inline std::vector<Datagram> sent;
inline std::deque<Datagram> incoming;
inline int opens=0, closes=0, requests=0, releases=0, network=1, epoch=1;
inline bool permission=true, failBind=false, failSend=false;
inline uint64_t now=1000;
inline uint32_t ip=0xc0a8010a, broadcast=0xc0a801ff, node=0x12345678;
inline uint16_t swap16(uint16_t n) { return (n<<8)|(n>>8); }
inline uint32_t swap32(uint32_t n) { return (n<<24)|((n<<8)&0xff0000)|((n>>8)&0xff00)|(n>>24); }
inline int sock(int,int,int) { ++opens; return 20+opens; }
inline int option(int,int,int,const void*,socklen_t) { return 0; }
inline int bindSocket(int,const sockaddr*,socklen_t) { if(failBind){errno=EADDRINUSE;return -1;}return 0; }
inline int control(int,int,...) { return 0; }
inline int closeFd(int fd) { if(fd!=99)++closes;return 0; }
inline int openRandom(const char*,int) { return 99; }
inline ssize_t readRandom(int,void *p,size_t n) { ++node;memcpy(p,&node,n);return n; }
inline int processId() { return 123; }
inline int clockTime(int,timespec *ts) { ts->tv_sec=now/1000;ts->tv_nsec=(now%1000)*1000000;return 0; }
inline int interfaces(ifaddrs**) { return -1; } // Android can deny netlink access
inline void freeInterfaces(ifaddrs*) {}
inline const char *formatIp(int,const void *raw,char *out,socklen_t n) {
  uint32_t ip=swap32(((const in_addr*)raw)->s_addr);
  snprintf(out,n,"%u.%u.%u.%u",ip>>24,(ip>>16)&255,(ip>>8)&255,ip&255);return out;
}
inline ssize_t send(int,const void *data,size_t n,int,const sockaddr *to,socklen_t) {
  if(failSend){errno=ENETUNREACH;return -1;}
  sent.push_back({*(const sockaddr_in*)to,std::vector<uint8_t>((const uint8_t*)data,(const uint8_t*)data+n)});return n;
}
inline ssize_t receive(int,void *data,size_t n,int,sockaddr *from,socklen_t*) {
  if(incoming.empty()){errno=EAGAIN;return -1;}
  auto packet=incoming.front();incoming.pop_front();
  size_t size=packet.data.size()<n?packet.data.size():n;
  memcpy(data,packet.data.data(),size);memcpy(from,&packet.address,sizeof(sockaddr_in));return size;
}
inline int log(int,const char*,const char*,...) { return 0; }
}
#define socket UdpTest::sock
#define setsockopt UdpTest::option
#define bind UdpTest::bindSocket
#define fcntl UdpTest::control
#define close UdpTest::closeFd
#define open UdpTest::openRandom
#define read UdpTest::readRandom
#define getpid UdpTest::processId
#define clock_gettime UdpTest::clockTime
#define getifaddrs UdpTest::interfaces
#define freeifaddrs UdpTest::freeInterfaces
#define inet_ntop UdpTest::formatIp
#define sendto UdpTest::send
#define recvfrom UdpTest::receive
#define htons UdpTest::swap16
#define ntohs UdpTest::swap16
#define htonl UdpTest::swap32
#define __android_log_print UdpTest::log
