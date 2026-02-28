#ifndef RTC_H
#define RTC_H

#include <Arduino.h>

struct RTCDateTime {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t dayOfWeek;
};

bool initRTC();
RTCDateTime getRTC();
String getTimeString(bool includeSeconds = true);
String getDateString();

void setRTCTime(uint16_t year, uint8_t month, uint8_t day, 
                uint8_t hour, uint8_t minute, uint8_t second,
                uint8_t dayOfWeek = 0);
void setRTCFromCompileTime();
float getTemperature();
bool isRunning();

// Fungsi untuk pengaturan
bool setTimeFromString(String timeStr);
bool setDateFromString(String dateStr);
bool setDayOfWeek(uint8_t dayOfWeek);

#endif
