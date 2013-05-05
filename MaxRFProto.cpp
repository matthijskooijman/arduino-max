#include "MaxRFProto.h"

/* TStreaming Formatting type to use for the field titles in print
 * output. */
typedef Align<16> Title;

static const char *mode_str[] = {
  [MODE_AUTO] = "auto",
  [MODE_MANUAL] = "manual",
  [MODE_TEMPORARY] = "temporary",
  [MODE_BOOST] = "boost",
};

static const char *display_mode_str[] = {
  [DISPLAY_SET_TEMP] = "Set temperature",
  [DISPLAY_ACTUAL_TEMP] = "Actual temperature",
};

/**
 * Static list of known devices.
 */
Device devices[] = {
  {0x00b825, DEVICE_CUBE, "cube", SET_TEMP_UNKNOWN, ACTUAL_TEMP_UNKNOWN, 0},
  {0x0298e5, DEVICE_WALL, "wall", SET_TEMP_UNKNOWN, ACTUAL_TEMP_UNKNOWN, 0},
  {0x04c8dd, DEVICE_RADIATOR, "up  ", SET_TEMP_UNKNOWN, ACTUAL_TEMP_UNKNOWN, 0, {.radiator = {MODE_UNKNOWN, VALVE_UNKNOWN}}},
  {0x0131b4, DEVICE_RADIATOR, "down", SET_TEMP_UNKNOWN, ACTUAL_TEMP_UNKNOWN, 0, {.radiator = {MODE_UNKNOWN, VALVE_UNKNOWN}}},
  0,
};


/* MaxRFMessage */

const FlashString *MaxRFMessage::type_to_str(uint8_t type) {
  switch(type) {
    case 0x00: return F("PairPing");
    case 0x01: return F("PairPong");
    case 0x02: return F("Ack");
    case 0x03: return F("TimeInformation");
    case 0x10: return F("ConfigWeekProfile");
    case 0x11: return F("ConfigTemperatures");
    case 0x12: return F("ConfigValve");
    case 0x20: return F("AddLinkPartner");
    case 0x21: return F("RemoveLinkPartner");
    case 0x22: return F("SetGroupId");
    case 0x23: return F("RemoveGroupId");
    case 0x30: return F("ShutterContactState");
    case 0x40: return F("SetTemperature");
    case 0x42: return F("WallThermostatState");
    case 0x43: return F("SetComfortTemperature");
    case 0x44: return F("SetEcoTemperature");
    case 0x50: return F("PushButtonState");
    case 0x60: return F("ThermostatState");
    case 0x82: return F("SetDisplayActualTemperature");
    case 0xF1: return F("WakeUp");
    case 0xF0: return F("Reset");
  }
  return F("Unknown");
}

MaxRFMessage *MaxRFMessage::create_message_from_type(uint8_t type) {
  switch(type) {
    case 0x40: return new SetTemperatureMessage();
    case 0x42: return new WallThermostatStateMessage();
    case 0x60: return new ThermostatStateMessage();
    case 0x82: return new SetDisplayActualTemperatureMessage();
    default: return new UnknownMessage();
  }
}

MaxRFMessage *MaxRFMessage::parse(const uint8_t *buf, size_t len) {
  if (len < 10)
    return NULL;

  uint8_t type = buf[2];
  MaxRFMessage *m = create_message_from_type(type);

  m->seqnum = buf[0];
  m->flags = buf[1];
  m->type = type;
  m->addr_from = getBits(buf + 3, 0, RF_ADDR_SIZE);
  m->addr_to = getBits(buf + 6, 0, RF_ADDR_SIZE);
  m->group_id = buf[9];
  if (m->parse_payload(buf + 10, len - 10))
    return m;
  else
    return NULL;
}

size_t MaxRFMessage::printTo(Print &p) const {
  p << V<Title>(F("Sequence num:")) << V<Hex>(this->seqnum) << "\r\n";
  p << V<Title>(F("Flags:")) << V<Hex>(this->flags) << "\r\n";
  p << V<Title>(F("Packet type:")) << V<Hex>(this->type)
    << " (" << type_to_str(this->type) << ")" << "\r\n";
  p << V<Title>(F("Packet from:")) << V<Address>(this->addr_from) << "\r\n";
  p << V<Title>(F("Packet to:")) << V<Address>(this->addr_to) << "\r\n";
  p << V<Title>(F("Group id:")) << V<Hex>(this->group_id) << "\r\n";

  return 0; /* XXX */
}

/* UnknownMessage */
bool UnknownMessage::parse_payload(const uint8_t *buf, size_t len) {
  this->payload = buf;
  this->payload_len = len;
  return true;
}

size_t UnknownMessage::printTo(Print &p) const{
  MaxRFMessage::printTo(p);
  p << V<Title>(F("Payload:"))
    << V<Array<Hex, TChar<' '>>>(this->payload, this->payload_len)
    << "\r\n";
}

/* SetTemperatureMessage */

bool SetTemperatureMessage::parse_payload(const uint8_t *buf, size_t len) {
  if (len < 1)
    return false;

  this->set_temp = buf[0] & 0x3f;
  this->mode = (enum mode) ((buf[0] >> 6) & 0x3);

  if (len >= 4)
    this->until = new UntilTime(buf + 1);
  else
    this->until = NULL;

  return true;
}

size_t SetTemperatureMessage::printTo(Print &p) const{
  MaxRFMessage::printTo(p);
  p << V<Title>(F("Mode:")) << mode_str[this->mode] << "\r\n";
  p << V<Title>(F("Set temp:")) << V<SetTemp>(this->set_temp) << "\r\n";
  if (this->until) {
    p << V<Title>(F("Until:")) << *(this->until) << "\r\n";
  }

  return 0; /* XXX */
}

