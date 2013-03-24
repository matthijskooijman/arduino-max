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

/* MaxRFMessage */

const char *MaxRFMessage::type_to_str(uint8_t type) {
  switch(type) {
    case 0x00: return "PairPing";
    case 0x01: return "PairPong";
    case 0x02: return "Ack";
    case 0x03: return "TimeInformation";
    case 0x10: return "ConfigWeekProfile";
    case 0x11: return "ConfigTemperatures";
    case 0x12: return "ConfigValve";
    case 0x20: return "AddLinkPartner";
    case 0x21: return "RemoveLinkPartner";
    case 0x22: return "SetGroupId";
    case 0x23: return "RemoveGroupId";
    case 0x30: return "ShutterContactState";
    case 0x40: return "SetTemperature";
    case 0x42: return "WallThermostatState";
    case 0x43: return "SetComfortTemperature";
    case 0x44: return "SetEcoTemperature";
    case 0x50: return "PushButtonState";
    case 0x60: return "ThermostatState";
    case 0x82: return "SetDisplayActualTemperature";
    case 0xF1: return "WakeUp";
    case 0xF0: return "Reset";
  }
  return "Unknown";
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
  p << V<Title>("Sequence num:") << V<Hex>(this->seqnum) << "\r\n";
  p << V<Title>("Flags:") << V<Hex>(this->flags) << "\r\n";
  p << V<Title>("Packet type:") << V<Hex>(this->type)
    << " (" << type_to_str(this->type) << ")" << "\r\n";
  p << V<Title>("Packet from:") << V<Address>(this->addr_from) << "\r\n";
  p << V<Title>("Packet to:") << V<Address>(this->addr_to) << "\r\n";
  p << V<Title>("Group id:") << V<Hex>(this->group_id) << "\r\n";

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
  p << V<Title>("Payload:")
    << V<Array<Hex, ' ', false>>(this->payload, this->payload_len)
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
  p << V<Title>("Mode:") << mode_str[this->mode] << "\r\n";
  p << V<Title>("Set temp:") << V<SetTemp>(this->set_temp) << "\r\n";
  if (this->until) {
    p << V<Title>("Until:") << *(this->until) << "\r\n";
  }

  return 0; /* XXX */
}

/* WallThermostatStateMessage */

bool WallThermostatStateMessage::parse_payload(const uint8_t *buf, size_t len) {
  if (len < 2)
    return false;

  this->set_temp = buf[0] & 0x7f;
  this->actual_temp = ((buf[0] & 0x80) << 1) |  buf[1];

  return true;
}

size_t WallThermostatStateMessage::printTo(Print &p) const {
  MaxRFMessage::printTo(p);
  p << V<Title>("Set temp:") << V<SetTemp>(this->set_temp) << "\r\n";
  p << V<Title>("Actual temp:") << V<ActualTemp>(this->actual_temp) << "\r\n";

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
  p << V<Title>("Mode:") << mode_str[this->mode] << "\r\n";
  p << V<Title>("Adjust to DST:") << this->dst << "\r\n";
  p << V<Title>("Locked:") << this->locked << "\r\n";
  p << V<Title>("Battery Low:") << this->battery_low << "\r\n";
  p << V<Title>("Valve position:") << this->valve_pos << "%" << "\r\n";
  p << V<Title>("Set temp:") << V<SetTemp>(this->set_temp) << "\r\n";

  if (this->actual_temp)
    p << V<Title>("Actual temp:") << V<ActualTemp>(this->actual_temp) << "\r\n";

  if (this->until)
    p << V<Title>("Until:") << *(this->until) << "\r\n";

  return 0; /* XXX */
}

/* SetDisplayActualTemperatureMessage */
bool SetDisplayActualTemperatureMessage::parse_payload(const uint8_t *buf, size_t len) {
  if (len < 1)
    return NULL;
  this->display_mode = (enum display_mode) ((buf[0] >> 5) & 0x1);
  return true;
}

size_t SetDisplayActualTemperatureMessage::printTo(Print &p) const{
  MaxRFMessage::printTo(p);
  p << V<Title>("Display mode:") << display_mode_str[this->display_mode] << "\r\n";
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

04: Display mode?
19: DST switch, mode = auto
    0:1 mode
    3   DST switch
2A: set temp (21°)
00: ?? Unused?
CD: Actual temp (20.5°)
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
*/

/*
Sequence num:   2C
Flags:          02
Packet type:    02 (Ack)
Packet from:    0298E5
Packet to:      04C8DD
Group id:       00
Payload:        01 11 00 28
01: 1 == ack? 0 == nak??
11: same as 11/19 in type 70?
00: Valve position
28: Set temp
*/

/* vim: set sw=2 sts=2 expandtab: */
