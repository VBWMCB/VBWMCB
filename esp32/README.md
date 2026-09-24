# Berliner U-Bahn auf dem ESP32

Der ESP32 spannt ein eigenes WLAN auf und liefert die interaktive U-Bahn-Karte
(mit fahrenden Zügen) als Webseite aus. Du verbindest dich mit dem WLAN des
ESP32 und öffnest die Adresse im Browser – kein Router und kein Internet nötig.

## Dateien

- `BerlinUBahn/BerlinUBahn.ino` – fertiger Arduino-Sketch (App ist eingebettet)
- `build_sketch.js` – erzeugt den Sketch neu aus der `index.html` im Repo-Root

## Flashen mit der Arduino IDE

1. **ESP32-Boardunterstützung installieren** (einmalig)
   - Arduino IDE → *Datei → Einstellungen* → *Zusätzliche Boardverwalter-URLs*:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - *Werkzeuge → Board → Boardverwalter* → „esp32" (von Espressif) installieren.

2. **Sketch öffnen:** `esp32/BerlinUBahn/BerlinUBahn.ino`

3. **Board & Port wählen:** *Werkzeuge → Board → ESP32 → „ESP32 Dev Module"*
   und unter *Port* den seriellen Port des angeschlossenen ESP32.
   (Standard-Einstellungen passen; Flash-Größe 4 MB reicht locker.)

4. **Hochladen** (Pfeil-Button). Falls der Upload nicht startet: beim
   „Connecting…" die **BOOT**-Taste am ESP32 gedrückt halten.

5. **Seriellen Monitor** (115200 Baud) öffnen – dort steht:
   ```
   Access Point gestartet.
     WLAN-Name (SSID): Berlin-UBahn
     Passwort:         ubahn1234
     Im Browser oeffnen: http://192.168.4.1
   ```

## Benutzen

1. Am Handy/Laptop mit dem WLAN **`Berlin-UBahn`** verbinden (Passwort `ubahn1234`).
2. Im Browser **http://192.168.4.1** öffnen.
3. Fertig – die U-Bahn fährt. 🚇

## Anpassen

- WLAN-Name/Passwort: oben im `.ino` bei `AP_SSID` / `AP_PASS` ändern
  (Passwort mind. 8 Zeichen, oder `""` für ein offenes WLAN).
- Karte geändert? `index.html` bearbeiten und
  ```
  node esp32/build_sketch.js
  ```
  ausführen, dann neu hochladen.

## Hinweis

Die fahrenden Züge sind eine realistische Simulation, keine echten
GPS-Positionen. Da der ESP32 im Access-Point-Modus läuft, hat er keinen
Internetzugang für echte Live-Daten; dafür müsste er sich stattdessen in ein
WLAN mit Internet einwählen (Station-Modus).
