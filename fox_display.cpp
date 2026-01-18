#include "fox_display.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <math.h>
#include "fox_config.h"
#include "fox_rtc.h"
#include "fox_vehicle.h"
#include <Fonts/FreeSansBold18pt7b.h>

// =============================================
// GLOBAL VARIABLES
// =============================================

// Display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// State variables
bool displayInitialized = false;
bool displayUpdateInProgress = false;
unsigned long displayUpdateStartTime = 0;

// Smart update tracking
int smartUpdateCount = 0;
int fallbackUpdateCount = 0;
int displayErrorCount = 0;

// I2C error tracking
int i2cErrorCount = 0;
unsigned long lastI2CErrorTime = 0;
unsigned long lastI2CCheck = 0;

// Display state caching (for partial updates)
struct DisplayCache {
    // Clock page
    int lastHour = -1;
    int lastMinute = -1;
    int lastDay = -1;
    int lastMonth = -1;
    int lastYear = -1;
    int lastDayOfWeek = -1;
    
    // Temperature page
    uint8_t lastTempCtrl = 0;
    uint8_t lastTempMotor = 0;
    uint8_t lastTempBatt = 0;
    
    // Electrical page
    float lastVoltage = 0;
    float lastCurrent = 0;
    
    // Sport page
    uint16_t lastSpeed = 0;
    uint16_t lastRPM = 0;
    FoxVehicleMode lastMode = MODE_UNKNOWN;
    
    // Dirty flags for each zone
    bool zonesDirty[13] = {false};
    
    // Current page being displayed
    int currentDisplayedPage = -1;
} displayCache;

// Blink state for setup/cruise mode
unsigned long lastBlinkTime = 0;
bool blinkState = true;

// Charging mode tracking
unsigned long lastChargingUpdate = 0;
uint8_t lastChargingSOC = 0;
float lastChargingVoltage = 0;

// Watchdog tracking
unsigned long lastSuccessfulDisplay = 0;
unsigned long displayWatchdogTimeout = DISPLAY_WATCHDOG_TIMEOUT_MS;

// Splash screen tracking
bool splashScreenShown = false;

// =============================================
// WATCHDOG & RECOVERY FUNCTIONS
// =============================================

void foxDisplayWatchdogCheck() {
    unsigned long now = millis();
    
    // Check if display is stuck in update
    if (displayUpdateInProgress && (now - displayUpdateStartTime > displayWatchdogTimeout)) {
        Serial.println("[WATCHDOG] Display update timeout! Recovering...");
        displayErrorCount++;
        
        // Cancel current update
        displayUpdateInProgress = false;
        
        // Try soft recovery first
        recoverI2C();
        
        // Reset display if still stuck
        if (displayUpdateInProgress) {
            Serial.println("[WATCHDOG] Soft recovery failed, hard resetting...");
            foxDisplayInitEnhanced();
        }
    }
    
    // Check for complete display freeze
    if (displayInitialized && (now - lastSuccessfulDisplay > DISPLAY_WATCHDOG_TIMEOUT_MS * 2)) {
        Serial.println("[WATCHDOG] No successful updates for too long!");
        foxDisplayInitEnhanced();
        lastSuccessfulDisplay = now;
    }
}

// =============================================
// I2C RECOVERY FUNCTIONS
// =============================================

