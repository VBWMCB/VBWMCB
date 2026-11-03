/*
 * Berliner U-Bahn LIVE auf dem Waveshare ESP32-S3 E-Paper 1.54" (200x200 s/w)
 * ---------------------------------------------------------------------------
 * Holt ueber WLAN die ECHTEN naechsten Abfahrten von der offenen BVG-API
 * (v6.bvg.transport.rest) und zeigt sie an. Stationen wechseln automatisch
 * (oder per BOOT-Taste). Uhrzeit kommt per NTP (echte Zeit).
 *
 * WLAN-Daten: WIFI_SSID / WIFI_PASS unten (werden vom Flash-Skript lokal
 * eingetragen, NICHT ins oeffentliche Git committet).
 *
 * Board: "ESP32S3 Dev Module". Bibliotheken: Adafruit GFX, ArduinoJson (v7).
 * Treiber/Pins: Waveshare epaper_driver_bsp.cpp (SSD1681, eigene LUT).
 */
#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// ====== HIER DEIN WLAN (Flash-Skript fuellt das lokal) ======
const char *WIFI_SSID = "__WIFI_SSID__";
const char *WIFI_PASS = "__WIFI_PASS__";
// ============================================================

// ---- Pins dieses Boards (Waveshare user_config.h) ----
#define EPD_PWR   6
#define EPD_BUSY  8
#define EPD_RST   9
#define EPD_DC    10
#define EPD_CS    11
#define EPD_SCK   12
#define EPD_MOSI  13
#define BTN_BOOT  0

#define EPD_W 200
#define EPD_H 200
#define EPD_ROW_BYTES (EPD_W / 8)
#define EPD_BUF_LEN (EPD_ROW_BYTES * EPD_H)
#define INK 1
#define BG  0

GFXcanvas1 canvas(EPD_W, EPD_H);

// Waveform-LUT (aus Waveshares epaper_driver_bsp.cpp)
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

// ---------- E-Paper Low-Level (Waveshare-kompatibel) ----------
static inline void epdCmd(uint8_t c)  { digitalWrite(EPD_DC, LOW);  digitalWrite(EPD_CS, LOW); SPI.transfer(c); digitalWrite(EPD_CS, HIGH); }
static inline void epdData(uint8_t d) { digitalWrite(EPD_DC, HIGH); digitalWrite(EPD_CS, LOW); SPI.transfer(d); digitalWrite(EPD_CS, HIGH); }
void epdWrite(const uint8_t *buf, int len, bool invert) {
  digitalWrite(EPD_DC, HIGH); digitalWrite(EPD_CS, LOW);
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
  epdReset(); epdBusy();
  epdCmd(0x12); epdBusy();
  epdCmd(0x01); epdData(0xC7); epdData(0x00); epdData(0x01);
  epdCmd(0x11); epdData(0x01);
  epdSetWindows(0, EPD_W - 1, EPD_H - 1, 0);
  epdCmd(0x3C); epdData(0x01);
  epdCmd(0x18); epdData(0x80);
  epdCmd(0x22); epdData(0xB1); epdCmd(0x20);
  epdSetCursor(0, EPD_H - 1);
  epdBusy();
  epdSetLut(WF_Full_1IN54);
}
void epdTurnOn() { epdCmd(0x22); epdData(0xC7); epdCmd(0x20); epdBusy(); }
void epdShowCanvas() {
  epdSetWindows(0, EPD_W - 1, EPD_H - 1, 0);
  epdSetCursor(0, EPD_H - 1);
  epdCmd(0x24);
  epdWrite(canvas.getBuffer(), EPD_BUF_LEN, true);
  epdTurnOn();
}

// ---------- Stationen ----------
struct Stn { const char *name; const char *query; };
Stn stations[] = {
  { "Alexanderplatz",  "Alexanderplatz" },
  { "Zoolog. Garten",  "Zoologischer Garten" },
  { "Hauptbahnhof",    "Berlin Hauptbahnhof" },
  { "Hermannplatz",    "Hermannplatz" },
};
const int STN_COUNT = sizeof(stations) / sizeof(stations[0]);
int idx = 0;
String stopId[8];       // zur Laufzeit aufgeloeste IDs (Cache)
bool wifiOK = false;

