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
    Device *d = &devices[i];
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
      (p & lcd) << " " << V<ValvePos>(d->data.radiator.valve_pos);
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
    Device *d = &devices[i];
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
    Device *d = &devices[i];
    if (!d->address) break;
    if (d->type != DEVICE_RADIATOR) continue;
    if (d->data.radiator.valve_pos == VALVE_UNKNOWN) continue;
    total += d->data.radiator.valve_pos;
  }

  /* Sum of valve positions > 50% */
  kettle_status = (total > 50);
  digitalWrite(KETTLE_RELAY_PIN, kettle_status ? HIGH : LOW);
}
#endif // KETTLE_RELAY_PIN

void dump_buffer(Print &p, uint8_t *buf, uint8_t len) {
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
}

void setup()
{
  Serial.begin(115200);

  if (rf.init())
    Serial.println(F("RF init OK"));
  else
    Serial.println(F("RF init failed"));

  #ifdef LCD_I2C
  lcd.init();
  lcd.backlight();
  lcd.home();
  lcd.clear();
  Serial.println(F("LCD init complete"));

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

  if (Serial.read() != -1) {
    Serial.println("OK");
    printStatus();
  }

  #ifdef ETHERNET
  EthernetClient c = server.available();
  if (c && c.read() != -1) {
    c.println("OK");
    printStatus();
  }
  #endif

  if (rf.recv(buf, &len))
  {
    /* Enable reception right away again, so we won't miss the next
     * message while processing this one. */
    rf.setModeRx();

    p << F("Received ") << len << F(" bytes") << "\r\n";

    dump_buffer(p, buf, len);

    if (len < 3) {
      p << F("Invalid packet length (") << len << ")" << "\r\n";
      return;
    }

    /* Dewhiten data */
    if (xor_pn9(buf, len) < 0) {
      p << F("Invalid packet length (") << len << ")" << "\r\n";
      return;
    }

    p << F("Dewhitened:") << "\r\n";
    dump_buffer(p, buf, len);

    /* Calculate CRC (but don't include the CRC itself) */
    uint16_t crc = calc_crc(buf, len - 2);
    if (buf[len - 1] != (crc & 0xff) || buf[len - 2] != (crc >> 8)) {
      p << F("CRC error") << "\r\n";
      return;
    }

    /* Parse the message (without length byte and CRC) */
    MaxRFMessage *rfm = MaxRFMessage::parse(buf + 1, len - 3);

    if (rfm == NULL) {
      p << F("Packet is invalid") << "\r\n";
    } else {
      p << *rfm << "\r\n";

      rfm->updateState();
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
