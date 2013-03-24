#ifndef __MAX_CRC_H
#define __MAX_CRC_H

#include <stdint.h>
#include <stddef.h>

uint16_t calc_crc(uint8_t *buf, size_t len);

#endif // __MAX_CRC_H

/* vim: set sw=2 sts=2 expandtab: */