struct Dep { String line, dir, when; int mins; };
Dep dep[6];
int depCount = 0;

// ---------- Zeit / Uhr ----------
bool timeSynced() { return time(nullptr) > 1600000000; }
int minsUntil(const String &iso) {
  int t = iso.indexOf('T');
  if (t < 0 || !timeSynced()) return -1;
  int hh = iso.substring(t + 1, t + 3).toInt();
  int mm = iso.substring(t + 4, t + 6).toInt();
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  int diff = (hh * 60 + mm) - (lt.tm_hour * 60 + lt.tm_min);
  if (diff < -60) diff += 1440;
  return diff;
}
void drawClock() {
  if (!timeSynced()) return;
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  char buf[6]; snprintf(buf, sizeof(buf), "%02d:%02d", lt.tm_hour, lt.tm_min);
  canvas.setFont(&FreeSans9pt7b); canvas.setTextColor(INK);
  int16_t bx, by; uint16_t bw, bh;
  canvas.getTextBounds(buf, 0, 0, &bx, &by, &bw, &bh);
  canvas.setCursor(196 - bw, 15); canvas.print(buf);
}

// ---------- HTTP / API ----------
String urlencode(const String &s) {
  String o;
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == ' ') o += "%20";
    else if (isalnum((unsigned char)c)) o += c;
    else { char b[4]; snprintf(b, sizeof(b), "%%%02X", (uint8_t)c); o += b; }
  }
  return o;
}
String httpGet(const String &url) {
  WiFiClientSecure client; client.setInsecure();
  HTTPClient https; https.setUserAgent("berlin-ubahn-epd");
  if (!https.begin(client, url)) return "";
  int code = https.GET();
  String out = (code == 200) ? https.getString() : String();
  https.end();
  return out;
}
String resolveStopId(const char *query) {
  String url = "https://v6.bvg.transport.rest/locations?poi=false&addresses=false&results=1&query=" + urlencode(query);
  String p = httpGet(url);
  if (!p.length()) return "";
  JsonDocument filter; filter[0]["id"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, p, DeserializationOption::Filter(filter))) return "";
  const char *id = doc[0]["id"] | "";
  return String(id);
}
void fetchDepartures(int i) {
  depCount = 0;
  if (!wifiOK) return;
  if (!stopId[i].length()) stopId[i] = resolveStopId(stations[i].query);
  if (!stopId[i].length()) return;
  String url = "https://v6.bvg.transport.rest/stops/" + stopId[i] +
               "/departures?results=6&duration=60&suburban=false&subway=true&tram=false&bus=false&ferry=false&express=false&regional=false";
  String p = httpGet(url);
  if (!p.length()) return;
  JsonDocument filter;
  filter["departures"][0]["when"] = true;
  filter["departures"][0]["plannedWhen"] = true;
  filter["departures"][0]["direction"] = true;
  filter["departures"][0]["line"]["name"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, p, DeserializationOption::Filter(filter))) return;
  for (JsonObject d : doc["departures"].as<JsonArray>()) {
    if (depCount >= 6) break;
    const char *when = d["when"] | "";
    if (!when[0]) when = d["plannedWhen"] | "";
    Dep e;
    e.line = (const char *)(d["line"]["name"] | "?");
    e.dir  = (const char *)(d["direction"] | "");
    String w = when; int t = w.indexOf('T');
    e.when = (t >= 0) ? w.substring(t + 1, t + 6) : "";
    e.mins = minsUntil(w);
    dep[depCount++] = e;
  }
}

