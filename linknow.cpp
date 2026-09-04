#include "linknow.h"
#include "linkudp.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

// Hybrid LAN transport. Two TamaPoke boards keep using ESP-NOW exactly as
// before. At the same time each board exposes a small WPA2 access point and a
// UDP endpoint so an Android build can join and carry the same Link packets.

static Link *gLink = nullptr;
static bool gUp = false;
static bool gEspNowUp = false;
static bool gUdpUp = false;
static bool gApUp = false;
enum Transport : uint8_t { TR_NONE, TR_ESPNOW, TR_UDP };
static volatile Transport gTransport = TR_NONE;

static bool gPeerAdded = false;
static uint8_t gPeer[6];
static LinkNowStats gStats;
static const uint8_t BROADCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static WiFiUDP gUdp;
static IPAddress gUdpPeer;
static uint16_t gUdpPeerPort = LINK_UDP_PORT;
static uint32_t gUdpPeerNode = 0;
static uint32_t gNodeId = 0;
static char gApName[24] = "";
static const char AP_PASSWORD[] = "tamapoke";

// ESP-NOW receives on the WiFi task, so it parks frames for the main loop.
#define RING_SLOTS 8
#define RING_SLOT_BYTES (2 + LINK_MAX_PAYLOAD)
struct RingSlot { uint8_t len; uint8_t buf[RING_SLOT_BYTES]; };
static RingSlot gRing[RING_SLOTS];
static volatile uint8_t gHead = 0, gTail = 0;

static bool sameMac(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  (void)info;
  if (!gEspNowUp) return;
  if (status == ESP_NOW_SEND_SUCCESS) gStats.tx++;
  else gStats.txFail++;
}

static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (!gUp || !gEspNowUp || gTransport == TR_UDP || len < 2 ||
      (size_t)len > RING_SLOT_BYTES) return;
  if (gTransport == TR_ESPNOW && info && info->src_addr &&
      !sameMac(info->src_addr, gPeer)) {
    gStats.foreign++;
    return;
  }
  uint8_t head = gHead, next = (uint8_t)((head + 1) % RING_SLOTS);
  if (next == gTail) { gStats.overflow++; return; }
  gRing[head].len = (uint8_t)len;
  memcpy(gRing[head].buf, data, len);
  if (gTransport == TR_NONE && info && info->src_addr) {
    memcpy(gPeer, info->src_addr, 6);
    gTransport = TR_ESPNOW;
  }
  gHead = next;
  gStats.rx++;
}

static bool udpSendFrame(const IPAddress &to, uint16_t port,
                         const uint8_t *frame, uint8_t len) {
  if (!gUdpUp) return false;
  uint8_t packet[LINK_UDP_MAX_PACKET];
  size_t packetLen = linkUdpEncode(packet, sizeof(packet), gNodeId, frame, len);
  if (!packetLen) return false;
  if (!gUdp.beginPacket(to, port)) return false;
  size_t written = gUdp.write(packet, packetLen);
  return written == packetLen && gUdp.endPacket() == 1;
}

static void hybridSend(void *, const uint8_t *frame, uint8_t len) {
  if (!gUp || !frame) return;
  if (gEspNowUp && gTransport != TR_UDP)
    esp_now_send(gTransport == TR_ESPNOW ? gPeer : BROADCAST, frame, len);
  if (gUdpUp && gTransport != TR_ESPNOW) {
    IPAddress destination = gTransport == TR_UDP ? gUdpPeer : IPAddress(192, 168, 4, 255);
    uint16_t port = gTransport == TR_UDP ? gUdpPeerPort : LINK_UDP_PORT;
    if (udpSendFrame(destination, port, frame, len)) gStats.tx++;
    else gStats.txFail++;
  }
}

static void lockEspPeer() {
  if (gTransport != TR_ESPNOW || gPeerAdded) return;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, gPeer, 6);
  peer.channel = 1;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) == ESP_OK) {
    gPeerAdded = true;
    Serial.printf("ESP-NOW peer %02X:%02X:%02X:%02X:%02X:%02X\n",
                  gPeer[0], gPeer[1], gPeer[2], gPeer[3], gPeer[4], gPeer[5]);
  }
}

