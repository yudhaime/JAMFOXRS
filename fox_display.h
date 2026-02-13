#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

// Basic display functions
void initDisplay();
void updateDisplay(int page);
void showSetupMode(bool blinkState);
void resetDisplay();
void resetDisplayState();

// Thread-safe display functions
bool safeDisplayUpdate(int page);

// Animation functions
void updateAnimation();
void updateAnimationTargets();
void resetAnimation();

// Formatting functions
String formatVoltage(float voltage);
String formatCurrent(float current);
String formatPower(float power);
String removeTrailingZero(float value, int decimalPlaces);

// I2C Safety & Recovery
bool safeI2COperation(uint32_t timeoutMs);
void releaseI2C();
void recoverI2CBus();

// Display status
extern bool displayReady;

// NEW: Charging page check
bool isChargingPageDisplayed();

#endif