void recoverI2C() {
    Serial.println("=== I2C RECOVERY START ===");
    
    // Stop I2C
    Wire.end();
    
    // Reset pins
    pinMode(SDA_PIN, INPUT);
    pinMode(SCL_PIN, INPUT);
    delay(100);
    
    // Generate clock pulses to clear stuck devices
    pinMode(SCL_PIN, OUTPUT);
    Serial.print("Sending clock pulses...");
    for(int i = 0; i < 20; i++) {
        digitalWrite(SCL_PIN, LOW);
        delayMicroseconds(10);
        digitalWrite(SCL_PIN, HIGH);
        delayMicroseconds(10);
    }
    Serial.println("Done");
    
    // Reinitialize with pull-up
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, INPUT_PULLUP);
    delay(100);
    
    Wire.begin(SDA_PIN, SCL_PIN);
    
    // Use slower speed if having issues
    FoxVehicleData data = foxVehicleGetData();
    if(data.mode == MODE_CHARGING) {
        Wire.setClock(50000); // 50kHz saat charging
    } else {
        Wire.setClock(100000); // 100kHz normal
    }
    
    // Test connection
    byte error;
    bool oledFound = false;
    uint8_t oledAddresses[] = {0x3C, 0x3D};
    
    for(int i = 0; i < 2; i++) {
        Wire.beginTransmission(oledAddresses[i]);
        error = Wire.endTransmission();
        
        if(error == 0) {
            Serial.print("OLED found at 0x");
            Serial.println(oledAddresses[i], HEX);
            
            // Reinitialize display
            if(display.begin(SSD1306_SWITCHCAPVCC, oledAddresses[i])) {
                displayInitialized = true;
                oledFound = true;
                Serial.println("OLED reinitialized successfully");
                break;
            }
        }
    }
    
    if(!oledFound) {
        Serial.println("OLED NOT FOUND after recovery");
        displayInitialized = false;
    }
    
    // Clear the display
    if(displayInitialized) {
        display.clearDisplay();
        display.display();
    }
    
    Serial.println("=== I2C RECOVERY END ===");
}

bool checkI2CHealth() {
    FoxVehicleData data = foxVehicleGetData();
    
    // Check less frequently during charging
    unsigned long checkInterval = (data.mode == MODE_CHARGING) ? 10000 : 1000;
    
    if(millis() - lastI2CCheck < checkInterval) {
        return displayInitialized;
    }
    
    lastI2CCheck = millis();
    
    if(!displayInitialized) {
        return false;
    }
    
    // Test OLED connection
    Wire.beginTransmission(OLED_ADDRESS);
    byte error = Wire.endTransmission();
    
    if(error != 0) {
        i2cErrorCount++;
        lastI2CErrorTime = millis();
        
        // Throttled logging
        static unsigned long lastErrorLog = 0;
        if(millis() - lastErrorLog > 5000) {
            Serial.print("[I2C] Error #");
            Serial.print(i2cErrorCount);
            Serial.print(" Code: ");
            Serial.println(error);
            lastErrorLog = millis();
        }
        
        // Auto-recovery after multiple errors
        if(i2cErrorCount >= 3) {
            Serial.println("[I2C] Multiple errors, attempting recovery...");
            recoverI2C();
            i2cErrorCount = 0;
        }
        
        return false;
    }
    
    // Reset error count on success
    if(i2cErrorCount > 0) {
        i2cErrorCount = 0;
    }
    
    return true;
}

// =============================================
// INITIALIZATION FUNCTIONS
// =============================================

void foxDisplayInit() {
    foxDisplayInitEnhanced(); // Use enhanced by default
}

void foxDisplayInitEnhanced() {
    Serial.println("Initializing OLED (Enhanced)...");
    
    // Initialize I2C
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000); // 100kHz
    delay(100);
    
    // Try multiple OLED addresses
    uint8_t oledAddresses[] = {0x3C, 0x3D};
    bool oledFound = false;
    byte error;
    
    for(int i = 0; i < 2; i++) {
        Wire.beginTransmission(oledAddresses[i]);
        error = Wire.endTransmission();
        
        if(error == 0) {
            Serial.print("OLED found at 0x");
            Serial.println(oledAddresses[i], HEX);
            
            if(!display.begin(SSD1306_SWITCHCAPVCC, oledAddresses[i])) {
                Serial.println("OLED init failed at this address");
            } else {
                Serial.println("OLED initialized successfully!");
                displayInitialized = true;
                oledFound = true;
                
                // Reset cache
                displayCache.currentDisplayedPage = -1;
                splashScreenShown = false;
                
                break;
            }
        }
        delay(50);
    }
    
    if(!oledFound) {
        Serial.println("OLED NOT FOUND at any address!");
        displayInitialized = false;
        return;
    }
    
    // ========== SPLASH SCREEN ==========
    display.clearDisplay();
    display.setTextSize(FONT_SIZE_MEDIUM);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 10);
    display.print(SPLASH_TEXT);
    display.display();
    
    delay(SPLASH_DURATION_MS);
    splashScreenShown = true;
    
    // Initial display - show default clock page
    display.clearDisplay();
    displayCache.currentDisplayedPage = PAGE_CLOCK;
    lastSuccessfulDisplay = millis();
    
    Serial.println("Display ready - showing default clock page");
}

