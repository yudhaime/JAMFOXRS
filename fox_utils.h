#ifndef FOX_UTILS_H
#define FOX_UTILS_H

#include <Arduino.h>

// Helper functions
uint8_t bcdToDec(uint8_t val);
uint8_t decToBcd(uint8_t val);
uint8_t calculateDayOfWeek(uint16_t year, uint8_t month, uint8_t day);
String formatTime(uint8_t hour, uint8_t minute, uint8_t second, bool includeSeconds);
String formatDate(uint8_t day, uint8_t month, uint16_t year);
bool isValidTime(uint8_t hour, uint8_t minute, uint8_t second);
bool isValidDate(uint8_t day, uint8_t month, uint16_t year);

#endif
