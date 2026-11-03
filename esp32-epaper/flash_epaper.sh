#!/usr/bin/env bash
#
# Flasher fuer das Waveshare ESP32-S3 E-Paper 1.54" (macOS).
# Installiert arduino-cli + ESP32-Core + Bibliotheken, kompiliert und flasht
# die LIVE-Abfahrten-Anzeige. Das WLAN wird NICHT hier eingetippt, sondern
# nach dem Start bequem am Handy eingerichtet (Portal "Berlin-UBahn-Setup").
#
#   Aufruf im Repo-Ordner:   bash esp32-epaper/flash_epaper.sh   [optional: PORT]
#
set -euo pipefail

FQBN="esp32:esp32:esp32s3"
URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH="$DIR/BerlinUBahnEPD"

echo "== 1/6  arduino-cli pruefen =="
if ! command -v arduino-cli >/dev/null 2>&1; then
  command -v brew >/dev/null 2>&1 || { echo "FEHLER: Homebrew fehlt: https://brew.sh"; exit 1; }
  brew install arduino-cli
fi
arduino-cli version

echo "== 2/6  ESP32-Core installieren (einmalig) =="
arduino-cli config init >/dev/null 2>&1 || true
arduino-cli config add board_manager.additional_urls "$URL" >/dev/null 2>&1 || true
arduino-cli core update-index --additional-urls "$URL"
arduino-cli core install esp32:esp32 --additional-urls "$URL" || true

echo "== 3/6  Bibliotheken installieren =="
arduino-cli lib install "Adafruit GFX Library" || true
arduino-cli lib install "ArduinoJson" || true
arduino-cli lib install "WiFiManager" || true

echo "== 4/6  ESP32-S3-Port finden =="
PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT="$(ls /dev/cu.* 2>/dev/null | grep -Ei 'usbmodem|usbserial|wchusbserial|SLAB' | head -n1 || true)"
fi
if [ -z "$PORT" ]; then
  echo "FEHLER: keinen ESP32-Port gefunden."
  echo "  -> ANDERE USB-C-Buchse / echtes Datenkabel probieren."
  echo "  -> oder Port angeben:  bash esp32-epaper/flash_epaper.sh /dev/cu.usbmodemXXXX"
  ls /dev/cu.* 2>/dev/null || true
  exit 1
fi
echo "  benutze Port: $PORT"

echo "== 5/6  kompilieren =="
arduino-cli compile -b "$FQBN" "$SKETCH"

echo "== 6/6  flashen =="
arduino-cli upload -p "$PORT" -b "$FQBN" "$SKETCH"

echo
echo "Fertig! WLAN einrichten am Handy:"
echo "  1) Am Handy ins WLAN 'Berlin-UBahn-Setup' gehen."
echo "  2) Die Seite oeffnet sich automatisch -> dein WLAN waehlen + Passwort eingeben."
echo "  3) ESP merkt sich das und zeigt dann die LIVE-Abfahrten."
echo "WLAN spaeter neu einrichten: beim Einschalten die BOOT-Taste gedrueckt halten."
