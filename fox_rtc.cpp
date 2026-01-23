#include "fox_rtc.h"
#include "fox_config.h"
#include <Wire.h>

// Register addresses untuk DS3231
#define DS3231_ADDRESS 0x68
#define DS3231_TIME_REG 0x00
#define DS3231_CONTROL_REG 0x0E
#define DS3231_TEMP_REG 0x11

// Konversi BCD ke decimal
static uint8_t bcdToDec(uint8_t val) {
    return ((val / 16) * 10) + (val % 16);
}

// Konversi decimal ke BCD
static uint8_t decToBcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

bool initRTC() {
    Wire.beginTransmission(DS3231_ADDRESS);
    if (Wire.endTransmission() == 0) {
        // Cek jika RTC berjalan
        Wire.beginTransmission(DS3231_ADDRESS);
        Wire.write(DS3231_CONTROL_REG);
        Wire.endTransmission();
        
        Wire.requestFrom(DS3231_ADDRESS, 1);
        if (Wire.available()) {
            uint8_t status = Wire.read();
            if (!(status & 0x80)) {
                // RTC berjalan normal
            } else {
                // Set dari waktu kompilasi otomatis
                setRTCFromCompileTime();
            }
        }
        return true;
    }
    
    return false;
}

RTCDateTime getRTC() {
    RTCDateTime dt;
    
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_TIME_REG);
    Wire.endTransmission();
    
    Wire.requestFrom(DS3231_ADDRESS, 7);
    if (Wire.available() == 7) {
        uint8_t seconds = bcdToDec(Wire.read() & 0x7F);
        uint8_t minutes = bcdToDec(Wire.read());
        uint8_t hours = bcdToDec(Wire.read() & 0x3F); // 24h mode
        uint8_t dayOfWeek = bcdToDec(Wire.read());
        uint8_t day = bcdToDec(Wire.read());
        uint8_t month = bcdToDec(Wire.read() & 0x1F); // Mask century bit
        uint16_t year = bcdToDec(Wire.read()) + 2000;
        
        dt.second = seconds;
        dt.minute = minutes;
        dt.hour = hours;
        dt.dayOfWeek = dayOfWeek;
        dt.day = day;
        dt.month = month;
        dt.year = year;
    } else {
        // Fallback ke dummy jika gagal baca
        static unsigned long lastMillis = 0;
        static unsigned long seconds = 0;
        
        unsigned long currentMillis = millis();
        if(currentMillis - lastMillis >= 1000) {
            seconds++;
            lastMillis = currentMillis;
        }
        
        dt.second = seconds % 60;
        dt.minute = (seconds / 60) % 60;
        dt.hour = (seconds / 3600) % 24;
        dt.dayOfWeek = 1;
        dt.day = 1;
        dt.month = 1;
        dt.year = 2024;
    }
    
    return dt;
}

void setRTCTime(uint16_t year, uint8_t month, uint8_t day,
                uint8_t hour, uint8_t minute, uint8_t second,
                uint8_t dayOfWeek) {
    
    // Convert tahun ke 2 digit
    uint8_t year2digit = year - 2000;
    
    // Jika dayOfWeek = 0 (default), set ke 1 (Senin)
    if (dayOfWeek == 0) {
        dayOfWeek = 1;  // Default: Senin
    }
    
    // Validasi dayOfWeek
    if (dayOfWeek < 1 || dayOfWeek > 7) {
        dayOfWeek = 1;  // Fallback ke Senin
    }
    
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_TIME_REG);
    
    Wire.write(decToBcd(second));
    Wire.write(decToBcd(minute));
    Wire.write(decToBcd(hour));      // 24h mode
    Wire.write(decToBcd(dayOfWeek)); // DAY OF WEEK
    Wire.write(decToBcd(day));
    Wire.write(decToBcd(month));     // No century bit
    Wire.write(decToBcd(year2digit));
    
    Wire.endTransmission();
    
    // Clear OSF flag
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_CONTROL_REG);
    Wire.write(0x00);  // Clear all control bits
    Wire.endTransmission();
}

