#include "fox_utils.h"

// Convert BCD to decimal
uint8_t bcdToDec(uint8_t val) {
    return ((val / 16) * 10) + (val % 16);
}

// Convert decimal to BCD
uint8_t decToBcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

// Calculate day of week (Zeller's Congruence)
// Returns: 0=Sunday, 1=Monday, ..., 6=Saturday
uint8_t calculateDayOfWeek(uint16_t year, uint8_t month, uint8_t day) {
    if (month < 3) {
        month += 12;
        year--;
    }
    
    uint16_t century = year / 100;
    uint16_t yearOfCentury = year % 100;
    
    uint16_t h = (day + (13 * (month + 1)) / 5 + yearOfCentury + 
                 (yearOfCentury / 4) + (century / 4) + (5 * century)) % 7;
    
    // Convert to 0=Saturday, 1=Sunday, ..., 6=Friday
    // Then convert to 1=Sunday, 2=Monday, ..., 7=Saturday
    uint8_t dayOfWeek = (h + 6) % 7; // 0=Sunday, 1=Monday, ...
    return dayOfWeek + 1; // 1=Sunday, 2=Monday, ..., 7=Saturday
}

// Format time string
String formatTime(uint8_t hour, uint8_t minute, uint8_t second, bool includeSeconds) {
    char buffer[9];
    if (includeSeconds) {
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hour, minute, second);
    } else {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
    }
    return String(buffer);
}

// Format date string
String formatDate(uint8_t day, uint8_t month, uint16_t year) {
    char buffer[11];
    snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", day, month, year);
    return String(buffer);
}

// Validate time
bool isValidTime(uint8_t hour, uint8_t minute, uint8_t second) {
    return (hour < 24 && minute < 60 && second < 60);
}

// Validate date (simple validation)
bool isValidDate(uint8_t day, uint8_t month, uint16_t year) {
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;
    if (year < 2000 || year > 2099) return false;
    
    // Check days in month
    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // Handle leap year for February
    if (month == 2) {
        bool isLeapYear = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (isLeapYear && day <= 29) return true;
        if (!isLeapYear && day <= 28) return true;
        return false;
    }
    
    return day <= daysInMonth[month - 1];
}

// Calculate moving average (for smoothing values)
float calculateMovingAverage(float newValue, float oldAverage, float alpha) {
    // alpha: 0.0-1.0 (higher = more responsive, lower = more smoothing)
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;
    
    return (alpha * newValue) + ((1.0 - alpha) * oldAverage);
}

// Constrain float value
float constrainFloat(float value, float minVal, float maxVal) {
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

// Check for significant change
bool hasSignificantChange(float newValue, float oldValue, float threshold) {
    if (threshold <= 0.0) return true; // Always update if no threshold
    
    float diff = fabs(newValue - oldValue);
    return (diff >= threshold);
}

// Pad number with leading zeros
String padNumber(int number, int width, char padChar) {
    String result = String(number);
    while (result.length() < width) {
        result = String(padChar) + result;
    }
    return result;
}

// Format float with specific decimal places
String formatFloat(float value, int decimalPlaces) {
    char buffer[20];
    String format = "%." + String(decimalPlaces) + "f";
    snprintf(buffer, sizeof(buffer), format.c_str(), value);
    return String(buffer);
}

// Truncate string if too long
String truncateString(const String& str, int maxLength) {
    if (str.length() <= maxLength) return str;
    return str.substring(0, maxLength - 3) + "...";
}

// Calculate time since timestamp
unsigned long timeSince(unsigned long timestamp) {
    unsigned long now = millis();
    
    // Handle millis() overflow (every ~50 days)
    if (now < timestamp) {
        return (ULONG_MAX - timestamp) + now;
    }
    
    return now - timestamp;
}

// Check if time has elapsed
bool isTimeElapsed(unsigned long timestamp, unsigned long interval) {
    return timeSince(timestamp) >= interval;
}

// Calculate delta time
unsigned long calculateDeltaTime(unsigned long lastTime) {
    unsigned long now = millis();
    
    if (now < lastTime) {
        // Handle overflow
        return (ULONG_MAX - lastTime) + now;
    }
    
    return now - lastTime;
}
