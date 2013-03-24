#include "Crc.h"

/**
 * CRC code based on example from Texas Instruments DN502, matches
 * CC1101 implementation
 */
#define CRC16_POLY 0x8005
static uint16_t calc_crc_step(uint8_t crcData, uint16_t crcReg) {
  uint8_t i;
  for (i = 0; i < 8; i++) {
    if (((crcReg & 0x8000) >> 8) ^ (crcData & 0x80))
      crcReg = (crcReg << 1) ^ CRC16_POLY;
    else
      crcReg = (crcReg << 1);
    crcData <<= 1;
  }
  return crcReg;
} // culCalcCRC

#define CRC_INIT 0xFFFF
uint16_t calc_crc(uint8_t *buf, size_t len) {
  uint16_t checksum;
  checksum = CRC_INIT;
  // Init value for CRC calculation
  for (size_t i = 0; i < len; i++)
    checksum = calc_crc_step(buf[i], checksum);
  return checksum;
}

/* vim: set sw=2 sts=2 expandtab: */
