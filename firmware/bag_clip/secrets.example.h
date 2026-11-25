// secrets.example.h
// Copy this file to "secrets.h" in the same folder and fill in your values.
// secrets.h is listed in .gitignore so your keys never get committed.
//
// Spotify keys, from https://developer.spotify.com/dashboard :
//   1. Create an app, add redirect URI  http://127.0.0.1:8888/callback
//   2. Get a refresh token with these scopes (the queue needs the 2nd one):
//        user-read-currently-playing user-read-playback-state user-modify-playback-state
//      (SpotifyArduino's "getRefreshToken" example works; see PROJECT.md.)
//   3. Paste the client id, secret and refresh token below.
#pragma once

#define SPOTIFY_CLIENT_ID      "YOUR_SPOTIFY_CLIENT_ID"
#define SPOTIFY_CLIENT_SECRET  "YOUR_SPOTIFY_CLIENT_SECRET"
#define SPOTIFY_REFRESH_TOKEN  "YOUR_SPOTIFY_REFRESH_TOKEN"

// Two-letter market code used for track lookups (IN = India).
#define SPOTIFY_MARKET         "IN"

// Password for the settings web page (user "admin") and for over-the-air updates.
// Change it: anyone on the same Wi-Fi who knows it can change settings or flash firmware.
#define ADMIN_PASSWORD         "change-me"

// Optional: a Wi-Fi network to save on first boot, so you can test before pairing a phone.
// Leave empty to add networks from the phone page over Bluetooth instead.
#define DEFAULT_WIFI_SSID      ""
#define DEFAULT_WIFI_PASSWORD  ""

// Bench test only: set to 1 if every Spotify request fails with a TLS error
// (certificate changed). This turns off certificate checks. Set back to 0 after.
#define SPOTIFY_TLS_INSECURE   0
