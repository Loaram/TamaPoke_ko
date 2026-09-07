// Same-Wi-Fi UDP transport for Android LAN battles. The game protocol remains
// in link.cpp; this file only discovers one peer and carries its small frames.
#include "linknow.h"
#include "linkudp.h"

#ifdef TAMAPOKE_UDP_TEST
#include "tests/udp_platform_stub.h"
#else
#include <android/log.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#endif
#include <stdio.h>

#define LOG_TAG "TamaPoke-LAN"

bool androidEnsureLocalNetworkPermission();
bool androidHasLocalNetworkPermission();
bool androidBeginLanNetwork();
void androidEndLanNetwork();
int androidLanState();
int androidLanEpoch();
uint32_t androidLanIpv4();
uint32_t androidLanBroadcast();

namespace {
int gSocket = -1;
Link *gLink = nullptr;
bool gLocked = false;
bool gWaitingForPermission = false;
uint64_t gPermissionPollAt = 0;
bool gNetworkRequested = false;
int gNetworkEpoch = -1;
uint32_t gIpv4 = 0, gBroadcast = 0;
int gLastError = 0;
const char *gStatus = "";
char gAddress[32]{};
sockaddr_in gPeer{};
uint32_t gNodeId = 0;
LinkNowStats gStats{};

uint32_t makeNodeId() {
  uint32_t id = 0;
  int randomFd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (randomFd >= 0) {
    ssize_t got = read(randomFd, &id, sizeof(id));
    close(randomFd);
    if (got != (ssize_t)sizeof(id)) id = 0;
  }
  if (!id) {
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    id = (uint32_t)now.tv_nsec ^ (uint32_t)now.tv_sec ^
         ((uint32_t)getpid() << 16) ^ (uint32_t)(uintptr_t)&gSocket;
  }
  return id ? id : 1;
}

uint64_t monotonicMillis() {
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}

bool samePeer(const sockaddr_in &a, const sockaddr_in &b) {
  return a.sin_family == b.sin_family && a.sin_port == b.sin_port &&
         a.sin_addr.s_addr == b.sin_addr.s_addr;
}

bool sendDatagram(const sockaddr_in &to, const uint8_t *frame, uint8_t len) {
  uint8_t packet[LINK_UDP_MAX_PACKET];
  size_t packetLen = linkUdpEncode(packet, sizeof(packet), gNodeId, frame, len);
  if (!packetLen) return false;
  ssize_t result = sendto(gSocket, packet, packetLen, 0,
                         reinterpret_cast<const sockaddr *>(&to), sizeof(to));
  if (result != (ssize_t)packetLen) {
    gLastError = errno;
    __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "sendto failed: %d", errno);
    return false;
  }
  return true;
}

bool openLanSocket() {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) { gLastError = errno; return false; }
  int yes = 1;
  // Do not share the receive port with another process: that silently splits
  // unicast packets. Surface a real bind error instead.
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) != 0) {
    gLastError = errno;
    close(sock);
    return false;
  }
  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_port = htons(LINK_UDP_PORT);
  local.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sock, reinterpret_cast<sockaddr *>(&local), sizeof(local)) != 0 ||
      fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK) != 0) {
    gLastError = errno;
    close(sock);
    return false;
  }
  gSocket = sock;
  gNetworkEpoch = androidLanEpoch();
  gIpv4 = androidLanIpv4();
  gBroadcast = androidLanBroadcast();
  in_addr address{};
  address.s_addr = htonl(gIpv4);
  inet_ntop(AF_INET, &address, gAddress, sizeof(gAddress));
  gStatus = "Wi-Fi 연결됨 / 상대 검색 중";
  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "UDP ready on port %u, id %04X",
                      LINK_UDP_PORT, gLink ? gLink->id : 0);
  return true;
}

void udpSend(void *, const uint8_t *frame, uint8_t len) {
  if (gSocket < 0 || !frame || len < 2 || len > 2 + LINK_MAX_PAYLOAD) return;
  bool sent = false;
  if (gLocked) {
    sent = sendDatagram(gPeer, frame, len);
  } else {
    // LinkProperties comes from the selected Wi-Fi Network; unlike getifaddrs
    // it does not depend on app access to the system's routing/netlink tables.
    if (gBroadcast) {
      sockaddr_in to{};
      to.sin_family = AF_INET;
      to.sin_port = htons(LINK_UDP_PORT);
      to.sin_addr.s_addr = htonl(gBroadcast);
      sent = sendDatagram(to, frame, len);
    }
    // Android may keep mobile data as its default route when the TamaPoke AP
    // has no Internet. Send through every active Wi-Fi/LAN interface instead
    // of relying only on 255.255.255.255 and the default route.
    ifaddrs *interfaces = nullptr;
    if (getifaddrs(&interfaces) == 0) {
      for (ifaddrs *it = interfaces; it; it = it->ifa_next) {
        if (!it->ifa_addr || !it->ifa_broadaddr ||
            it->ifa_addr->sa_family != AF_INET ||
            !(it->ifa_flags & IFF_UP) || !(it->ifa_flags & IFF_BROADCAST) ||
            (it->ifa_flags & IFF_LOOPBACK)) continue;
        sockaddr_in to = *reinterpret_cast<sockaddr_in *>(it->ifa_broadaddr);
        to.sin_port = htons(LINK_UDP_PORT);
        sent = sendDatagram(to, frame, len) || sent;
      }
      freeifaddrs(interfaces);
    }
    sockaddr_in fallback{};
    fallback.sin_family = AF_INET;
    fallback.sin_port = htons(LINK_UDP_PORT);
    fallback.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    sent = sendDatagram(fallback, frame, len) || sent;
  }
  if (sent) gStats.tx++;
  else gStats.txFail++;
}
}  // namespace

