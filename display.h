#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

void initDisplay();
void updateDisplay(int page);
void showSetupMode(bool blinkState);  // Fungsi baru

#endif