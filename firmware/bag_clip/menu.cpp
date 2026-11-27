#include "menu.h"
#include "config.h"
#include "settings.h"
#include "ui.h"
#include "dino.h"
#include "motivation.h"

// The list, in the order it shows. Change a label here; the behaviour is in menuHold().
enum Item : uint8_t { IT_MUSIC, IT_DINO, IT_MOTIVATION, IT_GREETING, IT_BRIGHTNESS, IT_INFO, IT_POWER, IT_EXIT, IT_COUNT };
static const char *const LABELS[IT_COUNT] = {
  "Music player", "Dino game", "Motivation", "Show greeting", "Brightness", "Info", "Power off", "Exit",
};

// Brightness steps the Brightness screen cycles through (OLED contrast 1-255).
static const uint8_t BRIGHT_LEVELS[] = {40, 120, 255};
static const char *const BRIGHT_NAMES[] = {"Low", "Medium", "High"};

enum View : uint8_t { V_OFF, V_LIST, V_MUSIC, V_DINO, V_MOTIVATION, V_BRIGHTNESS };
static View view = V_OFF;
static uint8_t sel = 0;            // highlighted item in the list
static uint8_t quote = 0;          // motivation line on show
static uint8_t brightLevel = 1;
static bool dirty = true;          // redraw at the next loop
static unsigned long lastTouch = 0, lastDraw = 0, lastStep = 0;
static Command pending = CMD_TOGGLE;
static DinoGame game;

static void show(View v) { view = v; dirty = true; }

void menuOpen() {
  view = V_LIST;
  sel = 0;
  dirty = true;
  lastTouch = millis();
}

bool menuActive() { return view != V_OFF; }
bool menuTakesPress() { return view == V_DINO; }
Command menuCommand() { return pending; }

void menuPress() {
  lastTouch = millis();
  dinoJump(game);
  dirty = true;
}

static uint8_t nearestBright() {
  uint8_t best = 0;
  for (uint8_t i = 1; i < sizeof(BRIGHT_LEVELS); i++)
    if (abs((int)BRIGHT_LEVELS[i] - settings.brightness) < abs((int)BRIGHT_LEVELS[best] - settings.brightness)) best = i;
  return best;
}

MenuAction menuTaps(int taps) {
  lastTouch = millis();
  dirty = true;
  switch (view) {
    case V_LIST: sel = (sel + 1) % IT_COUNT; return MENU_NONE;
    case V_MOTIVATION: quote = (quote + 1) % MOTIVATION_COUNT; return MENU_NONE;
    case V_BRIGHTNESS:
      brightLevel = (brightLevel + 1) % sizeof(BRIGHT_LEVELS);
      settings.brightness = BRIGHT_LEVELS[brightLevel];   // the main loop applies it at once
      return MENU_NONE;
    case V_MUSIC:   // the same taps as everywhere else: 1 play/pause, 2 next, 3 previous
      pending = taps == 1 ? CMD_TOGGLE : taps == 2 ? CMD_NEXT : CMD_PREV;
      return MENU_SEND;
    default: return MENU_NONE;
  }
}

MenuAction menuHold() {
  lastTouch = millis();
  dirty = true;
  if (view == V_BRIGHTNESS) settingsSave();   // keep the new brightness after sleep
  if (view != V_LIST) { show(V_LIST); return MENU_NONE; }
  switch (sel) {
    case IT_MUSIC:      show(V_MUSIC); return MENU_NONE;
    case IT_DINO:       dinoReset(game); show(V_DINO); return MENU_NONE;
    case IT_MOTIVATION: show(V_MOTIVATION); return MENU_NONE;
    case IT_BRIGHTNESS: brightLevel = nearestBright(); show(V_BRIGHTNESS); return MENU_NONE;
    case IT_GREETING:   view = V_OFF; return MENU_GREETING;
    case IT_INFO:       view = V_OFF; return MENU_INFO;
    case IT_POWER:      view = V_OFF; return MENU_POWER_OFF;
    default:            view = V_OFF; return MENU_CLOSED;   // Exit
  }
}

MenuAction menuLoop(const AppState &st) {
  unsigned long now = millis();
  bool playing = view == V_DINO && game.phase == DinoGame::RUNNING;
  if (!playing && now - lastTouch > MENU_TIMEOUT_MS) {
    if (view == V_BRIGHTNESS) settingsSave();
    view = V_OFF;
    return MENU_CLOSED;
  }
  if (view == V_DINO) {
    dinoUpdate(game, now - lastStep);
    if (playing) lastTouch = now;   // a running game never times out
  }
  lastStep = now;

  // How often each screen is redrawn: the game smoothly, the player for its clock and
  // scrolling title, the rest only when something changed.
  unsigned long every = view == V_DINO ? 33 : view == V_MUSIC ? 100 : 0;
  if (!dirty && !(every && now - lastDraw >= every)) return MENU_NONE;
  dirty = false;
  lastDraw = now;
  switch (view) {
    case V_LIST:       uiMenuList(LABELS, IT_COUNT, sel); break;
    case V_MUSIC:      uiMusic(st); break;
    case V_DINO:       uiDino(game); break;
    case V_MOTIVATION: uiMotivation(MOTIVATION[quote], quote + 1, MOTIVATION_COUNT); break;
    case V_BRIGHTNESS: uiBrightness(BRIGHT_NAMES, sizeof(BRIGHT_LEVELS), brightLevel); break;
    default: break;
  }
  return MENU_NONE;
}
