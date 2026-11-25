// secrets.example.h
// Copy this file to "secrets.h" in the same folder and fill in your values.
// secrets.h is listed in .gitignore so your keys never get committed.
//
// Get these from https://developer.spotify.com/dashboard :
//   1. Create an app, add redirect URI  http://localhost:8888/callback
//   2. Run the SpotifyArduino "getRefreshToken" example with scopes
//      user-read-currently-playing user-modify-playback-state
//   3. Paste the client id, secret and refresh token below.
#pragma once

#define SPOTIFY_CLIENT_ID      "YOUR_SPOTIFY_CLIENT_ID"
#define SPOTIFY_CLIENT_SECRET  "YOUR_SPOTIFY_CLIENT_SECRET"
#define SPOTIFY_REFRESH_TOKEN  "YOUR_SPOTIFY_REFRESH_TOKEN"

// Two-letter market code used for track lookups (IN = India).
#define SPOTIFY_MARKET         "IN"
