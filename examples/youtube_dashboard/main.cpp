// ===========================================================================
//  ESP32-2432S028R  "Cheap Yellow Display" (CYD)
//  YouTube channel statistics dashboard
//
//  Shows live subscriber / view / video counts for a channel on the 2.8" TFT.
//  Tap the screen to switch to the next channel; stats auto-refresh too.
//
//  NOTE: This is a *stats display*, not a video player. An ESP32 cannot decode
//        or stream YouTube video. This is the achievable "YouTube on CYD".
// ===========================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "config.h"

// ---------------------------------------------------------------------------
//  Touchscreen — on the CYD the XPT2046 sits on its OWN SPI bus (not shared
//  with the display), so we drive it with a separate SPIClass instance.
// ---------------------------------------------------------------------------
#define XPT2046_CLK   25
#define XPT2046_MISO  39
#define XPT2046_MOSI  32
#define XPT2046_CS    33
#define XPT2046_IRQ   36

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

TFT_eSPI tft = TFT_eSPI();

// ---------------------------------------------------------------------------
//  Colour palette (YouTube-ish)
// ---------------------------------------------------------------------------
#define COL_BG      TFT_BLACK
#define COL_RED     0xF800          // YouTube red
#define COL_WHITE   TFT_WHITE
#define COL_GREY    0x8410
#define COL_CARD    0x1082          // dark card background

// ---------------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------------
const size_t CHANNEL_COUNT = sizeof(CHANNELS) / sizeof(CHANNELS[0]);
size_t        currentChannel = 0;
unsigned long lastRefresh     = 0;
bool          touchWasDown    = false;

struct Stats {
  bool     ok = false;
  uint64_t subs = 0;
  uint64_t views = 0;
  uint64_t videos = 0;
  String   error;
};

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

// Format a number with thousands separators: 1234567 -> "1,234,567"
static String groupThousands(uint64_t n) {
  String s = String((unsigned long long)n);
  String out;
  int c = 0;
  for (int i = s.length() - 1; i >= 0; --i) {
    out = s[i] + out;
    if (++c % 3 == 0 && i > 0) out = "," + out;
  }
  return out;
}

// Compact form for very large numbers: 1234567 -> "1.23M"
static String humanCount(uint64_t n) {
  if (n >= 1000000000ULL) return String(n / 1e9, 2) + "B";
  if (n >= 1000000ULL)    return String(n / 1e6, 2) + "M";
  if (n >= 1000ULL)       return String(n / 1e3, 1) + "K";
  return String((unsigned long)n);
}

static void connectWiFi() {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_WHITE, COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Connecting WiFi...", tft.width() / 2, tft.height() / 2, 4);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
  }

  tft.fillScreen(COL_BG);
  if (WiFi.status() != WL_CONNECTED) {
    tft.setTextColor(COL_RED, COL_BG);
    tft.drawString("WiFi failed", tft.width() / 2, tft.height() / 2, 4);
    delay(2000);
  }
}

// Query the YouTube Data API for one channel's statistics.
static Stats fetchStats(const char *channelId) {
  Stats st;

  if (WiFi.status() != WL_CONNECTED) {
    st.error = "No WiFi";
    return st;
  }

  String url = String("https://www.googleapis.com/youtube/v3/channels")
             + "?part=statistics&id=" + channelId
             + "&key=" + YT_API_KEY;

  WiFiClientSecure client;
  client.setInsecure();   // skip cert validation (simple hobby setup)

  HTTPClient http;
  http.begin(client, url);
  int code = http.GET();

  if (code != 200) {
    st.error = "HTTP " + String(code);
    http.end();
    return st;
  }

  // Only pull the fields we need — keeps the JSON doc small.
  StaticJsonDocument<256> filter;
  filter["items"][0]["statistics"]["subscriberCount"] = true;
  filter["items"][0]["statistics"]["viewCount"]       = true;
  filter["items"][0]["statistics"]["videoCount"]      = true;

  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, http.getStream(),
                      DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    st.error = String("JSON: ") + err.c_str();
    return st;
  }

  JsonObject s = doc["items"][0]["statistics"];
  if (s.isNull()) {
    st.error = "No such channel";
    return st;
  }

  st.subs   = strtoull(s["subscriberCount"] | "0", nullptr, 10);
  st.views  = strtoull(s["viewCount"]       | "0", nullptr, 10);
  st.videos = strtoull(s["videoCount"]      | "0", nullptr, 10);
  st.ok     = true;
  return st;
}

// ---------------------------------------------------------------------------
//  Drawing
// ---------------------------------------------------------------------------
static void drawStatCard(int y, int h, const char *title,
                         const String &value, uint16_t accent) {
  const int m = 10;
  tft.fillRoundRect(m, y, tft.width() - 2 * m, h, 8, COL_CARD);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_GREY, COL_CARD);
  tft.drawString(title, m + 12, y + 8, 2);
  tft.setTextColor(accent, COL_CARD);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(value, m + 12, y + 26, 6);
}

static void render(const char *label, const Stats &st) {
  tft.fillScreen(COL_BG);

  // Header bar
  tft.fillRect(0, 0, tft.width(), 34, COL_RED);
  tft.setTextColor(COL_WHITE, COL_RED);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(label, 10, 17, 4);

  if (!st.ok) {
    tft.setTextColor(COL_RED, COL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(st.error.length() ? st.error : "Error",
                   tft.width() / 2, tft.height() / 2, 4);
    return;
  }

  int y = 44;
  drawStatCard(y, 74, "SUBSCRIBERS", humanCount(st.subs), COL_WHITE);
  y += 82;
  drawStatCard(y, 60, "VIEWS", groupThousands(st.views), COL_GREY);
  y += 68;
  drawStatCard(y, 60, "VIDEOS", groupThousands(st.videos), COL_GREY);

  // Footer hint
  tft.setTextColor(COL_GREY, COL_BG);
  tft.setTextDatum(BC_DATUM);
  tft.drawString("tap = next channel", tft.width() / 2, tft.height() - 4, 2);
}

static void refresh() {
  const YtChannel &ch = CHANNELS[currentChannel];

  // brief "loading" hint on the header
  tft.fillRect(0, 0, tft.width(), 34, COL_RED);
  tft.setTextColor(COL_WHITE, COL_RED);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("...", tft.width() - 10, 17, 4);

  Stats st = fetchStats(ch.id);
  render(ch.label, st);
  lastRefresh = millis();
}

// ---------------------------------------------------------------------------
//  Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0);            // portrait 240x320
  tft.fillScreen(COL_BG);

  // Backlight (build flag TFT_BL=21 already drives it, but be explicit)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(0);

  connectWiFi();
  refresh();
}

void loop() {
  // --- touch: rising edge => next channel ---
  bool down = ts.touched();
  if (down && !touchWasDown) {
    currentChannel = (currentChannel + 1) % CHANNEL_COUNT;
    refresh();
    delay(250);                 // simple debounce
  }
  touchWasDown = down;

  // --- periodic auto-refresh ---
  if (millis() - lastRefresh >= REFRESH_INTERVAL_MS) {
    refresh();
  }

  delay(20);
}
