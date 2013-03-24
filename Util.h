#ifndef __MAX_UTIL_H
#define __MAX_UTIL_H

#include <stdint.h>

/**
 * Get a number of bits from the given buffer, optionally skipping a few
 * bits at the start.
 */
uint32_t getBits(const uint8_t *buf, uint8_t start_bit, uint8_t num_bits);

#define lengthof(x) (sizeof(x) / sizeof(*x))

#endif // __MAX_UTIL_H

/* vim: set sw=2 sts=2 expandtab: */
