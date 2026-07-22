// ===========================================================================
//  ESP32-2432S028R  "Cheap Yellow Display" (CYD)
//  Telegram lead notifier
//
//  Turns the CYD into a physical lead ticker: it long-polls the Telegram Bot
//  API over WiFi and shows every message that arrives at @vollblutclaudeBot as
//  a big "NEUER LEAD" card on the 2.8" TFT, with a beep + green LED flash.
//  Tap the screen to scroll back through recent leads.
//
//  Secrets live in include/config.h (git-ignored) — see include/config.h.example
// ===========================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "config.h"

// ---------------------------------------------------------------------------
//  Board peripherals (fixed CYD wiring)
// ---------------------------------------------------------------------------
// Touch (XPT2046) sits on its OWN SPI bus on the CYD.
#define XPT2046_CLK   25
#define XPT2046_MISO  39
#define XPT2046_MOSI  32
#define XPT2046_CS    33
#define XPT2046_IRQ   36
// Onboard RGB LED (common anode -> active LOW) and speaker.
#define LED_R   4
#define LED_G  16
#define LED_B  17
#define SPEAKER_PIN 26

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();
Preferences prefs;

// ---------------------------------------------------------------------------
//  Colours
// ---------------------------------------------------------------------------
#define COL_BG      TFT_BLACK
#define COL_ACCENT  0x001F      // deep blue header (Telegram-ish)
#define COL_GOOD    0x07E0      // green
#define COL_WHITE   TFT_WHITE
#define COL_GREY    0x8410

// ---------------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------------
struct Lead {
  String name;
  String username;
  String text;
  String when;
};

Lead    history[LEAD_HISTORY];
int     histFilled  = 0;          // how many slots are populated
int     histHead    = 0;          // index of the newest lead
long    totalLeads  = 0;          // running counter shown as "#N"
long    nextOffset  = 0;          // Telegram getUpdates offset (next update_id)
int     viewOffset  = 0;          // 0 = newest; grows as you scroll back
bool    touchWasDown = false;
int     W, H;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
static void ledGreen(bool on) {
#if ENABLE_LED
  digitalWrite(LED_G, on ? LOW : HIGH);   // active LOW
#endif
}

static void alertNewLead() {
  ledGreen(true);
#if ENABLE_BEEP
  tone(SPEAKER_PIN, 2100, 120); delay(150);
  tone(SPEAKER_PIN, 2700, 120); delay(150);
  noTone(SPEAKER_PIN);
#else
  delay(250);
#endif
  ledGreen(false);
}

static String nowClock() {
  struct tm t;
  if (!getLocalTime(&t, 50)) return "";
  char buf[6];
  strftime(buf, sizeof(buf), "%H:%M", &t);
  return String(buf);
}

// ---------------------------------------------------------------------------
//  Drawing
// ---------------------------------------------------------------------------
static void drawIdle() {
  tft.fillScreen(COL_BG);
  tft.fillRect(0, 0, W, 30, COL_ACCENT);
  tft.setTextColor(COL_WHITE, COL_ACCENT);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("VOLLBLUT CLAUDE", 8, 15, 4);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("#" + String(totalLeads), W - 8, 15, 4);

  tft.setTextColor(COL_GREY, COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Warte auf Leads...", W / 2, H / 2 - 10, 4);
  tft.setTextColor(COL_GOOD, COL_BG);
  tft.drawString(WiFi.localIP().toString(), W / 2, H / 2 + 24, 2);
}

static void drawLead(int viewIdx) {
  if (histFilled == 0) { drawIdle(); return; }
  // viewIdx 0 = newest; walk backwards through the ring buffer
  int idx = (histHead - viewIdx + LEAD_HISTORY * 2) % LEAD_HISTORY;
  Lead &l = history[idx];

  tft.fillScreen(COL_BG);

  // Header
  uint16_t hcol = (viewIdx == 0) ? COL_GOOD : COL_ACCENT;
  tft.fillRect(0, 0, W, 30, hcol);
  tft.setTextColor(COL_WHITE, hcol);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(viewIdx == 0 ? "NEUER LEAD" : "LEAD", 8, 15, 4);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("#" + String(totalLeads - viewIdx), W - 8, 15, 4);

  // Sender
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_WHITE, COL_BG);
  tft.drawString(l.name.length() ? l.name : "Unbekannt", 10, 38, 4);

  String sub;
  if (l.username.length()) sub += "@" + l.username + "   ";
  sub += l.when;
  tft.setTextColor(COL_GREY, COL_BG);
  tft.drawString(sub, 10, 64, 2);

  tft.drawFastHLine(10, 84, W - 20, COL_GREY);

  // Message body (word-wrapped)
  tft.setTextColor(COL_WHITE, COL_BG);
  tft.setTextWrap(true);
  tft.setTextFont(4);
  tft.setCursor(10, 92);
  tft.print(l.text.length() ? l.text : "(kein Text)");

  // Footer hint
  if (histFilled > 1) {
    tft.setTextColor(COL_GREY, COL_BG);
    tft.setTextDatum(BR_DATUM);
    tft.drawString("tap = zurueckblaettern", W - 6, H - 4, 2);
  }
}

