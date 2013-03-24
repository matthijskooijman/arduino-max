#include "Util.h"

uint32_t getBits(const uint8_t *buf, uint8_t start_bit, uint8_t num_bits) {
  uint32_t res = 0;
  while (num_bits) {
    /* Take min(8,num_bits) bits starting at start_bit */
    uint8_t mask = 0xff;
    if (num_bits < 8)
      mask <<= (8 - num_bits);
    if (start_bit)
      mask >>= start_bit;

    /* Select the right bits */
    uint32_t select = ((uint32_t)*buf) & mask;

    /* Make sure the bits get shifted to the right spot in the final
     * result. Unfortunately we can't shift by a negative amount, so
     * we need this if. */
    if (num_bits > 8)
      res += select << (num_bits - 8);
    else
      res += select >> (8 - num_bits);

    num_bits -= (8 - start_bit);
    start_bit = 0;
    buf++;
  }
  return res;
}

/* vim: set sw=2 sts=2 expandtab: */
