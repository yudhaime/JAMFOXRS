#ifndef FOX_UTILS_H
#define FOX_UTILS_H

#include <Arduino.h>

// =============================================
// CONVERSION FUNCTIONS
// =============================================

uint8_t bcdToDec(uint8_t val);
uint8_t decToBcd(uint8_t val);
uint8_t calculateDayOfWeek(uint16_t year, uint8_t month, uint8_t day);
String formatTime(uint8_t hour, uint8_t minute, uint8_t second, bool includeSeconds);
String formatDate(uint8_t day, uint8_t month, uint16_t year);

// =============================================
// VALIDATION FUNCTIONS
// =============================================

bool isValidTime(uint8_t hour, uint8_t minute, uint8_t second);
bool isValidDate(uint8_t day, uint8_t month, uint16_t year);

// =============================================
// MATH & CALCULATION FUNCTIONS
// =============================================

float calculateMovingAverage(float newValue, float oldAverage, float alpha = 0.3);
float constrainFloat(float value, float minVal, float maxVal);
bool hasSignificantChange(float newValue, float oldValue, float threshold);

// =============================================
// STRING UTILITIES
// =============================================

String padNumber(int number, int width, char padChar = '0');
String formatFloat(float value, int decimalPlaces);
String truncateString(const String& str, int maxLength);

// =============================================
// TIME UTILITIES
// =============================================

unsigned long timeSince(unsigned long timestamp);
bool isTimeElapsed(unsigned long timestamp, unsigned long interval);
unsigned long calculateDeltaTime(unsigned long lastTime);

#endif
