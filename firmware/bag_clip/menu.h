// The menu: hold the pad 1.5 s to open it. In the list, a tap moves to the next item and
// a hold chooses it; inside an item, a hold goes back to the list. Untouched for
// MENU_TIMEOUT_MS it closes by itself. Drawing is in ui.cpp; this file is the behaviour.
#pragma once
#include <Arduino.h>
#include "state.h"

// What the main loop has to do after a touch in the menu.
enum MenuAction : uint8_t {
  MENU_NONE,
  MENU_CLOSED,        // the menu closed: redraw the normal screen
  MENU_SEND,          // send menuCommand() to the phone (music player taps)
  MENU_GREETING,      // show the birthday greeting again
  MENU_INFO,          // show the info screen
  MENU_POWER_OFF,     // go to deep sleep (tap to wake)
};

void menuOpen();
bool menuActive();
bool menuTakesPress();        // the dino game: a finger down jumps at once, it isn't counted as a tap
void menuPress();             // finger down (only called when menuTakesPress())
MenuAction menuTaps(int taps);
MenuAction menuHold();
Command menuCommand();        // the command for MENU_SEND
// Runs the game and the timeout and draws the current menu screen when one is due.
// Returns MENU_CLOSED when the menu timed out.
MenuAction menuLoop(const AppState &st);