bool foxDisplayIsInitialized() {
    return displayInitialized;
}

// =============================================
// SMART UPDATE SYSTEM (MAIN FUNCTION)
// =============================================

void foxDisplayUpdateSmart(int page, bool forceFullUpdate) {
    if(!displayInitialized) {
        return;
    }
    
    // Check I2C health first
    if(!checkI2CHealth()) {
        fallbackUpdateCount++;
        return;
    }
    
    // Prevent overlapping updates
    if(displayUpdateInProgress) {
        return;
    }
    
    displayUpdateInProgress = true;
    displayUpdateStartTime = millis();
    
    try {
        // Get current vehicle data
        FoxVehicleData vehicleData = foxVehicleGetData();
        
        // Handle special modes first
        if(vehicleData.mode == MODE_CHARGING) {
            // Charging mode has its own simple display
            foxDisplayUpdateChargingMode();
            smartUpdateCount++;
            displayUpdateInProgress = false;
            lastSuccessfulDisplay = millis();
            return;
        }
        
        // Check if page has changed or force update is requested
        bool pageChanged = (displayCache.currentDisplayedPage != page);
        
        if(forceFullUpdate || pageChanged) {
            // Full redraw for page switches
            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);
            display.setFont();
            display.setTextSize(FONT_SIZE_SMALL);
            
            switch(page) {
                case PAGE_CLOCK:
                    displayPageClock(true);
                    break;
                case PAGE_TEMP:
                    displayPageTemperature(vehicleData, true);
                    break;
                case PAGE_ELECTRICAL:
                    displayPageElectrical(vehicleData, true);
                    break;
                case PAGE_SPORT:
                    displayPageSport(vehicleData, true);
                    break;
                default:
                    // Fallback to clock
                    page = PAGE_CLOCK;
                    displayPageClock(true);
                    break;
            }
            
            display.display();
            displayCache.currentDisplayedPage = page;
            fallbackUpdateCount++;
        } else {
            // Smart partial update - ONLY for current page
            if(displayCache.currentDisplayedPage == page) {
                updateDirtyZones(vehicleData);
                smartUpdateCount++;
            }
        }
        
        displayUpdateInProgress = false;
        lastSuccessfulDisplay = millis();
        
    } catch(...) {
        // Catch any display errors
        Serial.println("[DISPLAY] Exception during update!");
        displayUpdateInProgress = false;
        recoverI2C();
        fallbackUpdateCount++;
    }
}

bool foxDisplayCheckUpdateNeeded(const FoxVehicleData& vehicleData) {
    // Check if any data has changed significantly
    
    // Check current changes
    if(fabs(vehicleData.current - displayCache.lastCurrent) > CURRENT_UPDATE_THRESHOLD) {
        return true;
    }
    
    // Check voltage changes
    if(fabs(vehicleData.voltage - displayCache.lastVoltage) > VOLTAGE_UPDATE_THRESHOLD) {
        return true;
    }
    
    // Check speed changes (only relevant for sport page)
    if(displayCache.currentDisplayedPage == PAGE_SPORT) {
        if(abs(vehicleData.speedKmh - displayCache.lastSpeed) > SPEED_UPDATE_THRESHOLD) {
            return true;
        }
    }
    
    // Check temperature changes
    if(abs(vehicleData.tempController - displayCache.lastTempCtrl) > TEMP_UPDATE_THRESHOLD ||
       abs(vehicleData.tempMotor - displayCache.lastTempMotor) > TEMP_UPDATE_THRESHOLD ||
       abs(vehicleData.tempBattery - displayCache.lastTempBatt) > TEMP_UPDATE_THRESHOLD) {
        return true;
    }
    
    // Check mode changes
    if(vehicleData.mode != displayCache.lastMode) {
        return true;
    }
    
    // Check time changes (for clock page)
    if(displayCache.currentDisplayedPage == PAGE_CLOCK) {
        RTCDateTime dt = foxRTCGetDateTime();
        if(dt.minute != displayCache.lastMinute) {
            return true;
        }
    }
    
    return false;
}

