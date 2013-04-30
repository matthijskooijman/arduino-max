#include "Max.h"

#include <TStreaming.h>
#ifdef ETHERNET
#include <Ethernet.h>
#endif

#ifdef LCD_I2C
#include <LiquidCrystal_I2C.h>
#endif // LCD_I2C

#include "Crc.h"
#include "Util.h"
#include "MaxRF22.h"
#include "MaxRFProto.h"
#include "Pn9.h"

static_assert(PN9_LEN >= RF22_MAX_MESSAGE_LEN, "Not enough pn9 bytes defined");

enum device_type {
  DEVICE_CUBE,
  DEVICE_WALL,
  DEVICE_RADIATOR,
};

struct device {
  uint32_t address;
  enum device_type type;
  const char *name;
  uint8_t set_temp; /* In 0.5° increments */
  uint8_t actual_temp; /* In 0.1° increments */
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

struct device devices[6] = {
  {0x00b825, DEVICE_CUBE, "cube"},
  {0x0298e5, DEVICE_WALL, "wall"},
  {0x04c8dd, DEVICE_RADIATOR, "up  "},
  {0x0131b4, DEVICE_RADIATOR, "down"},
  0,
};

MaxRF22 rf(9);

#ifdef LCD_I2C
#define LCD_ADDR 0x20
#define LCD_COLS 20
#define LCD_ROWS 4
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS); // set the LCD address to 0x27 for a 20 chars and 4 line display
#else
/* Define the lcd object as a bottomless pit for prints. */
Null lcd;
#endif // LCD_I2C

#ifdef KETTLE_RELAY_PIN
bool kettle_status;
#endif // KETTLE_RELAY_PIN


#ifdef ETHERNET
EthernetServer server = EthernetServer(1234); //port 80

DoublePrint p = (Serial & server);
#else
Print &p = Serial;
#endif

void printStatus() {
  #ifdef LCD_I2C
  int row = LCD_ROWS - 1;
  lcd.clear();
  #endif
  for (int i = 0; i < lengthof(devices); ++i) {
    struct device *d = &devices[i];
    if (!d->address) break;
    if (d->type != DEVICE_RADIATOR && d->type != DEVICE_WALL) continue;

    #ifdef LCD_I2C
    lcd.setCursor(0, row--);
    #endif

    if (d->name) {
      (p & lcd) << d->name;
    } else {
      /* Only print two bytes on the lcd to save space */
      lcd << V<HexBits<16>>(d->address);
      p << V<Address>(d->address);
    }

    (p & lcd) << " " << V<ActualTemp>(d->actual_temp)
              << "/" << V<SetTemp>(d->set_temp);
    if (d->type == DEVICE_RADIATOR)
      (p & lcd) << " " << d->data.radiator.valve_pos << "%";
    p << endl;
  }
  p << endl;

  #ifdef LCD_I2C
  #ifdef KETTLE_RELAY_PIN
  lcd.home();

  (p & lcd) << F("Kettle: ") << (kettle_status ? F("On") : F("Off"));
  #endif // KETTLE_RELAY_PIN
  #endif // LCD_I2C

  p << endl;

  /* Print machine-parseable status line (to draw pretty graphs) */
  p << "STATUS\t" << millis() << "\t";
  for (int i = 0; i < lengthof(devices); ++i) {
    struct device *d = &devices[i];
    if (!d->address) break;
    if (d->type != DEVICE_RADIATOR && d->type != DEVICE_WALL) continue;

    p << V<ActualTemp>(d->actual_temp) << "\t" << V<SetTemp>(d->set_temp) << "\t";
    if (d->type == DEVICE_RADIATOR)
      p << V<ValvePos>(d->data.radiator.valve_pos);
    else
      p << "NA";
    p << "\t";
  }
  p << (kettle_status ? "1" : "0") << endl;
}

#ifdef KETTLE_RELAY_PIN
void switchKettle() {
  uint32_t total = 0;
  for (int i = 0; i < lengthof(devices); ++i) {
    struct device *d = &devices[i];
    if (!d->address) break;
    if (d->type != DEVICE_RADIATOR) continue;
    total += d->data.radiator.valve_pos;
  }

  /* Sum of valve positions > 50% */
  kettle_status = (total > 50);
  digitalWrite(KETTLE_RELAY_PIN, kettle_status ? HIGH : LOW);
}
#endif // KETTLE_RELAY_PIN