static void pollUdp() {
  if (!gUdpUp || !gLink || gTransport == TR_ESPNOW) return;
  for (;;) {
    int size = gUdp.parsePacket();
    if (size <= 0) break;
    uint8_t packet[LINK_UDP_MAX_PACKET];
    IPAddress remote = gUdp.remoteIP();
    uint16_t remotePort = gUdp.remotePort();
    int got = gUdp.read(packet, sizeof(packet));
    if (got != size) continue;
    uint32_t sender = 0;
    const uint8_t *frame = nullptr;
    uint8_t frameLen = 0;
    if (!linkUdpDecode(packet, (size_t)got, &sender, &frame, &frameLen)) continue;
    if (!sender || sender == gNodeId) continue;
    if (gTransport == TR_NONE) {
      gUdpPeer = remote;
      gUdpPeerPort = remotePort;
      gUdpPeerNode = sender;
      gTransport = TR_UDP;
      Serial.printf("UDP peer %s:%u\n", remote.toString().c_str(), remotePort);
    } else if (sender != gUdpPeerNode || remote != gUdpPeer || remotePort != gUdpPeerPort) {
      gStats.foreign++;
      continue;
    }
    gStats.rx++;
    gLink->onPacket(frame, frameLen);
  }
}

void linkNowPoll() {
  if (!gUp || !gLink) return;
  pollUdp();
  lockEspPeer();
  while (gTail != gHead) {
    RingSlot &slot = gRing[gTail];
    gLink->onPacket(slot.buf, slot.len);
    gTail = (uint8_t)((gTail + 1) % RING_SLOTS);
  }
}

bool linkNowBegin(Link *link) {
  if (!link) return false;
  if (gUp) {
    if (gPeerAdded) esp_now_del_peer(gPeer);
    gLink = link;
    gTransport = TR_NONE;
    gPeerAdded = false;
    gHead = gTail = 0;
    gUdpPeerNode = 0;
    gStats = LinkNowStats();
    link->id = (uint16_t)(gNodeId ^ (gNodeId >> 16));
    if (!link->id) link->id = 1;
    link->send = hybridSend;
    link->ctx = nullptr;
    return true;
  }
  gLink = link;
  gTransport = TR_NONE;
  gPeerAdded = false;
  gHead = gTail = 0;
  gStats = LinkNowStats();

  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  snprintf(gApName, sizeof(gApName), "TamaPoke-%02X%02X", mac[4], mac[5]);
  gNodeId = ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) |
            ((uint32_t)mac[4] << 8) | mac[5];
  if (!gNodeId) gNodeId = micros() | 1u;

  // The AP gives Android a network that needs no router or credential entry on
  // the watch. ESP-NOW remains on the same fixed channel for board-to-board play.
  gApUp = WiFi.softAP(gApName, AP_PASSWORD, 1, false, 2);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  gUdpUp = gApUp && gUdp.begin(LINK_UDP_PORT);

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onRecv);
    esp_now_register_send_cb(onSent);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST, 6);
    peer.channel = 1;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) == ESP_OK) gEspNowUp = true;
    else esp_now_deinit();
  }

  gUp = gEspNowUp || gUdpUp;
  if (!gUp) {
    if (gUdpUp) gUdp.stop();
    if (gApUp) WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    gApName[0] = 0;
    gLink = nullptr;
    return false;
  }
  link->id = (uint16_t)(gNodeId ^ (gNodeId >> 16));
  if (!link->id) link->id = 1;
  link->send = hybridSend;
  link->ctx = nullptr;
  Serial.printf("LAN listo: ESP-NOW %s, UDP %s, AP %s, id %04X\n",
                gEspNowUp ? "si" : "no", gUdpUp ? "si" : "no",
                gApUp ? gApName : "no", link->id);
  return true;
}

void linkNowEnd() {
  if (!gUp && !gApUp) return;
  gUp = false;
  if (gEspNowUp) {
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
  }
  if (gUdpUp) gUdp.stop();
  if (gApUp) WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.printf("LAN fin: rx %u tx %u fallos %u ajenos %u desborde %u\n",
                gStats.rx, gStats.tx, gStats.txFail, gStats.foreign,
                gStats.overflow);
  gEspNowUp = gUdpUp = gApUp = false;
  gTransport = TR_NONE;
  gApName[0] = 0;
  gLink = nullptr;
}

bool linkNowUp() { return gUp; }
const char *linkNowNetworkName() { return gApUp ? gApName : ""; }
const char *linkNowNetworkPassword() { return gApUp ? AP_PASSWORD : ""; }
const LinkNowStats &linkNowStats() { return gStats; }