bool linkNowBegin(Link *link) {
  if (!link) return false;
  // Recreate even on retry: an old socket retains its old network binding and
  // may contain packets (including BYE) from the previous session.
  linkNowEnd();
  gLastError = 0;
  gAddress[0] = 0;
  gLink = link;
  gLocked = false;
  gNodeId = makeNodeId();
  gStats = LinkNowStats();
  link->id = (uint16_t)(gNodeId ^ (gNodeId >> 16));
  if (!link->id) link->id = 1;
  link->send = udpSend;
  link->ctx = nullptr;
  if (!androidEnsureLocalNetworkPermission()) {
    gStatus = "근거리 네트워크 권한을 허용하세요";
    gWaitingForPermission = true;
    gPermissionPollAt = 0;
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "Waiting for local network permission");
    return true;
  }
  gNetworkRequested = true;
  gStatus = "Wi-Fi 연결 준비 중";
  androidBeginLanNetwork();
  // Open only after the requested Wi-Fi route and its IPv4 address are ready.
  return true;
}

void linkNowEnd() {
  if (gSocket >= 0) close(gSocket);
  gSocket = -1;
  gLink = nullptr;
  gLocked = false;
  gWaitingForPermission = false;
  gPermissionPollAt = 0;
  if (gNetworkRequested) androidEndLanNetwork();
  gNetworkRequested = false;
  gNetworkEpoch = -1;
  gIpv4 = gBroadcast = 0;
}

bool linkNowUp() { return gSocket >= 0 || gWaitingForPermission || gNetworkRequested; }
const char *linkNowNetworkName() { return ""; }
const char *linkNowNetworkPassword() { return ""; }

void linkNowPoll() {
  if (!gLink) return;
  if (gWaitingForPermission) {
    uint64_t now = monotonicMillis();
    if (now - gPermissionPollAt < 250) return;
    gPermissionPollAt = now;
    if (!androidHasLocalNetworkPermission()) return;
    gWaitingForPermission = false;
    gNetworkRequested = true;
    gStatus = "Wi-Fi 연결 준비 중";
    androidBeginLanNetwork();
  }
  int networkState = androidLanState();
  if (networkState <= 0 || (gSocket >= 0 &&
      (networkState != 2 || androidLanEpoch() != gNetworkEpoch))) {
    gStatus = networkState == -2 ? "Wi-Fi 권한 또는 연결 오류" : "Wi-Fi 연결을 확인하고 다시 시도하세요";
    gLink->state = LINK_LOST;
    linkNowEnd();
    return;
  }
  if (gSocket < 0) {
    if (networkState != 2) return;
    if (!openLanSocket()) {
      gStatus = "통신 소켓을 열지 못했습니다";
      gLink->state = LINK_LOST;
      linkNowEnd();
      return;
    }
  }
  // Bound work per frame, even on a noisy LAN, so receive traffic cannot starve
  // protocol timers, UI, cancellation or the game's saving loop.
  for (int received = 0; received < 64; ++received) {
    uint8_t packet[LINK_UDP_MAX_PACKET];
    sockaddr_in from{};
    socklen_t fromLen = sizeof(from);
    ssize_t size = recvfrom(gSocket, packet, sizeof(packet), 0,
                            reinterpret_cast<sockaddr *>(&from), &fromLen);
    if (size < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "recvfrom failed: %d", errno);
      break;
    }
    uint32_t sender = 0;
    const uint8_t *frame = nullptr;
    uint8_t frameLen = 0;
    if (!linkUdpDecode(packet, (size_t)size, &sender, &frame, &frameLen)) continue;
    if (!sender || sender == gNodeId) continue;
    if (!gLocked) {
      gPeer = from;
      gLocked = true;
      gStatus = "상대 연결됨";
      char ip[INET_ADDRSTRLEN]{};
      inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
      __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Peer locked: %s:%u", ip,
                          (unsigned)ntohs(from.sin_port));
    } else if (!samePeer(gPeer, from)) {
      gStats.foreign++;
      continue;
    }
    gStats.rx++;
    gLink->onPacket(frame, frameLen);
  }
}

const LinkNowStats &linkNowStats() { return gStats; }
const char *androidLanStatus() { return gStatus; }
const char *androidLanAddress() { return gAddress; }
int androidLanLastError() { return gLastError; }
