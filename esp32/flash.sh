#!/usr/bin/env bash
#
# Ein-Klick-Flasher fuer den ESP32 (macOS).
# Installiert bei Bedarf arduino-cli + ESP32-Core, fragt dein WLAN ab,
# kompiliert die U-Bahn-App und spielt sie auf den ESP32.
#
#   Aufruf im Repo-Ordner:   bash esp32/flash.sh   [optional: PORT]
#
set -euo pipefail

FQBN="esp32:esp32:esp32"
URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH="$DIR/BerlinUBahn"
INO="$SKETCH/BerlinUBahn.ino"

echo "== 1/6  arduino-cli pruefen =="
if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "arduino-cli fehlt - installiere per Homebrew..."
  command -v brew >/dev/null 2>&1 || { echo "FEHLER: Homebrew fehlt. Installiere: https://brew.sh"; exit 1; }
  brew install arduino-cli
fi
arduino-cli version

echo "== 2/6  ESP32-Core installieren (einmalig, kann etwas dauern) =="
arduino-cli config init >/dev/null 2>&1 || true
arduino-cli config add board_manager.additional_urls "$URL" >/dev/null 2>&1 || true
arduino-cli core update-index --additional-urls "$URL"
arduino-cli core install esp32:esp32 --additional-urls "$URL" || true

echo "== 3/6  WLAN-Zugangsdaten =="
if grep -q 'DEIN_WLAN_NAME' "$INO"; then
  read -r -p "  WLAN-Name (SSID): " SSID
  read -r -s -p "  WLAN-Passwort:    " PASS; echo
  # Backup + lokal eintragen (wird NICHT committet)
  cp "$INO" "$INO.bak"
  sed -i '' "s|DEIN_WLAN_NAME|$SSID|; s|DEIN_WLAN_PASSWORT|$PASS|" "$INO"
  echo "  eingetragen (Backup: BerlinUBahn.ino.bak). Bitte NICHT committen."
else
  echo "  WLAN scheint schon eingetragen - ok."
fi

echo "== 4/6  ESP32-Port finden =="
PORT="${1:-}"
if [ -z "$PORT" ]; then
  PORT="$(arduino-cli board list | awk '/serial/{print $1; exit}')"
fi
if [ -z "$PORT" ]; then
  echo "FEHLER: keinen ESP32-Port gefunden. Stecker pruefen, oder Port angeben:"
  echo "        bash esp32/flash.sh /dev/cu.usbserial-XXXX"
  arduino-cli board list
  exit 1
fi
echo "  benutze Port: $PORT"

echo "== 5/6  kompilieren =="
arduino-cli compile -b "$FQBN" "$SKETCH"

echo "== 6/6  flashen =="
arduino-cli upload -p "$PORT" -b "$FQBN" "$SKETCH"

echo
echo "Fertig! Serieller Monitor (mit Strg-C beenden) - dort steht die Adresse:"
echo
arduino-cli monitor -p "$PORT" -c baudrate=115200
