#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

// Basic display functions
void initDisplay();
void updateDisplay(int page);
void showSetupMode(bool blinkState);
void resetDisplay();

// Thread-safe display functions
bool safeDisplayUpdate(int page);

// SPECIAL MODE DISPLAY FUNCTIONS
void updateSpecialModeDisplay(uint8_t modeType, bool blinkState = false);
bool safeShowSpecialMode(uint8_t modeType, bool blinkState = false);

// Display status
extern bool displayReady;

// Helper untuk FreeRTOS
bool safeI2COperation(uint32_t timeoutMs);  // Internal use
void releaseI2C();                          // Internal use

#endif
