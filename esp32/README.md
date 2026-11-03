# Berliner U-Bahn auf dem ESP32

Der ESP32 wählt sich in dein Heim-WLAN ein und liefert die interaktive
U-Bahn-Karte (mit fahrenden Zügen) als Webseite aus. Du erreichst sie von
jedem Gerät im selben Netz. Fällt das WLAN aus, macht der ESP32 ersatzweise
ein eigenes WLAN auf, damit er nie unerreichbar ist.

## Dateien

- `BerlinUBahn/BerlinUBahn.ino` – fertiger Arduino-Sketch (App ist eingebettet)
- `build_sketch.js` – erzeugt den Sketch neu aus der `index.html` im Repo-Root

## Vor dem Flashen: WLAN eintragen ⚠️

Im Sketch oben stehen Platzhalter, die du **lokal** ausfüllen musst:

```cpp
const char *WIFI_SSID = "DEIN_WLAN_NAME";
const char *WIFI_PASS = "DEIN_WLAN_PASSWORT";
```

> **Dein WLAN-Passwort NICHT ins öffentliche Git committen!** Nur lokal
> eintragen. (`git update-index --skip-worktree esp32/BerlinUBahn/BerlinUBahn.ino`
> verhindert, dass die Änderung versehentlich gepusht wird.)

## Flashen mit der Arduino IDE

1. **ESP32-Boardunterstützung installieren** (einmalig)
   - Arduino IDE → *Datei → Einstellungen* → *Zusätzliche Boardverwalter-URLs*:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - *Werkzeuge → Board → Boardverwalter* → „esp32" (von Espressif) installieren.

2. **Sketch öffnen:** `esp32/BerlinUBahn/BerlinUBahn.ino` und WLAN eintragen (siehe oben).

3. **Board & Port wählen:** *Werkzeuge → Board → ESP32 → „ESP32 Dev Module"*
   und unter *Port* den seriellen Port des angeschlossenen ESP32.

4. **Hochladen** (Pfeil-Button). Falls der Upload nicht startet: beim
   „Connecting…" die **BOOT**-Taste am ESP32 gedrückt halten.
   > Achtung: Dies **überschreibt die bisherige Firmware** (z. B. Flightradar).

5. **Seriellen Monitor** (115200 Baud) öffnen – dort steht die Adresse:
   ```
   WLAN verbunden.
     Im Browser oeffnen: http://192.168.x.x
     oder bequem:        http://ubahn.local
   ```

## Benutzen

1. Mit demselben WLAN verbunden sein wie der ESP32.
2. Im Browser **http://ubahn.local** (oder die angezeigte IP) öffnen.
3. Fertig – die U-Bahn fährt. 🚇

## Anpassen

- WLAN-Zugangsdaten: oben im `.ino` bei `WIFI_SSID` / `WIFI_PASS`.
- Karte geändert? `index.html` bearbeiten und
  ```
  node esp32/build_sketch.js
  ```
  ausführen (WLAN-Daten danach ggf. wieder eintragen), dann neu hochladen.

## Hinweis

Die fahrenden Züge sind eine realistische Simulation, keine echten
GPS-Positionen. Da der ESP32 im Access-Point-Modus läuft, hat er keinen
Internetzugang für echte Live-Daten; dafür müsste er sich stattdessen in ein
WLAN mit Internet einwählen (Station-Modus).
