#!/usr/bin/env bash
#
# Flasher fuer das Waveshare ESP32-S3 E-Paper 1.54" (macOS).
# Installiert arduino-cli + ESP32-Core + Bibliotheken, fragt dein WLAN ab,
# kompiliert und flasht die LIVE-Abfahrten-Anzeige.
#
#   Aufruf im Repo-Ordner:   bash esp32-epaper/flash_epaper.sh   [optional: PORT]
#
set -euo pipefail

FQBN="esp32:esp32:esp32s3"
URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH="$DIR/BerlinUBahnEPD"
INO="$SKETCH/BerlinUBahnEPD.ino"

echo "== 1/7  arduino-cli pruefen =="
if ! command -v arduino-cli >/dev/null 2>&1; then
  command -v brew >/dev/null 2>&1 || { echo "FEHLER: Homebrew fehlt: https://brew.sh"; exit 1; }
  brew install arduino-cli
fi
arduino-cli version

echo "== 2/7  ESP32-Core installieren (einmalig) =="
arduino-cli config init >/dev/null 2>&1 || true
arduino-cli config add board_manager.additional_urls "$URL" >/dev/null 2>&1 || true
arduino-cli core update-index --additional-urls "$URL"
arduino-cli core install esp32:esp32 --additional-urls "$URL" || true

echo "== 3/7  Bibliotheken installieren =="
arduino-cli lib install "Adafruit GFX Library" || true
arduino-cli lib install "ArduinoJson" || true

echo "== 4/7  WLAN-Zugangsdaten =="
if grep -q '__WIFI_SSID__' "$INO"; then
  read -r -p "  WLAN-Name (SSID): " SSID
  read -r -s -p "  WLAN-Passwort:    " PASS; echo
  cp "$INO" "$INO.bak"
  # nach dem Flashen Platzhalter wiederherstellen -> git bleibt sauber
  trap 'mv -f "$INO.bak" "$INO" 2>/dev/null || true' EXIT
  sed -i '' "s|__WIFI_SSID__|$SSID|; s|__WIFI_PASS__|$PASS|" "$INO"
  echo "  eingetragen (nur fuer diesen Flash; Datei wird danach zurueckgesetzt)."
else
  echo "  WLAN scheint schon eingetragen - ok."
fi

echo "== 5/7  ESP32-S3-Port finden =="
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

echo "== 6/7  kompilieren =="
arduino-cli compile -b "$FQBN" "$SKETCH"

echo "== 7/7  flashen =="
arduino-cli upload -p "$PORT" -b "$FQBN" "$SKETCH"

echo
echo "Fertig! Der ESP waehlt sich ins WLAN ein und zeigt die LIVE-Abfahrten."
echo "(Kein WLAN? Dann steht es auf dem Display - SSID/Passwort pruefen.)"
