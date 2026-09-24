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
 * Der ESP32 spannt ein eigenes WLAN auf (Access Point). Verbinde dich mit
 * dem WLAN und oeffne die angezeigte Adresse im Browser - dann laeuft die
 * interaktive Karte mit fahrenden Zuegen direkt vom ESP32.
 *
 * WICHTIG: Diese Datei wird AUTOMATISCH aus index.html erzeugt.
 *          Nicht von Hand editieren - stattdessen index.html aendern und
 *          "node esp32/build_sketch.js" neu ausfuehren.
 *
 * Board:     ESP32 (Arduino IDE -> Tools -> Board -> "ESP32 Dev Module")
 * Bibliothek: WiFi + WebServer (im ESP32-Core enthalten, nichts extra noetig)
 */
#include <WiFi.h>
#include <WebServer.h>

// ---- WLAN-Zugangsdaten des Access Points (nach Belieben aendern) ----
const char *AP_SSID = "Berlin-UBahn";   // Name des WLANs, das der ESP32 aufspannt
const char *AP_PASS = "ubahn1234";      // mind. 8 Zeichen; "" = offenes WLAN

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

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();   // Standard: 192.168.4.1

  Serial.println();
  Serial.println(ok ? "Access Point gestartet." : "Access Point FEHLER!");
  Serial.print("  WLAN-Name (SSID): "); Serial.println(AP_SSID);
  Serial.print("  Passwort:         "); Serial.println(AP_PASS);
  Serial.print("  Im Browser oeffnen: http://"); Serial.println(ip);

  // Jede Adresse liefert die App aus (bequem als "Captive Portal").
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