// =============================================
// PARTIAL UPDATE FUNCTIONS
// =============================================

void updateDirtyZones(const FoxVehicleData& vehicleData) {
    bool anythingUpdated = false;
    
    // Only update zones relevant to current page
    switch(displayCache.currentDisplayedPage) {
        case PAGE_CLOCK: {
            // Update clock if minute changed
            RTCDateTime dt = foxRTCGetDateTime();
            if(dt.minute != displayCache.lastMinute || dt.hour != displayCache.lastHour) {
                updateClockPartial();
                anythingUpdated = true;
                displayCache.lastHour = dt.hour;
                displayCache.lastMinute = dt.minute;
            }
            break;
        }
            
        case PAGE_TEMP: {
            // Update temperatures if changed
            if(abs(vehicleData.tempController - displayCache.lastTempCtrl) > TEMP_UPDATE_THRESHOLD ||
               abs(vehicleData.tempMotor - displayCache.lastTempMotor) > TEMP_UPDATE_THRESHOLD ||
               abs(vehicleData.tempBattery - displayCache.lastTempBatt) > TEMP_UPDATE_THRESHOLD) {
                
                updateTemperaturePartial(vehicleData.tempController, 
                                        vehicleData.tempMotor, 
                                        vehicleData.tempBattery);
                anythingUpdated = true;
                
                displayCache.lastTempCtrl = vehicleData.tempController;
                displayCache.lastTempMotor = vehicleData.tempMotor;
                displayCache.lastTempBatt = vehicleData.tempBattery;
            }
            break;
        }
            
        case PAGE_ELECTRICAL: {
            // Update current if changed
            if(fabs(vehicleData.current - displayCache.lastCurrent) > CURRENT_UPDATE_THRESHOLD) {
                updateCurrentPartial(vehicleData.current);
                anythingUpdated = true;
                displayCache.lastCurrent = vehicleData.current;
            }
            
            // Update voltage if changed
            if(fabs(vehicleData.voltage - displayCache.lastVoltage) > VOLTAGE_UPDATE_THRESHOLD) {
                updateVoltagePartial(vehicleData.voltage);
                anythingUpdated = true;
                displayCache.lastVoltage = vehicleData.voltage;
            }
            break;
        }
            
        case PAGE_SPORT: {
            // Handle transisi antara mode "SPORT" dan "SPORT + SPEED"
            bool currentlyShowingSpeed = (displayCache.lastSpeed >= SPEED_TRIGGER_SPORT_PAGE);
            bool shouldShowSpeed = (vehicleData.speedKmh >= SPEED_TRIGGER_SPORT_PAGE);
            
            // Jika ada perubahan mode (dari show speed ke tidak, atau sebaliknya)
            if(currentlyShowingSpeed != shouldShowSpeed) {
                // Perubahan mode membutuhkan full redraw
                displayCache.lastSpeed = vehicleData.speedKmh;
                // Force full update di main loop
                anythingUpdated = false; // Akan trigger full update
                break;
            }
            
            // Jika tetap di mode yang sama, update speed jika berubah
            if(shouldShowSpeed && abs(vehicleData.speedKmh - displayCache.lastSpeed) > SPEED_UPDATE_THRESHOLD) {
                updateSpeedPartial(vehicleData.speedKmh);
                anythingUpdated = true;
                displayCache.lastSpeed = vehicleData.speedKmh;
            }
            break;
        }
    }
    
    // Update mode if changed (affects all pages)
    if(vehicleData.mode != displayCache.lastMode) {
        displayCache.lastMode = vehicleData.mode;
        // Mode changes require full page update
        anythingUpdated = false; // Force full update via main loop
    }
    
    // Only update display if something changed
    if(anythingUpdated) {
        display.display();
    }
}

