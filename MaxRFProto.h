#ifndef __MAX_RF_PROTO_H
#define __MAX_RF_PROTO_H

#include <stdint.h>
#include <Print.h>
#include <TStreaming.h>

#include "Max.h"
#include "Util.h"

const size_t RF_ADDR_SIZE = 24;
const uint16_t ACTUAL_TEMP_UNKNOWN = 0xffff;
const uint8_t SET_TEMP_UNKNOWN = 0xff;
const uint8_t VALVE_UNKNOWN = 0xff;

// Constant string with external linkage, so it can be passed as a
// template param
constexpr const char na[] = "NA";

typedef HexBits<RF_ADDR_SIZE> Address;
typedef SpecialValue<Fixed<10, 1>,
                     TInt<ACTUAL_TEMP_UNKNOWN>,
                     TStr<na>>
        ActualTemp;
typedef SpecialValue<Fixed<2, 1>,
                     TInt<SET_TEMP_UNKNOWN>,
                     TStr<na>>
        SetTemp;
typedef SpecialValue<Postfix<NoFormat, TChar<'%'>>,
                     TInt<VALVE_UNKNOWN>,
                     TStr<na>>
        ValvePos;

enum mode {MODE_AUTO, MODE_MANUAL, MODE_TEMPORARY, MODE_BOOST, MODE_UNKNOWN};
enum display_mode {DISPLAY_SET_TEMP, DISPLAY_ACTUAL_TEMP};

enum device_type {DEVICE_CUBE, DEVICE_WALL, DEVICE_RADIATOR};

/**
 * Current state for a specific device.
 */
class Device {
public:
  uint32_t address;
  enum device_type type;
  const char *name;
  uint8_t set_temp; /* In 0.5° increments */
  uint16_t actual_temp; /* In 0.1° increments */
  unsigned long actual_temp_time; /* When was the actual_temp last updated */
  union {
    struct {
      enum mode mode;
      uint8_t valve_pos; /* 0-64 (inclusive) */
    } radiator;

    struct {
    } wall;
  } data;
};

/*
 * Known devices, terminated with a NULL entry.
 */
extern Device devices[6];

class UntilTime : public Printable {
public:
  /* Parse an until time from three bytes from an RF packet */
  UntilTime(const uint8_t *buf);

  virtual size_t printTo(Print &p) const;

  uint8_t year, month, day;

  /* In 30-minute increments */
  uint8_t time;
};


class MaxRFMessage : public Printable {
public:
  /**
   * Parse a RF message. Buffer should contain only headers and
   * payload (so no length byte and no CRC).
   *
   * Note that the message might keep a reference to the buffer around
   * to prevent unnecessary copies!
   */
  static MaxRFMessage *parse(const uint8_t *buf, size_t len);

  /**
   * Returns a string describing a given message type.
   */
  static const FlashString *type_to_str(uint8_t type);

  virtual size_t printTo(Print &p) const;

  uint8_t seqnum;
  uint8_t flags;
  uint8_t type;
  uint32_t addr_from;
  uint32_t addr_to;
  uint8_t group_id;

  virtual ~MaxRFMessage() {}
private:
  static MaxRFMessage *create_message_from_type(uint8_t type);
  virtual bool parse_payload(const uint8_t *buf, size_t len) = 0;
};

class UnknownMessage : public MaxRFMessage {
public:
  virtual bool parse_payload(const uint8_t *buf, size_t len);
  virtual size_t printTo(Print &p) const;

  /**
   * The raw data of the message (excluding headers).
   * Shouldn't be freed, since this is a reference into the buffer
   * passed to parse.
   */
  const uint8_t* payload;
  size_t payload_len;
};

class SetTemperatureMessage : public MaxRFMessage {
public:
  virtual bool parse_payload(const uint8_t *buf, size_t len);
  virtual size_t printTo(Print &p) const;

  uint8_t set_temp; /* In 0.5° units */
  enum mode mode;

  UntilTime *until; /* Only when mode is MODE_TEMPORARY */

  virtual ~SetTemperatureMessage() {delete this->until; }
};

class WallThermostatStateMessage : public MaxRFMessage {
public:
  virtual bool parse_payload(const uint8_t *buf, size_t len);
  virtual size_t printTo(Print &p) const;

  uint16_t actual_temp; /* In 0.1° units */
  uint8_t set_temp; /* In 0.5° units */
};

class ThermostatStateMessage : public MaxRFMessage {
public:
  virtual bool parse_payload(const uint8_t *buf, size_t len);
  virtual size_t printTo(Print &p) const;

  bool dst;
  bool locked;
  bool battery_low;
  enum mode mode;
  uint8_t valve_pos; /* In percent */
  uint8_t set_temp; /* In 0.5° units */
  uint8_t actual_temp; /* In 0.1° units, 0 when not present */
  UntilTime *until; /* Only when mode is MODE_TEMPORARY */
  virtual ~ThermostatStateMessage() {delete this->until; }
};

class SetDisplayActualTemperatureMessage : public MaxRFMessage {
public:
  virtual bool parse_payload(const uint8_t *buf, size_t len);
  virtual size_t printTo(Print &p) const;

  enum display_mode display_mode;
};

#endif // __MAX_RF_PROTO_H

/* vim: set sw=2 sts=2 expandtab: */
