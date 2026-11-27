#include "dino.h"

// Distances are pixels on the 128x64 screen; times in seconds.
static const float JUMP_SPEED   = 255.0f;   // px/s up: a jump about 36 px high, 0.6 s long
static const float GRAVITY      = 900.0f;   // px/s^2
static const float START_SPEED  = 70.0f;    // px/s
static const float MAX_SPEED    = 150.0f;
static const float SPEED_UP     = 2.5f;     // px/s faster every second
static const float DINO_X = 8, DINO_W = 16; // the dino's box (ui.cpp draws it there)
static const float RESTART_MS = 600;        // a game over ignores taps this long

RTC_DATA_ATTR uint32_t dinoHighScore = 0;

static float randomGap(float speed) { return 55 + speed * 0.4f + (float)(esp_random() % 70); }

void dinoReset(DinoGame &g) {
  g = DinoGame();
  g.speed = START_SPEED;
  g.nextGap = 60;
}

void dinoJump(DinoGame &g) {
  if (g.phase == DinoGame::READY) { dinoReset(g); g.phase = DinoGame::RUNNING; }
  if (g.phase == DinoGame::OVER) {
    if (millis() - g.overAt < RESTART_MS) return;
    dinoReset(g);
    g.phase = DinoGame::RUNNING;
    return;
  }
  if (g.y <= 0.01f) g.vy = JUMP_SPEED;     // only from the ground
}

void dinoUpdate(DinoGame &g, uint32_t dtMs) {
  if (g.phase != DinoGame::RUNNING) return;
  float dt = (dtMs > 50 ? 50 : dtMs) / 1000.0f;   // a long frame never teleports a cactus
  g.speed = g.speed + SPEED_UP * dt;
  if (g.speed > MAX_SPEED) g.speed = MAX_SPEED;
  float step = g.speed * dt;
  g.distance += step;

  // the dino
  if (g.y > 0 || g.vy > 0) {
    g.vy -= GRAVITY * dt;
    g.y += g.vy * dt;
    if (g.y < 0) { g.y = 0; g.vy = 0; }
  }

  // the cacti
  g.nextGap -= step;
  for (auto &c : g.cacti) {
    if (!c.used) continue;
    c.x -= step;
    if (c.x + c.w < 0) c.used = false;
  }
  if (g.nextGap <= 0) {
    for (auto &c : g.cacti) {
      if (c.used) continue;
      c.used = true;
      c.x = 128;
      c.w = 6 + esp_random() % 6;
      c.h = 10 + esp_random() % 7;
      break;
    }
    g.nextGap = randomGap(g.speed);
  }

  // a hit: the dino's box (a little smaller than the drawing) against each cactus
  float dinoBottom = g.y, dinoTop = g.y + 13;
  for (auto &c : g.cacti) {
    if (!c.used) continue;
    bool overlapX = c.x < DINO_X + DINO_W - 3 && c.x + c.w > DINO_X + 3;
    bool overlapY = dinoBottom < c.h - 1 && dinoTop > 0;
    if (overlapX && overlapY) {
      g.phase = DinoGame::OVER;
      g.overAt = millis();
      if (g.score() > dinoHighScore) dinoHighScore = g.score();
      return;
    }
  }
}
