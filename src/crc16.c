#include "crc16.h"

uint16_t crc16_ccitt(const uint8_t *data, uint32_t len) {
  uint16_t crc = 0xFFFF;
  for (uint32_t i = 0; i < len; i++) {
    uint8_t x = (crc >> 8) ^ data[i];
    x ^= x >> 4;
    crc = (crc << 8) ^ ((uint16_t)x << 12) ^ ((uint16_t)x << 5) ^ (uint16_t)x;
  }
  return crc;
}
