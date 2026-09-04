#include "Arduino.h"
#include "Preferences.h"
#include "linkudp.h"
#include <cstdio>

uint32_t g_seed = 19;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
bool wasPressed = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}

static int bad = 0;
static void check(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

int main() {
  uint8_t frame[] = { LM_ACT, 2, 7, LINK_ACT_MOVE(0) };
  uint8_t packet[LINK_UDP_MAX_PACKET] = {};
  size_t size = linkUdpEncode(packet, sizeof(packet), 0x12345678u,
                              frame, sizeof(frame));
  check(size == LINK_UDP_HEADER + sizeof(frame), "a Link frame is wrapped once");

  uint32_t sender = 0;
  const uint8_t *decoded = nullptr;
  uint8_t decodedSize = 0;
  check(linkUdpDecode(packet, size, &sender, &decoded, &decodedSize),
        "the shared Android/ESP32 envelope decodes");
  check(sender == 0x12345678u, "the sender identity survives");
  check(decodedSize == sizeof(frame) && !memcmp(decoded, frame, sizeof(frame)),
        "the Link frame survives unchanged");

  packet[0] = 'X';
  check(!linkUdpDecode(packet, size, &sender, &decoded, &decodedSize),
        "foreign UDP traffic is ignored");
  packet[0] = 'T';
  packet[LINK_UDP_HEADER + 1]++;
  check(!linkUdpDecode(packet, size, &sender, &decoded, &decodedSize),
        "a truncated or lying frame is ignored");
  check(!linkUdpEncode(packet, sizeof(packet), 0, frame, sizeof(frame)),
        "a zero sender identity is refused");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
