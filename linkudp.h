#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "link.h"

// UDP framing shared by Android and the ESP32 access-point bridge. The inner
// frame is still Link's [type][length][payload]; the small outer header only
// identifies TamaPoke traffic and lets a socket discard its own broadcasts.
#define LINK_UDP_PORT 38631u
#define LINK_UDP_HEADER 8u
#define LINK_UDP_MAX_PACKET (LINK_UDP_HEADER + 2u + LINK_MAX_PAYLOAD)

static const uint8_t LINK_UDP_MAGIC[4] = { 'T', 'P', 'L', '1' };

inline size_t linkUdpEncode(uint8_t *out, size_t capacity, uint32_t nodeId,
                            const uint8_t *frame, uint8_t frameLen) {
  size_t total = LINK_UDP_HEADER + frameLen;
  if (!out || !frame || !nodeId || frameLen < 2 ||
      frameLen > 2 + LINK_MAX_PAYLOAD || capacity < total) return 0;
  memcpy(out, LINK_UDP_MAGIC, sizeof(LINK_UDP_MAGIC));
  out[4] = (uint8_t)(nodeId & 0xFF);
  out[5] = (uint8_t)((nodeId >> 8) & 0xFF);
  out[6] = (uint8_t)((nodeId >> 16) & 0xFF);
  out[7] = (uint8_t)((nodeId >> 24) & 0xFF);
  memcpy(out + LINK_UDP_HEADER, frame, frameLen);
  return total;
}

inline bool linkUdpDecode(const uint8_t *packet, size_t size,
                          uint32_t *sender, const uint8_t **frame,
                          uint8_t *frameLen) {
  if (!packet || !sender || !frame || !frameLen ||
      size < LINK_UDP_HEADER + 2 || size > LINK_UDP_MAX_PACKET ||
      memcmp(packet, LINK_UDP_MAGIC, sizeof(LINK_UDP_MAGIC)) != 0) return false;
  uint32_t id = (uint32_t)packet[4] | ((uint32_t)packet[5] << 8) |
                ((uint32_t)packet[6] << 16) | ((uint32_t)packet[7] << 24);
  size_t innerSize = size - LINK_UDP_HEADER;
  const uint8_t *inner = packet + LINK_UDP_HEADER;
  if (!id || inner[1] + 2u != innerSize) return false;
  *sender = id;
  *frame = inner;
  *frameLen = (uint8_t)innerSize;
  return true;
}
