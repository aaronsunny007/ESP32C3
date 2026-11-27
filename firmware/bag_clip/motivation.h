// Lines for the Motivation screen (menu -> Motivation). It opens on a random line and
// each tap shows another random one (never the same twice in a row).
// Edit freely: each line is wrapped to the 21-character-wide OLED, about 5 lines (100
// characters) fit. Plain ASCII only (the OLED font has no accents or emoji).
#pragma once

static const char *const MOTIVATION[] = {
  "Small steps still move you forward.",
  "You have done hard things before. You can do this one too.",
  "Rest is part of the work.",
  "Be proud of how far you have come.",
  "Today counts, even if it goes slowly.",
  "One page, one task, one breath at a time.",
  "Mistakes mean you are trying.",
  "You are stronger than this week.",
  "Drink some water. Then keep going.",
  "Your best today is enough.",
  "Keep going. Future you will say thanks.",
  "You are allowed to be proud of yourself.",
};
static const int MOTIVATION_COUNT = sizeof(MOTIVATION) / sizeof(MOTIVATION[0]);