/* Find or assign a device struct based on the address */
struct device *get_device(uint32_t addr, enum device_type type) {
  for (int i = 0; i < lengthof(devices); ++i) {
    /* The address is not in the list yet, assign this empty slot. */
    if (devices[i].address == 0) {
      devices[i].address = addr;
      devices[i].type = type;
      devices[i].name = NULL;
    }
    /* Found it */
    if (devices[i].address == addr)
      return &devices[i];
  }
  /* Not found and no slots left */
  return NULL;
}

void setup()
{
  Serial.begin(115200);

  if (!rf.init())
    Serial.println(F("RF init failed"));

  #ifdef LCD_I2C
  lcd.init();
  lcd.backlight();
  lcd.home();
  lcd.clear();
  #endif // LCD_I2C

  #ifdef KETTLE_RELAY_PIN
  pinMode(KETTLE_RELAY_PIN, OUTPUT);
  #endif // KETTLE_RELAY_PIN

  #ifdef ETHERNET
  byte mac[] = ETHERNET_MAC;
  if (Ethernet.begin(mac))
    p << F("IP: ") << Ethernet.localIP() << "\r\n";
  else
    p << F("DHCP Failure") << "\r\n";

  server.begin();
  #endif

  p << F("Initialized") << "\r\n";
  printStatus();
}

void loop()
{
  uint8_t buf[RF22_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  if (Serial.read() != -1)
    Serial.println("OK");

  #ifdef ETHERNET
  EthernetClient c = server.available();
  if (c && c.read() != -1)
    c.println("OK");
  #endif

  if (rf.recv(buf, &len))
  {
    /* Enable reception right away again, so we won't miss the next
     * message while processing this one. */
    rf.setModeRx();

    p << F("Received ") << len << F(" bytes") << "\r\n";

    /* Dump the raw received data */
    int i, j;
    for (i = 0; i < len; i += 16)
    {
      // Hex
      for (j = 0; j < 16 && i+j < len; j++)
      {
        p << V<Hex>(buf[i+j]) << " ";
      }
      // Padding on last block
      while (j++ < 16)
        p << "   ";

      p << "   ";
      // ASCII
      for (j = 0; j < 16 && i+j < len; j++)
        p << (isprint(buf[i+j]) ? (char)buf[i+j] : '.');
      p << "\r\n";
    }
    p << "\r\n";

    if (len < 3) {
      p << F("Invalid packet length (") << len << ")" << "\r\n";
      return;
    }

    /* Dewhiten data */
    if (xor_pn9(buf, len) < 0) {
      p << F("Invalid packet length (") << len << ")" << "\r\n";
      return;
    }

    /* Calculate CRC (but don't include the CRC itself) */
    uint16_t crc = calc_crc(buf, len - 2);
    if (buf[len - 1] != (crc & 0xff) || buf[len - 2] != (crc >> 8)) {
      p << F("CRC error");
      return;
    }

    /* Parse the message (without length byte and CRC) */
    MaxRFMessage *rfm = MaxRFMessage::parse(buf + 1, len - 3);

    if (rfm == NULL) {
      p << F("Packet is invalid") << "\r\n";
    } else {
      p << *rfm << "\r\n";

      /* Update internal state from some messages */
      if (rfm->type == 0x42) { /* WallThermostatState */
        WallThermostatStateMessage *m = (WallThermostatStateMessage*)rfm;
        struct device *d = get_device(m->addr_from, DEVICE_WALL);
        d->set_temp = m->set_temp;
        d->actual_temp = m->actual_temp;
        d->actual_temp_time = millis();
      } else if (rfm->type == 0x60) { /* ThermostateState */
        ThermostatStateMessage *m = (ThermostatStateMessage*)rfm;
        struct device *d = get_device(m->addr_from, DEVICE_RADIATOR);
        d->set_temp = m->set_temp;
        d->data.radiator.valve_pos = m->valve_pos;
        if (m->actual_temp) {
          d->actual_temp = m->actual_temp;
          d->actual_temp_time = millis();
        }
      }
    }
    delete rfm;

    #ifdef KETTLE_RELAY_PIN
    switchKettle();
    #endif // KETTLE_RELAY_PIN

    printStatus();

    #if 0
    #ifdef LCD_I2C
    /* Use the first two rows of the LCD for dumped packet data */
    lcd.home();
    for (i = 0; i < len && i < LCD_COLS; ++i) {
      if (i == LCD_COLS / 2) lcd.setCursor(0, 1);
      printHex(NULL, buf[i], BYTE_SIZE, false, &lcd);
    }
    #endif // LCD_I2C
    #endif

    p << "\r\n";
  }
}

/* vim: set sw=2 sts=2 expandtab filetype=cpp: */
