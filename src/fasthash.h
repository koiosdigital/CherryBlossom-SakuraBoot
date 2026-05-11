#ifndef FASTHASH_H
#define FASTHASH_H

#include <stddef.h>
#include <stdint.h>

/* fasthash64/32 — matches scripts/gen_metadata.py byte-for-byte. */
uint64_t fasthash64(const void *data, size_t len, uint64_t seed);
uint32_t fasthash32(const void *data, size_t len, uint64_t seed);

#endif /* FASTHASH_H */