void updateClockPartial() {
    RTCDateTime dt = foxRTCGetDateTime();
    
    // Clear time area first
    display.fillRect(0, 10, 84, 28, SSD1306_BLACK);
    
    // Update time (large font)
    display.setFont(&FreeSansBold18pt7b);
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), CLOCK_TIME_FORMAT, dt.hour, dt.minute);
    display.setCursor(0, 28);
    display.print(timeStr);
    
    // Restore default font for other text
    display.setFont();
    display.setTextSize(FONT_SIZE_SMALL);
}

void updateCurrentPartial(float current) {
    // Update only current area (right side)
    int currentX = 86, currentY = 16;
    int currentWidth = 40, currentHeight = 16;
    
    // Clear area
    display.fillRect(currentX, currentY, currentWidth, currentHeight, SSD1306_BLACK);
    
    // Draw new current
    display.setTextSize(FONT_SIZE_MEDIUM);
    display.setCursor(currentX, currentY);
    
    float absCurrent = fabs(current);
    
    if(absCurrent < 0.1) {
        display.print("  0");
    } else if(current >= 0) {
        if(absCurrent < 10.0) {
            display.print("+");
            display.print(current, 1);
        } else {
            display.print("+");
            display.print((int)current);
        }
    } else {
        if(absCurrent < 10.0) {
            display.print(current, 1);
        } else {
            display.print((int)current);
        }
    }
}

void updateVoltagePartial(float voltage) {
    // Update only voltage area (left side)
    int voltageX = 0, voltageY = 16;
    int voltageWidth = 40, voltageHeight = 16;
    
    // Clear area
    display.fillRect(voltageX, voltageY, voltageWidth, voltageHeight, SSD1306_BLACK);
    
    // Draw new voltage
    display.setTextSize(FONT_SIZE_MEDIUM);
    display.setCursor(voltageX, voltageY);
    
    if(voltage < 0.1) {
        display.print("  0");
    } else if(voltage < 10.0) {
        display.print(voltage, 1);
    } else if(voltage < 100.0) {
        display.print(voltage, 1);
    } else {
        display.print((int)voltage);
    }
}

void updateSpeedPartial(uint16_t speed) {
    // Hanya update jika di sport page
    if(displayCache.currentDisplayedPage != PAGE_SPORT) {
        return;
    }
    
    // Jika speed < 80, TIDAK PERLU UPDATE APAPUN
    if(speed < SPEED_TRIGGER_SPORT_PAGE) {
        return;
    }
    
    // Speed ≥ 80 km/h: update area speed
    int speedX = 0, speedY = 10;
    int speedWidth = 54, speedHeight = 24;
    
    // Clear area speed
    display.fillRect(speedX, speedY, speedWidth, speedHeight, SSD1306_BLACK);
    
    // Draw new speed
    display.setTextSize(FONT_SIZE_LARGE);
    display.setCursor(speedX, speedY);
    display.print(speed);
    
    // Update km/h label
    display.setTextSize(FONT_SIZE_SMALL);
    display.setCursor(speedWidth + 4, 18);
    display.print(KMH_TEXT);
}

void updateTemperaturePartial(uint8_t tempCtrl, uint8_t tempMotor, uint8_t tempBatt) {
    // Update temperature areas
    
    // ECU temp
    display.fillRect(0, 16, 40, 16, SSD1306_BLACK);
    display.setTextSize(FONT_SIZE_MEDIUM);
    display.setCursor(0, 16);
    display.print(tempCtrl);
    
    // Motor temp
    display.fillRect(43, 16, 40, 16, SSD1306_BLACK);
    display.setCursor(43, 16);
    display.print(tempMotor);
    
    // Battery temp
    display.fillRect(86, 16, 40, 16, SSD1306_BLACK);
    display.setCursor(86, 16);
    display.print(tempBatt);
}

// =============================================
// PAGE DISPLAY FUNCTIONS (FULL REDRAW)
// =============================================

