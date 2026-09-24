#!/usr/bin/env node
/*
 * Generiert esp32/BerlinUBahn/BerlinUBahn.ino aus der index.html im Repo-Root.
 * Die komplette Web-App wird als PROGMEM-Rohstring in den Sketch eingebettet,
 * damit der ESP32 sie ueber einen eigenen WLAN-Access-Point ausliefern kann.
 *
 *   node esp32/build_sketch.js
 */
const fs = require("fs");
const path = require("path");

const root = path.resolve(__dirname, "..");
const html = fs.readFileSync(path.join(root, "index.html"), "utf8");

// Rohstring-Delimiter waehlen, der garantiert nicht im HTML vorkommt.
let delim = "UBAHN";
while (html.includes(")" + delim + "\"")) delim += "X";

const sketch = `/*
 * Berliner U-Bahn - Live-Netzplan als ESP32 WLAN-Webserver
 * ---------------------------------------------------------
 * Der ESP32 waehlt sich in dein Heim-WLAN ein (Station-Modus) und liefert
 * die interaktive Karte mit fahrenden Zuegen als Webseite aus. Du oeffnest
 * die im seriellen Monitor angezeigte Adresse (oder http://ubahn.local).
 *
 * WICHTIG: Diese Datei wird AUTOMATISCH aus index.html erzeugt.
 *          Nicht von Hand editieren - stattdessen index.html aendern und
 *          "node esp32/build_sketch.js" neu ausfuehren.
 *          AUSNAHME: WIFI_SSID/WIFI_PASS unten darfst/musst du ausfuellen.
 *
 * >>> VOR DEM FLASHEN: WIFI_SSID und WIFI_PASS unten eintragen! <<<
 * (Dein Passwort NICHT ins oeffentliche Git committen - lokal lassen.)
 *
 * Board:     ESP32 (Arduino IDE -> Tools -> Board -> "ESP32 Dev Module")
 * Bibliothek: WiFi + WebServer + ESPmDNS (alle im ESP32-Core enthalten)
 */
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ====== HIER DEIN WLAN EINTRAGEN ======
const char *WIFI_SSID = "DEIN_WLAN_NAME";       // Name deines Heim-WLANs
const char *WIFI_PASS = "DEIN_WLAN_PASSWORT";   // WLAN-Passwort
// ======================================

// Fallback: falls das Heim-WLAN nicht erreichbar ist, macht der ESP32
// ersatzweise sein eigenes WLAN auf, damit er nie unerreichbar ist.
const char *AP_SSID = "Berlin-UBahn";
const char *AP_PASS = "ubahn1234";

const char *HOSTNAME = "ubahn";   // erreichbar als http://ubahn.local

WebServer server(80);

// ---- Die komplette Web-App (aus index.html eingebettet) ----
const char INDEX_HTML[] PROGMEM = R"${delim}(
${html}
)${delim}";

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // 1) Versuchen, ins Heim-WLAN einzuwaehlen.
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println();
  Serial.print("Verbinde mit WLAN \\""); Serial.print(WIFI_SSID); Serial.print("\\" ");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500); Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WLAN verbunden.");
    Serial.print("  Im Browser oeffnen: http://"); Serial.println(WiFi.localIP());
    if (MDNS.begin(HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.print("  oder bequem:        http://"); Serial.print(HOSTNAME); Serial.println(".local");
    }
  } else {
    // 2) Fallback: eigenes WLAN aufspannen.
    Serial.println("WLAN nicht erreichbar - starte eigenen Access Point.");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("  WLAN-Name (SSID): "); Serial.println(AP_SSID);
    Serial.print("  Passwort:         "); Serial.println(AP_PASS);
    Serial.print("  Im Browser oeffnen: http://"); Serial.println(WiFi.softAPIP());
  }

  // Jede Adresse liefert die App aus.
  server.on("/", handleRoot);
  server.onNotFound(handleRoot);
  server.begin();
  Serial.println("Webserver laeuft. Bereit.");
}

void loop() {
  server.handleClient();
}
`;

const outDir = path.join(root, "esp32", "BerlinUBahn");
fs.mkdirSync(outDir, { recursive: true });
const outFile = path.join(outDir, "BerlinUBahn.ino");
fs.writeFileSync(outFile, sketch);
console.log("Sketch geschrieben:", path.relative(root, outFile));
console.log("Groesse:", sketch.length, "Bytes (HTML:", html.length, "Bytes)");
console.log("Delimiter:", delim);
