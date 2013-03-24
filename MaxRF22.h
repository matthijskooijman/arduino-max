#ifndef __MAX_RF_22_H
#define __MAX_RF_22_h

#include <RF22.h>

class MaxRF22 : public RF22 {
public:
  MaxRF22(uint8_t ss = SS, uint8_t interrupt = 0) : RF22(ss, interrupt) {}
  bool init();
};

#endif // __MAX_RF_22_H

/* vim: set sw=2 sts=2 expandtab: */
