/*
 * Berliner U-Bahn auf dem Waveshare ESP32-S3 E-Paper 1.54" (200x200 s/w)
 * ----------------------------------------------------------------------
 * Zeigt nacheinander jede Linie U1..U9 (grosse Nummer, von/nach,
 * Stationszahl, schematische Strecke) und wechselt alle ~15 s weiter.
 *
 * Der E-Paper-Treiber ist 1:1 Waveshares eigenem Code fuer dieses Board
 * nachgebaut (eigene SPI-Ansteuerung, exakte Befehlsfolge + Waveform-LUT),
 * weil das Panel eine eigene LUT braucht und Standardbibliotheken es nicht
 * korrekt ansprechen. Gezeichnet wird in einen GFXcanvas1 (Adafruit GFX).
 *
 * Board: "ESP32S3 Dev Module". Bibliothek: Adafruit GFX Library.
 * Pins/Befehle: waveshareteam/ESP32-S3-ePaper-1.54 (epaper_driver_bsp.cpp).
 */
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>

// ---- Pins dieses Boards (aus Waveshare user_config.h) ----
#define EPD_PWR   6    // Display-Strom: LOW = EIN, HIGH = AUS
#define EPD_BUSY  8    // HIGH = beschaeftigt
#define EPD_RST   9
#define EPD_DC    10
#define EPD_CS    11
#define EPD_SCK   12
#define EPD_MOSI  13

#define EPD_W 200
#define EPD_H 200
#define EPD_ROW_BYTES (EPD_W / 8)        // 25
#define EPD_BUF_LEN (EPD_ROW_BYTES * EPD_H)  // 5000

#define INK 1   // gezeichnet (wird schwarz)
#define BG  0   // Hintergrund (wird weiss)

GFXcanvas1 canvas(EPD_W, EPD_H);
SPISettings spiSet(4000000, MSBFIRST, SPI_MODE0);

// Waveform-LUT fuer den vollen Bildaufbau (aus Waveshares epaper_driver_bsp.cpp)
const uint8_t WF_Full_1IN54[159] = {
  0x80,0x48,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x40,0x48,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x80,0x48,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x40,0x48,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x0A,0x00,0x00,0x00,0x00,0x00,0x00,
  0x08,0x01,0x00,0x08,0x01,0x00,0x02,
  0x0A,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x22,0x22,0x22,0x22,0x22,0x22,0x00,0x00,0x00,
  0x22,0x17,0x41,0x00,0x32,0x20
};

// ---------- Low-Level E-Paper (Waveshare-kompatibel) ----------
static inline void epdCmd(uint8_t c)  { digitalWrite(EPD_DC, LOW);  digitalWrite(EPD_CS, LOW); SPI.transfer(c); digitalWrite(EPD_CS, HIGH); }
static inline void epdData(uint8_t d) { digitalWrite(EPD_DC, HIGH); digitalWrite(EPD_CS, LOW); SPI.transfer(d); digitalWrite(EPD_CS, HIGH); }

void epdWrite(const uint8_t *buf, int len, bool invert) {
  digitalWrite(EPD_DC, HIGH);
  digitalWrite(EPD_CS, LOW);
  for (int i = 0; i < len; i++) SPI.transfer(invert ? (uint8_t)~buf[i] : buf[i]);
  digitalWrite(EPD_CS, HIGH);
}

void epdBusy() { while (digitalRead(EPD_BUSY) == HIGH) delay(5); }

void epdReset() {
  digitalWrite(EPD_RST, HIGH); delay(50);
  digitalWrite(EPD_RST, LOW);  delay(20);
  digitalWrite(EPD_RST, HIGH); delay(50);
}

void epdSetWindows(uint16_t Xs, uint16_t Ys, uint16_t Xe, uint16_t Ye) {
  epdCmd(0x44); epdData((Xs >> 3) & 0xFF); epdData((Xe >> 3) & 0xFF);
  epdCmd(0x45); epdData(Ys & 0xFF); epdData((Ys >> 8) & 0xFF); epdData(Ye & 0xFF); epdData((Ye >> 8) & 0xFF);
}
void epdSetCursor(uint16_t Xs, uint16_t Ys) {
  epdCmd(0x4E); epdData(Xs & 0xFF);
  epdCmd(0x4F); epdData(Ys & 0xFF); epdData((Ys >> 8) & 0xFF);
}
void epdSetLut(const uint8_t *lut) {
  epdCmd(0x32); epdWrite(lut, 153, false); epdBusy();
  epdCmd(0x3F); epdData(lut[153]);
  epdCmd(0x03); epdData(lut[154]);
  epdCmd(0x04); epdData(lut[155]); epdData(lut[156]); epdData(lut[157]);
  epdCmd(0x2C); epdData(lut[158]);
}
void epdInit() {
  epdReset();
  epdBusy();
  epdCmd(0x12); epdBusy();                       // SWRESET
  epdCmd(0x01); epdData(0xC7); epdData(0x00); epdData(0x01);  // driver output
  epdCmd(0x11); epdData(0x01);                   // data entry
  epdSetWindows(0, EPD_W - 1, EPD_H - 1, 0);
  epdCmd(0x3C); epdData(0x01);                   // border
  epdCmd(0x18); epdData(0x80);                   // temp sensor
  epdCmd(0x22); epdData(0xB1); epdCmd(0x20);     // load temp/waveform
  epdSetCursor(0, EPD_H - 1);
  epdBusy();
  epdSetLut(WF_Full_1IN54);
}
void epdTurnOn() { epdCmd(0x22); epdData(0xC7); epdCmd(0x20); epdBusy(); }