void displayPageClock(bool forceFull) {
    // Clear display and set default font
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(FONT_SIZE_SMALL);
    display.setFont();
    
    RTCDateTime dt = foxRTCGetDateTime();
    
    // Large clock - only use large font for time
    display.setFont(&FreeSansBold18pt7b);
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), CLOCK_TIME_FORMAT, dt.hour, dt.minute);
    
    int timeX = 0;
    int timeY = 28;
    
    display.setCursor(timeX, timeY);
    display.print(timeStr);
    
    // Date info - switch back to default font
    display.setFont();
    display.setTextSize(FONT_SIZE_SMALL);
    
    const char* hari[] = {"MINGGU", "SENIN", "SELASA", "RABU", "KAMIS", "JUMAT", "SABTU"};
    const char* bulan[] = {"JAN", "FEB", "MAR", "APR", "MEI", "JUN", 
                          "JUL", "AGU", "SEP", "OKT", "NOV", "DES"};
    
    int hariIndex = dt.dayOfWeek - 1;
    if(hariIndex < 0) hariIndex = 0;
    if(hariIndex > 6) hariIndex = 6;
    
    int bulanIndex = dt.month - 1;
    if(bulanIndex < 0) bulanIndex = 0;
    if(bulanIndex > 11) bulanIndex = 11;
    
    int rightCol = 88;
    
    display.setCursor(rightCol, 5);
    display.print(hari[hariIndex]);
    
    char tanggalBulan[10];
    snprintf(tanggalBulan, sizeof(tanggalBulan), CLOCK_DATE_FORMAT, dt.day, bulan[bulanIndex]);
    display.setCursor(rightCol, 15);
    display.print(tanggalBulan);
    
    char yearStr[5];
    snprintf(yearStr, sizeof(yearStr), CLOCK_YEAR_FORMAT, dt.year);
    display.setCursor(rightCol, 25);
    display.print(yearStr);
    
    // Update cache
    displayCache.lastHour = dt.hour;
    displayCache.lastMinute = dt.minute;
    displayCache.lastDay = dt.day;
    displayCache.lastMonth = dt.month;
    displayCache.lastYear = dt.year;
    displayCache.lastDayOfWeek = dt.dayOfWeek;
}

void displayPageTemperature(const FoxVehicleData& vehicleData, bool forceFull) {
    // Clear and set default font
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(FONT_SIZE_SMALL);
    
    // Labels
    display.setCursor(0, 4);
    display.print(TEMP_LABEL_ECU);
    display.setCursor(43, 4);
    display.print(TEMP_LABEL_MOTOR);
    display.setCursor(86, 4);
    display.print(TEMP_LABEL_BATT);
    
    // Values
    display.setTextSize(FONT_SIZE_MEDIUM);
    display.setCursor(0, 16);
    display.print(vehicleData.tempController);
    display.setCursor(43, 16);
    display.print(vehicleData.tempMotor);
    display.setCursor(86, 16);
    display.print(vehicleData.tempBattery);
    
    // Update cache
    displayCache.lastTempCtrl = vehicleData.tempController;
    displayCache.lastTempMotor = vehicleData.tempMotor;
    displayCache.lastTempBatt = vehicleData.tempBattery;
}

void displayPageElectrical(const FoxVehicleData& vehicleData, bool forceFull) {
    #if PAGE_ELECTRICAL_ENABLED
        // Clear and set default font
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(FONT_SIZE_SMALL);
        
        // Labels
        display.setCursor(0, 4);
        display.print(ELECTRICAL_LABEL_VOLT);
        display.setCursor(86, 4);
        display.print(ELECTRICAL_LABEL_CURR);
        
        // Voltage
        display.setTextSize(FONT_SIZE_MEDIUM);
        float voltage = vehicleData.voltage;
        
        if(voltage < 0.1) {
            display.setCursor(0, 16);
            display.print("  0");
        } else if(voltage < 10.0) {
            char voltStr[6];
            snprintf(voltStr, sizeof(voltStr), "%4.1f", voltage);
            display.setCursor(0, 16);
            display.print(voltStr);
        } else if(voltage < 100.0) {
            char voltStr[6];
            snprintf(voltStr, sizeof(voltStr), "%4.1f", voltage);
            display.setCursor(0, 16);
            display.print(voltStr);
        } else {
            display.setCursor(0, 16);
            display.print((int)voltage);
        }
        
        // Current
        float current = vehicleData.current;
        float absCurrent = fabs(current);
        
        if(absCurrent < 0.1) {
            display.setCursor(86, 16);
            display.print("  0");
        } else {
            if(current >= 0) {
                if(absCurrent < 10.0) {
                    char currStr[6];
                    snprintf(currStr, sizeof(currStr), "+%3.1f", current);
                    display.setCursor(76, 16);
                    display.print(currStr);
                } else if(absCurrent < 100.0) {
                    display.setCursor(71, 16);
                    display.print("+");
                    display.print((int)current);
                } else {
                    display.setCursor(66, 16);
                    display.print("+");
                    display.print((int)current);
                }
            } else {
                if(absCurrent < 10.0) {
                    char currStr[6];
                    snprintf(currStr, sizeof(currStr), "%4.1f", current);
                    display.setCursor(76, 16);
                    display.print(currStr);
                } else if(absCurrent < 100.0) {
                    display.setCursor(71, 16);
                    display.print((int)current);
                } else {
                    display.setCursor(66, 16);
                    display.print((int)current);
                }
            }
        }
        
        // Update cache
        displayCache.lastVoltage = voltage;
        displayCache.lastCurrent = current;
        
    #else
        // Page disabled message
        display.clearDisplay();
        display.setTextSize(FONT_SIZE_MEDIUM);
        display.setCursor(10, 12);
        display.print("PAGE 3 DISABLED");
    #endif
}