// ---------------------------------------------------------------------------
//  Telegram
// ---------------------------------------------------------------------------
static String describeMessage(JsonObject msg) {
  if (msg["text"].is<const char *>())    return msg["text"].as<String>();
  if (msg["caption"].is<const char *>()) return msg["caption"].as<String>();
  if (!msg["contact"].isNull()) {
    JsonObject c = msg["contact"];
    return String("Kontakt: ") + (c["first_name"] | "") + " " +
           (c["phone_number"] | "");
  }
  if (!msg["location"].isNull())
    return String("Standort: ") + (msg["location"]["latitude"] | 0.0) + ", " +
           (msg["location"]["longitude"] | 0.0);
  if (!msg["photo"].isNull())    return "[Foto]";
  if (!msg["document"].isNull()) return "[Datei]";
  if (!msg["voice"].isNull())    return "[Sprachnachricht]";
  return "[Nachricht]";
}

static void storeLead(JsonObject msg, bool announce) {
  JsonObject from = msg["from"];
  String name = String(from["first_name"] | "");
  if (from["last_name"].is<const char *>()) name += " " + String(from["last_name"].as<const char *>());

  Lead l;
  l.name     = name;
  l.username = String(from["username"] | "");
  l.text     = describeMessage(msg);
  if (l.text.length() > 280) l.text = l.text.substring(0, 277) + "...";
  l.when     = nowClock();

  histHead = (histFilled == 0) ? 0 : (histHead + 1) % LEAD_HISTORY;
  history[histHead] = l;
  if (histFilled < LEAD_HISTORY) histFilled++;
  totalLeads++;

  if (announce) {
    viewOffset = 0;
    drawLead(0);
    alertNewLead();
  }
}

// Returns number of updates processed.
static int telegramPoll(bool announce) {
  if (WiFi.status() != WL_CONNECTED) return 0;

  WiFiClientSecure client;
  client.setInsecure();               // skip cert store (simple hobby setup)

  HTTPClient http;
  http.setTimeout((POLL_TIMEOUT_S + 5) * 1000);
  String url = String("https://api.telegram.org/bot") + TELEGRAM_BOT_TOKEN +
               "/getUpdates?limit=5&timeout=" + POLL_TIMEOUT_S +
               "&offset=" + String(nextOffset);

  if (!http.begin(client, url)) return 0;
  int code = http.GET();
  if (code != 200) { http.end(); return 0; }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) return 0;

  int n = 0;
  for (JsonObject upd : doc["result"].as<JsonArray>()) {
    long uid = upd["update_id"] | 0L;
    nextOffset = uid + 1;

    JsonObject msg = upd["message"];
    if (msg.isNull()) msg = upd["channel_post"].as<JsonObject>();
    if (!msg.isNull()) { storeLead(msg, announce); n++; }
  }
  if (n > 0) prefs.putLong("offset", nextOffset);
  return n;
}

// ---------------------------------------------------------------------------
//  WiFi
// ---------------------------------------------------------------------------
static void connectWiFi() {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_WHITE, COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Verbinde WLAN...", W / 2, H / 2, 4);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(300);
}

// ---------------------------------------------------------------------------
//  Setup / loop
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

#if ENABLE_LED
  pinMode(LED_R, OUTPUT); pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R, HIGH); digitalWrite(LED_G, HIGH); digitalWrite(LED_B, HIGH);
#endif

  tft.init();
  tft.setRotation(1);            // landscape 320x240 (more room for text)
  W = tft.width(); H = tft.height();
  pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  prefs.begin("leads", false);
  nextOffset = prefs.getLong("offset", 0);

  connectWiFi();

  // Time (for lead timestamps)
  configTime(GMT_OFFSET_HOURS * 3600, 0, "pool.ntp.org", "time.google.com");

  // Drain any backlog silently so we only alert on leads arriving from now on.
  if (nextOffset == 0) {
    while (telegramPoll(false) > 0) { /* advance offset without alerting */ }
    totalLeads = 0; histFilled = 0;   // don't count drained backlog
  }

  drawIdle();
}

void loop() {
  // Touch: tap -> scroll back one lead (rising edge).
  bool down = ts.touched();
  if (down && !touchWasDown && histFilled > 0) {
    viewOffset = (viewOffset + 1) % histFilled;
    drawLead(viewOffset);
    delay(200);                        // debounce
  }
  touchWasDown = down;

  // Reconnect WiFi if dropped.
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    return;
  }

  // Long-poll Telegram; new leads render + alert inside storeLead().
  telegramPoll(true);
}
