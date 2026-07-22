# Vollblut Claude — Telegram Lead Notifier (ESP32 CYD)

Turns an **ESP32-2432S028R "Cheap Yellow Display" (CYD)** into a physical lead
ticker. It long-polls the Telegram Bot API over WiFi, and **every message that
arrives at your bot (`@vollblutclaudeBot`) is shown as a big "NEUER LEAD" card**
on the 2.8" TFT — with a beep and a green LED flash as an alert. Tap the screen
to scroll back through recent leads.

> An ESP32 can't play or stream video, so this is a **notification display**,
> not a media player — which is exactly what a lead ticker needs to be.

## Hardware

- ESP32-2432S028R (CYD): ESP32-WROOM-32, 2.8" ILI9341 TFT (240×320), resistive
  XPT2046 touch, onboard RGB LED + speaker, CH340 USB-UART.
- A USB cable (data, not charge-only).

All display/touch pins are already configured in `platformio.ini` (TFT_eSPI via
build flags) and `src/main.cpp` (touch on its separate SPI bus). Nothing to wire.

## Setup

1. **Install [PlatformIO](https://platformio.org/)** (VS Code extension or the
   `pio` CLI).

2. **Create your config** (this file is git-ignored, so your token stays out of
   the repo):
   ```bash
   cp include/config.h.example include/config.h
   ```
   Then edit `include/config.h`:
   - `WIFI_SSID` / `WIFI_PASSWORD` — your 2.4 GHz WiFi (the ESP32 has no 5 GHz).
   - `TELEGRAM_BOT_TOKEN` — the token for `@vollblutclaudeBot` from
     [@BotFather](https://t.me/BotFather).
   - Optional: `GMT_OFFSET_HOURS`, alert/beep toggles, history length.

3. **Flash it** (board connected via USB):
   ```bash
   pio run -t upload
   ```
   Watch the logs / see leads on serial:
   ```bash
   pio device monitor
   ```
   If the upload doesn't start, hold the **BOOT** button while it connects.

## How it works

- On boot the device connects to WiFi, syncs the clock (NTP), and **silently
  drains any old backlog** so it only alerts on leads arriving from then on.
- It long-polls `getUpdates`; each new message is stored, rendered, and
  triggers the beep + LED flash.
- The Telegram `offset` is persisted in flash (NVS), so a reboot doesn't replay
  or miss leads.
- **Tap** the touchscreen to page back through the last `LEAD_HISTORY` leads.

## Security note

The bot token is a secret. It lives only in `include/config.h`, which is
git-ignored — never commit it. If the token was ever shared in plain text,
rotate it in **@BotFather → `/revoke`** and paste the new one into your local
`include/config.h`.

## Also in this repo

`examples/youtube_dashboard/` — an earlier build that shows live YouTube channel
stats (subscribers/views/videos) on the same board. Not compiled by default;
swap it into `src/` if you want to try it.
