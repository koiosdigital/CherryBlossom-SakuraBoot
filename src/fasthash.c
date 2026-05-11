#include "fasthash.h"

#include <string.h>

static inline uint64_t mix(uint64_t h) {
  h ^= h >> 23;
  h *= 0x2127599BF4325C37ULL;
  h ^= h >> 47;
  return h;
}

uint64_t fasthash64(const void *data, size_t len, uint64_t seed) {
  const uint64_t m = 0x880355F21E6D1965ULL;
  uint64_t h = seed ^ ((uint64_t)len * m);

  const uint8_t *p = (const uint8_t *)data;
  size_t blocks = len / 8;

  for (size_t i = 0; i < blocks; i++) {
    uint64_t k;
    memcpy(&k, p, 8);  /* unaligned-safe; little-endian on Cortex-M */
    p += 8;
    h ^= mix(k);
    h *= m;
  }

  size_t tail = len & 7;
  if (tail) {
    uint64_t v = 0;
    for (size_t i = 0; i < tail; i++) {
      v |= (uint64_t)p[i] << (i * 8);
    }
    h ^= mix(v);
    h *= m;
  }

  return mix(h);
}

uint32_t fasthash32(const void *data, size_t len, uint64_t seed) {
  uint64_t h = fasthash64(data, len, seed);
  return (uint32_t)(h - (h >> 32));
}