void displayPageSport(const FoxVehicleData& vehicleData, bool forceFull) {
    bool isCruiseMode = (vehicleData.mode == MODE_CRUISE || 
                        vehicleData.mode == MODE_SPORT_CRUISE);
    bool isSportMode = (vehicleData.mode == MODE_SPORT || 
                       vehicleData.mode == MODE_SPORT_CRUISE);
    
    if(isCruiseMode) {
        // Cruise mode: hanya update cache, display dihandle oleh foxDisplayUpdateCruiseMode()
        displayCache.lastSpeed = vehicleData.speedKmh;
        displayCache.lastRPM = vehicleData.rpm;
        displayCache.lastMode = vehicleData.mode;
        return;
    }
    
    // Clear and set default font untuk sport mode
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(FONT_SIZE_SMALL);
    
    if(isSportMode) {
        if(vehicleData.speedKmh < SPEED_TRIGGER_SPORT_PAGE) {
            displaySportModeLowSpeed();
        } else {
            displaySportModeHighSpeed(vehicleData.speedKmh);
        }
    } else {
        display.setTextSize(FONT_SIZE_MEDIUM);
        display.setCursor(20, 10);
        display.print("SPORT PAGE");
    }
    
    // Update cache
    displayCache.lastSpeed = vehicleData.speedKmh;
    displayCache.lastRPM = vehicleData.rpm;
    displayCache.lastMode = vehicleData.mode;
}

// =============================================
// SPECIAL MODE FUNCTIONS
// =============================================

void foxDisplayUpdateChargingMode() {
    unsigned long now = millis();
    
    FoxVehicleData data = foxVehicleGetData();
    
    bool socChanged = (abs((int)data.soc - (int)lastChargingSOC) > 1);
    bool voltChanged = (fabs(data.voltage - lastChargingVoltage) > 0.5);
    bool timeout = (now - lastChargingUpdate > 30000);
    
    if(!socChanged && !voltChanged && !timeout) {
        return;
    }
    
    // Simple display for charging mode
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(15, 0);
    display.print("CHARGING");
    
    display.setTextSize(1);
    RTCDateTime dt = foxRTCGetDateTime();
    String currentTime = String(dt.hour) + ":" + (dt.minute < 10 ? "0" : "") + String(dt.minute);
    display.setCursor(48, 20);
    display.print(currentTime);
    
    display.display();
    
    // Update trackers
    lastChargingSOC = data.soc;
    lastChargingVoltage = data.voltage;
    lastChargingUpdate = now;
    
    // Update cache
    displayCache.currentDisplayedPage = PAGE_CLOCK;
}

void foxDisplayShowSetupMode(bool blinkState) {
    if(!displayInitialized) return;
    
    display.clearDisplay();
    display.setTextSize(FONT_SIZE_MEDIUM);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 10);
    if (blinkState) {
        display.print(SETUP_TEXT);
    }
    display.display();
}

