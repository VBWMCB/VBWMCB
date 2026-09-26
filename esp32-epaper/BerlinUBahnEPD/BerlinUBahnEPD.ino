/*
 * Berliner U-Bahn auf dem ESP32-S3 E-Paper 1.54" (200x200, Schwarz/Weiss)
 * -----------------------------------------------------------------------
 * Zeigt der Reihe nach jede U-Bahn-Linie (U1..U9): grosse Liniennummer,
 * Start- und Zielbahnhof ("von / nach"), Stationszahl und eine
 * schematische Strecke. Wechselt automatisch alle ~15 Sekunden weiter.
 *
 * Board:      "ESP32S3 Dev Module"  (Arduino IDE)
 * Panel:      Waveshare 1.54" 200x200 s/w (Controller SSD1681)
 * Bibliothek: GxEPD2 (installiert automatisch Adafruit GFX + BusIO)
 * Pins:       aus Waveshares offizieller user_config.h fuer dieses Board.
 *
 * Falls das Display leer bleibt: siehe Hinweis unten bei DISPLAY-TYP.
 */
#include <GxEPD2_BW.h>

// ---- Pins dieses Boards (aus Waveshare user_config.h) ----
#define EPD_PWR   6    // schaltet die Display-Stromversorgung
#define EPD_BUSY  8
#define EPD_RST   9
#define EPD_DC    10
#define EPD_CS    11
#define EPD_SCK   12
#define EPD_MOSI  13

// ---- DISPLAY-TYP ----
// Standard fuer das aktuelle 1.54"-200x200-Modul ist SSD1681 (GxEPD2_154_D67).
// Bleibt der Schirm leer, ersetze GxEPD2_154_D67 durch GxEPD2_154 (aelteres Panel).
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

struct Line { const char *num; const char *from; const char *to; int stops; };

// Umlaute bewusst als ae/oe/ue/ss - der Standard-Font kann keine Umlaute.
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

void drawScreen(const Line &L) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Linien-Badge oben links
    display.fillRoundRect(8, 8, 60, 38, 5, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setTextSize(3);
    display.setCursor(16, 16);
    display.print(L.num);

    // Kopfzeile
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(1);
    display.setCursor(78, 14); display.print("BERLIN");
    display.setCursor(78, 28); display.print("U-BAHN");
    display.drawLine(8, 52, 192, 52, GxEPD_BLACK);

    // Schematische Strecke (senkrecht, links)
    const int x = 22, yTop = 74, yBot = 166;
    display.drawLine(x, yTop, x, yBot, GxEPD_BLACK);
    display.drawLine(x + 1, yTop, x + 1, yBot, GxEPD_BLACK);
    for (int k = 0; k < L.stops; k++) {
      int y = yTop + (long)(yBot - yTop) * k / (L.stops - 1);
      display.drawCircle(x, y, 2, GxEPD_BLACK);
    }
    display.fillCircle(x, yTop, 5, GxEPD_BLACK);   // Start
    display.fillCircle(x, yBot, 5, GxEPD_BLACK);   // Ziel

    // Beschriftung
    display.setTextSize(1);
    display.setCursor(36, 62);  display.print("von");
    display.setCursor(36, 73);  display.print(L.from);
    display.setCursor(36, 150); display.print("nach");
    display.setCursor(36, 161); display.print(L.to);

    display.setCursor(8, 184);
    display.print(L.stops); display.print(" Stationen");
  } while (display.nextPage());
}

void setup() {
  Serial.begin(115200);

  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, HIGH);   // Display mit Strom versorgen
  delay(100);

  // SPI auf die Pins dieses Boards legen (MISO nicht genutzt -> -1)
  SPI.end();
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);

  display.init(115200);
  display.setRotation(0);

  drawScreen(lines[idx]);
}

void loop() {
  delay(15000);                        // ~15 s pro Linie
  idx = (idx + 1) % LINE_COUNT;
  drawScreen(lines[idx]);
}
