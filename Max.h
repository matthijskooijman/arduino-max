#ifndef __MAX_H
#define __MAX_H

// Control a relay on this pin (undef to disable)
#define KETTLE_RELAY_PIN 4

// Enable the LCD display (undef to disable)
#define LCD_I2C

// Enable the ethernet server (undef to disable)
#define ETHERNET

#define ETHERNET_MAC  { 0x90, 0xA2, 0xDA, 0x0D, 0xb5, 0x82 }

/* String stored in Flash. Type helps the Print class to autoload the
 * string during printing. */
typedef __FlashStringHelper FlashString;

#endif // __MAX_H

/* vim: set sw=2 sts=2 expandtab: */