void setRTCFromCompileTime() {
    // Ambil waktu dari waktu kompilasi (__DATE__ dan __TIME__)
    char monthStr[4];
    int year, month, day, hour, minute, second;
    
    sscanf(__DATE__, "%s %d %d", monthStr, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
    
    // Convert month string to number
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (month = 0; month < 12; month++) {
        if (strcmp(monthStr, months[month]) == 0) {
            month++; // Convert to 1-based
            break;
        }
    }
    
    setRTCTime(year, month, day, hour, minute, second, 0);
}

float getTemperature() {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_TEMP_REG);
    Wire.endTransmission();
    
    Wire.requestFrom(DS3231_ADDRESS, 2);
    if (Wire.available() == 2) {
        uint8_t temp_msb = Wire.read();
        uint8_t temp_lsb = Wire.read();
        
        // Convert to temperature (℃)
        float temp = temp_msb + ((temp_lsb >> 6) * 0.25);
        return temp;
    }
    return -100.0; // Error value
}

bool isRunning() {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_CONTROL_REG);
    Wire.endTransmission();
    
    Wire.requestFrom(DS3231_ADDRESS, 1);
    if (Wire.available()) {
        uint8_t status = Wire.read();
        return !(status & 0x80); // OSF flag clear = running
    }
    return false;
}

String getTimeString(bool includeSeconds) {
    RTCDateTime dt = getRTC();
    
    char buffer[9];
    if(includeSeconds) {
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", 
                 dt.hour, dt.minute, dt.second);
    } else {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", 
                 dt.hour, dt.minute);
    }
    
    return String(buffer);
}

String getDateString() {
    RTCDateTime dt = getRTC();
    
    char buffer[11];
    snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", 
             dt.day, dt.month, dt.year);
    
    return String(buffer);
}

bool setTimeFromString(String timeStr) {
    // Format: HH:MM:SS atau HH:MM
    int hour, minute, second = 0;
    
    if (timeStr.length() == 8) { // HH:MM:SS
        if (sscanf(timeStr.c_str(), "%d:%d:%d", &hour, &minute, &second) != 3) {
            return false;
        }
    } else if (timeStr.length() == 5) { // HH:MM
        if (sscanf(timeStr.c_str(), "%d:%d", &hour, &minute) != 2) {
            return false;
        }
        second = 0;
    } else {
        return false;
    }
    
    // Validasi
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        return false;
    }
    
    // Ambil tanggal saat ini dari RTC
    RTCDateTime current = getRTC();
    
    // Set waktu baru (tanggal dan hari tetap)
    setRTCTime(current.year, current.month, current.day, hour, minute, second, current.dayOfWeek);
    
    return true;
}

bool setDateFromString(String dateStr) {
    // Format: DD/MM/YYYY
    int day, month, year;
    
    if (sscanf(dateStr.c_str(), "%d/%d/%d", &day, &month, &year) != 3) {
        return false;
    }
    
    // Validasi
    if (day < 1 || day > 31 || month < 1 || month > 12 || year < 2000 || year > 2099) {
        return false;
    }
    
    // Ambil waktu saat ini dari RTC
    RTCDateTime current = getRTC();
    
    // Set tanggal baru (waktu tetap, hari tetap)
    setRTCTime(year, month, day, current.hour, current.minute, current.second, current.dayOfWeek);
    
    return true;
}

bool setDayOfWeek(uint8_t dayOfWeek) {
    // Validasi: 1-7 (1=Minggu, 2=Senin, ..., 7=Sabtu)
    if (dayOfWeek < 1 || dayOfWeek > 7) {
        return false;
    }
    
    // Ambil waktu saat ini dari RTC
    RTCDateTime current = getRTC();
    
    // Set waktu dengan dayOfWeek baru (tanggal & waktu tetap)
    setRTCTime(current.year, current.month, current.day, 
               current.hour, current.minute, current.second, dayOfWeek);
    
    return true;
}