void epdShowCanvas() {
  epdSetWindows(0, EPD_W - 1, EPD_H - 1, 0);
  epdSetCursor(0, EPD_H - 1);
  epdCmd(0x24);
  // Canvas: Bit1 = Tinte. Panel: Bit1 = weiss. Also invertiert senden.
  epdWrite(canvas.getBuffer(), EPD_BUF_LEN, true);
  epdTurnOn();
}

// ---------- U-Bahn-Daten ----------
struct Line { const char *num; const char *from; const char *to; int stops; };
Line lines[] = {
  {"U1", "Uhlandstrasse",   "Warschauer Str.",  13},
  {"U2", "Ruhleben",        "Pankow",           29},
  {"U3", "Krumme Lanke",    "Warschauer Str.",  24},
  {"U4", "Nollendorfplatz", "Innsbrucker Pl.",   5},
  {"U5", "Hauptbahnhof",    "Hoenow",           26},
  {"U6", "Alt-Tegel",       "Alt-Mariendorf",   29},
  {"U7", "Rathaus Spandau", "Rudow",            40},
  {"U8", "Wittenau",        "Hermannstrasse",   24},
  {"U9", "Osloer Strasse",  "Rathaus Steglitz", 18},
};
const int LINE_COUNT = sizeof(lines) / sizeof(lines[0]);
int idx = 0;

void renderLine(int i) {
  const Line &L = lines[i];
  canvas.fillScreen(BG);

  // Linien-Badge (schwarz, weisse Schrift)
  canvas.fillRoundRect(8, 8, 60, 38, 5, INK);
  canvas.setTextColor(BG);
  canvas.setTextSize(3);
  canvas.setCursor(16, 16);
  canvas.print(L.num);

  // Kopfzeile
  canvas.setTextColor(INK);
  canvas.setTextSize(1);
  canvas.setCursor(78, 14); canvas.print("BERLIN");
  canvas.setCursor(78, 28); canvas.print("U-BAHN");
  canvas.drawLine(8, 52, 192, 52, INK);

  // Schematische Strecke (senkrecht, links)
  const int x = 22, yTop = 74, yBot = 166;
  canvas.drawLine(x, yTop, x, yBot, INK);
  canvas.drawLine(x + 1, yTop, x + 1, yBot, INK);
  for (int k = 0; k < L.stops; k++) {
    int y = yTop + (long)(yBot - yTop) * k / (L.stops - 1);
    canvas.drawCircle(x, y, 2, INK);
  }
  canvas.fillCircle(x, yTop, 5, INK);
  canvas.fillCircle(x, yBot, 5, INK);

  // Beschriftung
  canvas.setCursor(36, 62);  canvas.print("von");
  canvas.setCursor(36, 73);  canvas.print(L.from);
  canvas.setCursor(36, 150); canvas.print("nach");
  canvas.setCursor(36, 161); canvas.print(L.to);
  canvas.setCursor(8, 184);  canvas.print(L.stops); canvas.print(" Stationen");

  epdShowCanvas();
}

void setup() {
  Serial.begin(115200);

  pinMode(EPD_PWR, OUTPUT); digitalWrite(EPD_PWR, LOW); delay(100);  // Display EIN
  pinMode(EPD_CS, OUTPUT);  digitalWrite(EPD_CS, HIGH);
  pinMode(EPD_DC, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  pinMode(EPD_BUSY, INPUT);

  SPI.begin(EPD_SCK, -1, EPD_MOSI, -1);   // CS/DC von Hand gesteuert
  SPI.beginTransaction(spiSet);

  epdInit();
  renderLine(idx);
}

void loop() {
  delay(15000);
  idx = (idx + 1) % LINE_COUNT;
  renderLine(idx);
}
