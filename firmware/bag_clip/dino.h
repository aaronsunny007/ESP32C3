// The dino game (menu -> Dino game): tap to jump over the cacti. The game logic lives
// here; ui.cpp draws it (uiDino).
#pragma once
#include <Arduino.h>

#define DINO_MAX_CACTI 4

struct DinoCactus { float x; uint8_t w, h; bool used; };

struct DinoGame {
  enum Phase : uint8_t { READY, RUNNING, OVER } phase = READY;
  float y = 0;              // height above the ground, px
  float vy = 0;             // px/s, up is positive
  float speed = 0;          // px/s the ground moves
  float distance = 0;       // px run this game
  float nextGap = 0;        // px until the next cactus
  unsigned long overAt = 0; // millis() when the game ended
  DinoCactus cacti[DINO_MAX_CACTI];
  uint32_t score() const { return (uint32_t)(distance / 8); }
};

extern uint32_t dinoHighScore;   // kept through deep sleep, lost when switched off

void dinoReset(DinoGame &g);
void dinoJump(DinoGame &g);                 // a tap: start, jump, or restart after a game over
void dinoUpdate(DinoGame &g, uint32_t dtMs);
