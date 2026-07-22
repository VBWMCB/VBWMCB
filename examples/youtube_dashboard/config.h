// ===========================================================================
//  config.h  —  EDIT THIS FILE BEFORE FLASHING
//
//  Put your WiFi credentials, a YouTube Data API v3 key, and the channel(s)
//  you want to watch here. Nothing else in the project needs changing.
// ===========================================================================
#pragma once

// ---- WiFi -----------------------------------------------------------------
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"

// ---- YouTube Data API v3 --------------------------------------------------
//  How to get a key (free):
//    1. https://console.cloud.google.com/  ->  create/select a project
//    2. "APIs & Services" -> "Library" -> enable "YouTube Data API v3"
//    3. "APIs & Services" -> "Credentials" -> "Create credentials" -> API key
//    4. Paste the key below.
#define YT_API_KEY     "YOUR_YOUTUBE_API_KEY"

// ---- Channels to display --------------------------------------------------
//  Use the CHANNEL ID (starts with "UC..."), NOT the @handle.
//  Find it: open the channel -> "..." / Share -> "Copy channel ID",
//  or use https://commentpicker.com/youtube-channel-id.php
//
//  Tap the touchscreen to cycle to the next channel in this list.
struct YtChannel {
  const char *id;     // e.g. "UC_x5XG1OV2P6uZZ5FSM9Ttw"  (Google Developers)
  const char *label;  // shown on screen (your own friendly name)
};

static const YtChannel CHANNELS[] = {
  { "UC_x5XG1OV2P6uZZ5FSM9Ttw", "Google Developers" },
  { "UCXuqSBlHAE6Xw-yeJA0Tunw", "Linus Tech Tips"   },
  // { "UCxxxxxxxxxxxxxxxxxxxxxx", "My Channel"      },
};

// ---- Behaviour ------------------------------------------------------------
#define REFRESH_INTERVAL_MS  60000UL   // auto-refresh every 60 s