// ---------- Anzeige ----------
void renderMessage(const char *l1, const char *l2) {
  canvas.setFont(NULL);
  canvas.fillScreen(BG);
  canvas.setTextColor(INK); canvas.setTextWrap(true);
  canvas.setFont(&FreeSansBold12pt7b); canvas.setCursor(6, 80);  canvas.print(l1);
  canvas.setFont(&FreeSans9pt7b);      canvas.setCursor(6, 112); canvas.print(l2);
  canvas.setFont(NULL);
  epdShowCanvas();
}
void renderDepartures(int i) {
  canvas.setFont(NULL);
  canvas.fillScreen(BG);
  canvas.setTextColor(INK); canvas.setTextWrap(false); canvas.setTextSize(1);

  // Kopf: Stationsname + Uhr
  canvas.setFont(&FreeSansBold12pt7b); canvas.setCursor(4, 20); canvas.print(stations[i].name);
  drawClock();
  canvas.drawLine(4, 28, 196, 28, INK);

  canvas.setFont(&FreeSans9pt7b);
  if (depCount == 0) {
    canvas.setCursor(4, 60);
    canvas.print(wifiOK ? "keine Abfahrten" : "kein WLAN");
    canvas.setFont(NULL); epdShowCanvas(); return;
  }
  int y = 50;
  for (int k = 0; k < depCount && k < 5; k++) {
    canvas.setCursor(2, y); canvas.print(dep[k].line);          // z.B. U8
    String d = dep[k].dir;
    if (d.length() > 13) d = d.substring(0, 12) + ".";
    canvas.setCursor(42, y); canvas.print(d);                   // Richtung
    char mb[8];
    if (dep[k].mins >= 0 && dep[k].mins <= 99)
      (dep[k].mins == 0) ? strcpy(mb, "jetzt") : snprintf(mb, sizeof(mb), "%d'", dep[k].mins);
    else
      snprintf(mb, sizeof(mb), "%s", dep[k].when.c_str());      // sonst Uhrzeit
    int16_t bx, by; uint16_t bw, bh;
    canvas.getTextBounds(mb, 0, 0, &bx, &by, &bw, &bh);
    canvas.setCursor(198 - bw, y); canvas.print(mb);            // Minuten rechtsbuendig
    y += 30;
  }
  canvas.setFont(NULL);
  epdShowCanvas();
}

void showStation(int i) {
  renderDepartures(i);   // alte Daten/Platzhalter sofort zeigen waere moeglich; wir holen frisch:
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);

  pinMode(EPD_PWR, OUTPUT); digitalWrite(EPD_PWR, LOW); delay(100);
  pinMode(EPD_CS, OUTPUT);  digitalWrite(EPD_CS, HIGH);
  pinMode(EPD_DC, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  pinMode(EPD_BUSY, INPUT);
  pinMode(BTN_BOOT, INPUT_PULLUP);

  SPI.begin(EPD_SCK, -1, EPD_MOSI, -1);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  epdInit();

  renderMessage("Berlin U-Bahn", "Verbinde WLAN ...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(250);
  wifiOK = (WiFi.status() == WL_CONNECTED);

  if (wifiOK) {
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");
    renderMessage("WLAN verbunden", "Lade Abfahrten ...");
    // kurz auf NTP warten (max 5 s)
    unsigned long n0 = millis();
    while (!timeSynced() && millis() - n0 < 5000) delay(200);
    fetchDepartures(idx);
    renderDepartures(idx);
  } else {
    renderMessage("Kein WLAN!", "SSID/Passwort pruefen");
  }
}

void loop() {
  unsigned long start = millis();
  while (millis() - start < 25000) {
    if (digitalRead(BTN_BOOT) == LOW) {                 // naechste Station sofort
      idx = (idx + 1) % STN_COUNT;
      fetchDepartures(idx);
      renderDepartures(idx);
      start = millis();
      while (digitalRead(BTN_BOOT) == LOW) delay(10);
    }
    delay(20);
  }
  idx = (idx + 1) % STN_COUNT;   // automatisch zur naechsten Station, frische Daten
  fetchDepartures(idx);
  renderDepartures(idx);
}