void foxDisplayUpdateCruiseMode(bool blinkState) {
    if(!displayInitialized) return;
    
    // Prevent overlapping updates
    if(displayUpdateInProgress) {
        return;
    }
    
    displayUpdateInProgress = true;
    displayUpdateStartTime = millis();
    
    try {
        // Clear display
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        
        // Only show text when blinkState is true
        if(blinkState) {
            display.setTextSize(FONT_SIZE_LARGE);
            int textWidth = strlen(CRUISE_TEXT) * 18;
            int xPos = (SCREEN_WIDTH - textWidth) / 2;
            int yPos = 4;
            
            display.setCursor(xPos, yPos);
            display.print(CRUISE_TEXT);
        }
        
        // Update display
        display.display();
        
        displayUpdateInProgress = false;
        lastSuccessfulDisplay = millis();
        
    } catch(...) {
        Serial.println("[DISPLAY] Exception in cruise mode!");
        displayUpdateInProgress = false;
        recoverI2C();
    }
}

// =============================================
// HELPER FUNCTIONS
// =============================================

void updateBlinkState() {
    if(millis() - lastBlinkTime > BLINK_INTERVAL_MS) {
        blinkState = !blinkState;
        lastBlinkTime = millis();
    }
}

void displayCruiseMode() {
    // Fungsi ini untuk compatibility
    display.setTextSize(FONT_SIZE_LARGE);
    int textWidth = strlen(CRUISE_TEXT) * 18;
    int xPos = (SCREEN_WIDTH - textWidth) / 2;
    int yPos = 4;
    
    display.setCursor(xPos, yPos);
    if(blinkState) {
        display.print(CRUISE_TEXT);
    }
}

void displaySportModeLowSpeed() {
    // "SPORT" besar di tengah layar (tanpa speed)
    display.setTextSize(FONT_SIZE_LARGE);
    int textWidth = strlen(SPORT_TEXT) * 18;
    int xPos = (SCREEN_WIDTH - textWidth) / 2;
    int yPos = 4;
    
    display.setCursor(xPos, yPos);
    display.print(SPORT_TEXT);
}

void displaySportModeHighSpeed(uint16_t speedKmh) {
    // "SPORT MODE" kecil di atas, speed besar di kiri
    display.setTextSize(FONT_SIZE_SMALL);
    int sportWidth = strlen(SPORT_MODE_LABEL) * 6;
    int sportX = (SCREEN_WIDTH - sportWidth) / 2;
    display.setCursor(sportX, POS_TOP);
    display.print(SPORT_MODE_LABEL);
    
    display.setTextSize(FONT_SIZE_LARGE);
    char speedStr[5];
    snprintf(speedStr, sizeof(speedStr), "%3d", speedKmh);
    display.setCursor(0, 10);
    display.print(speedStr);
    
    display.setTextSize(FONT_SIZE_SMALL);
    int speedWidth = 54;
    int kmhX = speedWidth + 4;
    int kmhY = 18;
    display.setCursor(kmhX, kmhY);
    display.print(KMH_TEXT);
}

int foxDisplayGetSmartUpdateCount() {
    return smartUpdateCount;
}

int foxDisplayGetFallbackUpdateCount() {
    return fallbackUpdateCount;
}

int foxDisplayGetSmartUpdateSuccessRate() {
    int totalUpdates = smartUpdateCount + fallbackUpdateCount;
    if(totalUpdates == 0) return 0;
    return (smartUpdateCount * 100) / totalUpdates;
}

int getI2CErrorCount() {
    return i2cErrorCount;
}

unsigned long getLastI2CErrorTime() {
    return lastI2CErrorTime;
}

// =============================================
// ZONE MANAGEMENT FUNCTIONS (for future expansion)
// =============================================

void markZoneDirty(DisplayZone zone) {
    if(zone >= 0 && zone < 13) {
        displayCache.zonesDirty[zone] = true;
    }
}

void clearAllZonesDirty() {
    for(int i = 0; i < 13; i++) {
        displayCache.zonesDirty[i] = false;
    }
}

bool isAnyZoneDirty() {
    for(int i = 0; i < 13; i++) {
        if(displayCache.zonesDirty[i]) {
            return true;
        }
    }
    return false;
}