/* WallThermostatStateMessage */

bool WallThermostatStateMessage::parse_payload(const uint8_t *buf, size_t len) {
  if (len < 2)
    return false;

  this->set_temp = buf[0] & 0x7f;
  this->actual_temp = ((buf[0] & 0x80) << 1) |  buf[1];
  /* Note that mode and until time are not in this message */

  return true;
}

size_t WallThermostatStateMessage::printTo(Print &p) const {
  MaxRFMessage::printTo(p);
  p << V<Title>(F("Set temp:")) << V<SetTemp>(this->set_temp) << "\r\n";
  p << V<Title>(F("Actual temp:")) << V<ActualTemp>(this->actual_temp) << "\r\n";

  return 0; /* XXX */
}

/* ThermostatStateMessage */

bool ThermostatStateMessage::parse_payload(const uint8_t *buf, size_t len) {
  if (len < 3)
    return false;

  this->mode = (enum mode) (buf[0] & 0x3);
  this->dst = (buf[0] >> 2) & 0x1;
  this->locked = (buf[0] >> 5) & 0x1;
  this->battery_low = (buf[0] >> 7) & 0x1;
  this->valve_pos = buf[1];
  this->set_temp = buf[2];

  this->actual_temp = 0;
  if (this->mode != MODE_TEMPORARY && len >= 5)
    this->actual_temp = ((buf[3] & 0x1) << 8) + buf[4];

  this->until = NULL;
  if (this->mode == MODE_TEMPORARY && len >= 6)
    this->until = new UntilTime(buf + 3);

  return true;
}

size_t ThermostatStateMessage::printTo(Print &p) const {
  MaxRFMessage::printTo(p);
  p << V<Title>(F("Mode:")) << mode_str[this->mode] << "\r\n";
  p << V<Title>(F("Adjust to DST:")) << this->dst << "\r\n";
  p << V<Title>(F("Locked:")) << this->locked << "\r\n";
  p << V<Title>(F("Battery Low:")) << this->battery_low << "\r\n";
  p << V<Title>(F("Valve position:")) << this->valve_pos << "%" << "\r\n";
  p << V<Title>(F("Set temp:")) << V<SetTemp>(this->set_temp) << "\r\n";

  if (this->actual_temp)
    p << V<Title>(F("Actual temp:")) << V<ActualTemp>(this->actual_temp) << "\r\n";

  if (this->until)
    p << V<Title>(F("Until:")) << *(this->until) << "\r\n";

  return 0; /* XXX */
}

/* SetDisplayActualTemperatureMessage */
bool SetDisplayActualTemperatureMessage::parse_payload(const uint8_t *buf, size_t len) {
  if (len < 1)
    return NULL;
  this->display_mode = (enum display_mode) ((buf[0] >> 2) & 0x1);
  return true;
}

size_t SetDisplayActualTemperatureMessage::printTo(Print &p) const{
  MaxRFMessage::printTo(p);
  p << V<Title>(F("Display mode:")) << display_mode_str[this->display_mode] << "\r\n";
}

/* UntilTime */

UntilTime::UntilTime(const uint8_t *buf) {
  this->year = buf[1] & 0x3f;
  this->month = ((buf[0] & 0xE0) >> 4) | (buf[1] >> 7);
  this->day = buf[0] & 0x1f;
  this->time = buf[2] & 0x3f;
}

size_t UntilTime::printTo(Print &p) const {
  p << "20" << V<Number<2>>(this->year) << "." << V<Number<2>>(this->month) << "." << V<Number<2>>(this->day);
  p << " " << V<Number<2>>(this->time / 2) << (this->time % 2 ? ":30" : ":00");
}


/*
Sequence num:   E4
Flags:          04
Packet type:    70 (Unknown)
Packet from:    0298E5
Packet to:      000000
Group id:       00
Payload:        19 04 2A 00 CD

19: DST switch, mode = auto
    0:1 mode
    2   DST switch
    5   ??
04: Display mode?
2A: set temp (21°)
00 CD: Actual temp (20.5°)

Sequence num:   9C
Flags:          04
Packet type:    70 (Unknown)
Packet from:    0298E5
Packet to:      000000
Group id:       00
Payload:        12 04 24 48 0D 1B

19: DST switch, mode = temporary
    0:1 mode
    2   DST switch
    5   ??
04: Display mode?
24: set temp (18°)
48 0D 1B: until time

Perhaps 70 is really WallThermostatState and the curren WallThermostatState is
more of a "update temp" message? It seems 70 is sent when the SetTemp of a WT
changes.

*/

/*
Set DST adjust
Sequence num:   E5
Flags:          00
Packet type:    81 (Unknown)
Packet from:    00B825
Packet to:      0298E5
Group id:       00
Payload:        00
00: Disable
01: Enable

Sent to radiator thermostats only?

*/

/*
Sequence num:   2C
Flags:          02
Packet type:    02 (Ack)
Packet from:    0298E5
Packet to:      04C8DD
Group id:       00
Payload:        01 11 00 28
01: 1 == more data? 0 == no data??
11: flags, same as 11/19 in type 70?
00: Valve position / displaymode flags
28: Set temp

Sequence num:   1B
Flags:          02
Packet type:    02 (Ack)
Packet from:    0298E5
Packet to:      00B825
Group id:       00
Payload:        01 12 04 24 48 0D 1B

01: 1 == more data? 0 == no data??
11: flags, same as 11/19 in type 70? x2 == temporary
00: Valve position / displaymode flags
24: Set temp (18.0°)
48 0D 1B: Until time
*/

/* vim: set sw=2 sts=2 expandtab: */
