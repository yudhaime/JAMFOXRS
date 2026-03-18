#include "fox_display.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "fox_config.h"
#include "fox_rtc.h"
#include "fox_canbus.h"
#include "fox_serial.h"
#include "fox_page.h"
#include "fox_ble.h"
#include "fox_task.h"
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

// =============================================
// GLOBAL VARIABLES
// =============================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool displayInitialized = false;
bool displayReady = false;

#ifdef ESP32
extern SemaphoreHandle_t i2cMutex;
QueueHandle_t displayQueue = NULL;
#endif

// =============================================
// I2C SAFETY VARIABLES
// =============================================
static volatile bool i2cOperationInProgress = false;
static unsigned long lastI2CFailure = 0;
static uint32_t i2cFailureCount = 0;

// =============================================
// ANIMATION VARIABLES
// =============================================
static float animatedVoltage = 0.0f;
static float animatedCurrent = 0.0f;
static float animatedPower = 0.0f;
static float targetVoltage = 0.0f;
static float targetCurrent = 0.0f;
static float targetPower = 0.0f;
static unsigned long lastAnimationUpdate = 0;
static bool animationInitialized = false;

static float currentVelocity = 0.0f;
static const float MAX_VELOCITY = 5.0f;
static const float ACCELERATION = 10.0f;
static const float DAMPING = 0.9f;

// =============================================
// DISPLAY STATE VARIABLES
// =============================================
bool appModeDisplayActive = false;
unsigned long lastDisplayUpdateTime = 0;

// =============================================
// EXTERNAL VARIABLES FROM BLE
// =============================================
extern volatile bool deviceConnected;

// =============================================
// I2C RECOVERY FUNCTIONS
// =============================================
void recoverI2CBus() {
    serialPrintf("[I2C-RECOVERY] Starting recovery...\n");
    
    #ifdef ESP32
    if(i2cMutex != NULL) {
        xSemaphoreGive(i2cMutex);
    }
    #endif
    
    Wire.end();
    delay(100);
    
    #ifdef ESP32
    gpio_reset_pin((gpio_num_t)SDA_PIN);
    gpio_reset_pin((gpio_num_t)SCL_PIN);
    delay(10);
    #endif
    
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(50000);
    #ifdef ESP32
    Wire.setTimeOut(1000);
    #endif
    
    delay(100);
    
    for(int i = 0; i < 3; i++) {
        Wire.beginTransmission(OLED_ADDRESS);
        if(Wire.endTransmission() == 0) {
            serialPrintf("[I2C-RECOVERY] Success\n");
            
            if(display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
                displayInitialized = true;
                displayReady = true;
                display.clearDisplay();
                display.display();
                i2cFailureCount = 0;
            }
            break;
        }
        delay(50);
    }
    
    #ifdef ESP32
    if(i2cMutex != NULL) {
        xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100));
    }
    #endif
}

// =============================================
// HARD RESET I2C - TARIK PIN KE LOW
// =============================================
void hardResetI2C() {
    serialPrintflnAlways("[I2C] HARD RESET - Pulling pins LOW");
    
    #ifdef ESP32
    // Pull SDA and SCL LOW to reset I2C bus
    pinMode(SDA_PIN, OUTPUT);
    pinMode(SCL_PIN, OUTPUT);
    digitalWrite(SDA_PIN, LOW);
    digitalWrite(SCL_PIN, LOW);
    delay(10);
    
    // Release pins
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, INPUT_PULLUP);
    delay(10);
    
    // Restart I2C
    Wire.end();
    delay(100);
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(50000);
    
    // Re-init display
    if(display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        displayInitialized = true;
        displayReady = true;
        serialPrintflnAlways("[I2C] HARD RESET successful");
    } else {
        serialPrintflnAlways("[I2C] HARD RESET failed - display not responding");
    }
    #endif
}

// =============================================
// SAFE I2C OPERATION (ORIGINAL)
// =============================================
bool safeI2COperation(uint32_t timeoutMs) {
    if(!displayInitialized) return false;
    
    #ifdef ESP32
    if(i2cOperationInProgress) return false;
    i2cOperationInProgress = true;
    
    if(i2cMutex != NULL) {
        if(xSemaphoreTake(i2cMutex, 0) != pdTRUE) {
            i2cOperationInProgress = false;
            return false;
        }
    }
    #endif
    
    unsigned long startTime = micros();
    uint8_t error = 5;
    
    while(micros() - startTime < 500) {
        Wire.beginTransmission(OLED_ADDRESS);
        Wire.write(0x00);
        error = Wire.endTransmission(true);
        
        if(error == 0) break;
        delayMicroseconds(10);
    }
    
    bool success = (error == 0);
    
    #ifdef ESP32
    if(i2cMutex != NULL) xSemaphoreGive(i2cMutex);
    i2cOperationInProgress = false;
    #endif
    
    if(!success) {
        lastI2CFailure = millis();
        i2cFailureCount++;
        
        if(i2cFailureCount >= 3) recoverI2CBus();
    }
    
    return success;
}

// =============================================
// ENHANCED I2C OPERATION WITH EXPONENTIAL BACKOFF
// =============================================
bool safeI2COperationWithBackoff(uint32_t timeoutMs) {
    if(!displayInitialized) return false;
    
    #ifdef ESP32
    if(i2cOperationInProgress) return false;
    i2cOperationInProgress = true;
    
    if(i2cMutex != NULL) {
        if(xSemaphoreTake(i2cMutex, 0) != pdTRUE) {
            i2cOperationInProgress = false;
            return false;
        }
    }
    #endif
    
    bool success = false;
    int retryCount = 0;
    uint32_t delayMs = I2C_RETRY_DELAY_BASE_MS;
    
    while (retryCount < I2C_MAX_RETRIES && !success) {
        unsigned long startTime = micros();
        uint8_t error = 5;
        
        while(micros() - startTime < 500) {
            Wire.beginTransmission(OLED_ADDRESS);
            Wire.write(0x00);
            error = Wire.endTransmission(true);
            
            if(error == 0) break;
            delayMicroseconds(10);
        }
        
        success = (error == 0);
        
        if (!success) {
            retryCount++;
            delay(delayMs);
            delayMs *= 2; // Exponential backoff
        }
    }
    
    #ifdef ESP32
    if(i2cMutex != NULL) xSemaphoreGive(i2cMutex);
    i2cOperationInProgress = false;
    #endif
    
    if(!success) {
        lastI2CFailure = millis();
        i2cFailureCount++;
        
        if(i2cFailureCount >= I2C_RECOVERY_MAX_ATTEMPTS) {
            hardResetI2C();
            i2cFailureCount = 0;
        }
    }
    
    return success;
}

void releaseI2C() {
    #ifdef ESP32
    if(i2cMutex != NULL) xSemaphoreGive(i2cMutex);
    #endif
    delayMicroseconds(50);
}

// =============================================
// HELPER FUNCTIONS
// =============================================
int calculateWidthFont2(const String& text) {
    int width = 0;
    for (unsigned int i = 0; i < text.length(); i++) {
        char c = text[i];
        if (c == '.' || c == ',') width += 6;
        else if (c == '+' || c == '-') width += 8;
        else width += 12;
    }
    return width;
}

String removeTrailingZero(float value, int decimalPlaces) {
    char buffer[20];
    dtostrf(value, 0, decimalPlaces, buffer);
    String result = String(buffer);
    if (result.indexOf('.') != -1) {
        while (result.endsWith("0")) result.remove(result.length() - 1);
        if (result.endsWith(".")) result.remove(result.length() - 1);
    }
    return result;
}

String formatVoltage(float voltage) {
    if (voltage < 0.05f && voltage > -0.05f) return "0";
    if (voltage < 100.0f) return removeTrailingZero(voltage, 1);
    else {
        char buffer[10];
        sprintf(buffer, "%.0f", voltage);
        return String(buffer);
    }
}

String formatCurrent(float current) {
    float absCurrent = fabs(current);
    if (absCurrent < 0.05f) return "0";
    if (absCurrent < 10.0f) return removeTrailingZero(absCurrent, 1);
    else if (absCurrent < 100.0f) return removeTrailingZero(absCurrent, 1);
    else {
        char buffer[10];
        sprintf(buffer, "%.0f", absCurrent);
        return String(buffer);
    }
}

String formatPower(float power) {
    float absPower = fabs(power);
    if (absPower < 0.05f) return "0";
    if (absPower < 10.0f) return removeTrailingZero(absPower, 1);
    else {
        char buffer[10];
        sprintf(buffer, "%.0f", absPower);
        return String(buffer);
    }
}

// =============================================
// ENHANCED ANIMATION FUNCTIONS
// =============================================
void resetAnimation() {
    animatedVoltage = 0.0f;
    animatedCurrent = 0.0f;
    animatedPower = 0.0f;
    targetVoltage = 0.0f;
    targetCurrent = 0.0f;
    targetPower = 0.0f;
    currentVelocity = 0.0f;
    lastAnimationUpdate = 0;
    animationInitialized = false;
}

void updateAnimationTargets() {
    float newVoltage = getRealtimeVoltage();
    float newCurrent = getRealtimeCurrent();
    
    float newPower = newVoltage * newCurrent;
    
    if(newPower > MAX_DISPLAY_POWER) newPower = MAX_DISPLAY_POWER;
    if(newPower < MIN_DISPLAY_POWER) newPower = MIN_DISPLAY_POWER;
    
    if(!animationInitialized) {
        targetVoltage = newVoltage;
        targetCurrent = newCurrent;
        targetPower = newPower;
        animatedVoltage = newVoltage;
        animatedCurrent = newCurrent;
        animatedPower = newPower;
        animationInitialized = true;
        return;
    }
    
    if(fabs(newVoltage - targetVoltage) > VOLTAGE_CHANGE_THRESHOLD) {
        targetVoltage = newVoltage;
    }
    
    if(fabs(newCurrent - targetCurrent) > CURRENT_CHANGE_THRESHOLD) {
        targetCurrent = newCurrent;
    }
    
    if(fabs(newPower - targetPower) > POWER_CHANGE_THRESHOLD) {
        targetPower = newPower;
    }
}

void updateAnimation() {
    unsigned long now = millis();
    if(now - lastAnimationUpdate < ANIMATION_INTERVAL_MS) return;
    
    float deltaTime = (now - lastAnimationUpdate) / 1000.0f;
    lastAnimationUpdate = now;
    
    if(fabs(targetVoltage - animatedVoltage) > 0.01f) {
        animatedVoltage = animatedVoltage + (targetVoltage - animatedVoltage) * ANIMATION_SMOOTHNESS;
        if(fabs(targetVoltage - animatedVoltage) < 0.05f) {
            animatedVoltage = targetVoltage;
        }
    }
    
    float currentDiff = targetCurrent - animatedCurrent;
    
    if(fabs(currentDiff) > CURRENT_CHANGE_THRESHOLD) {
        float acceleration = currentDiff * ACCELERATION;
        currentVelocity = currentVelocity * DAMPING + acceleration * deltaTime;
        
        if(currentVelocity > MAX_VELOCITY) currentVelocity = MAX_VELOCITY;
        if(currentVelocity < -MAX_VELOCITY) currentVelocity = -MAX_VELOCITY;
        
        animatedCurrent += currentVelocity * deltaTime;
        
        if(fabs(currentDiff) < 0.05f) {
            animatedCurrent = targetCurrent;
            currentVelocity = 0.0f;
        }
        
        if((targetCurrent > animatedCurrent && currentDiff < 0) || 
           (targetCurrent < animatedCurrent && currentDiff > 0)) {
            animatedCurrent = targetCurrent;
            currentVelocity = 0.0f;
        }
    } else {
        animatedCurrent = targetCurrent;
        currentVelocity = 0.0f;
    }
    
    if(fabs(targetPower - animatedPower) > 1.0f) {
        animatedPower = animatedPower + (targetPower - animatedPower) * ANIMATION_SMOOTHNESS;
        if(fabs(targetPower - animatedPower) < 5.0f) {
            animatedPower = targetPower;
        }
        if(animatedPower > MAX_DISPLAY_POWER) animatedPower = MAX_DISPLAY_POWER;
        if(animatedPower < MIN_DISPLAY_POWER) animatedPower = MIN_DISPLAY_POWER;
    }
}

// =============================================
// CHECK IF CHARGING PAGE SHOULD BE DISPLAYED
// =============================================
bool isChargingPageDisplayed() {
    #ifdef ESP32
    return (isChargingModeActive() && CHARGING_PAGE_ENABLED);
    #else
    return false;
    #endif
}

// =============================================
// DISPLAY FUNCTIONS
// =============================================
void resetDisplayState() {
    if(!displayInitialized) return;
    display.setFont();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    display.setCursor(0, 0);
}

void showSplashScreen() {
    if(!displayInitialized) return;
    resetDisplayState();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    const GFXfont* splashFont = NULL;
    if(SPLASH_FONT_SIZE == 1) splashFont = &FreeSansBold9pt7b;
    else if(SPLASH_FONT_SIZE == 2) splashFont = &FreeSansBold12pt7b;
    
    if(splashFont != NULL) display.setFont(splashFont);
    
    int textWidth = strlen(SPLASH_TEXT) * (SPLASH_FONT_SIZE == 0 ? 6 : (SPLASH_FONT_SIZE == 1 ? 10 : 15));
    int xPos = SPLASH_POS_X == 0 ? (SCREEN_WIDTH - textWidth) / 2 : SPLASH_POS_X;
    int yPos = SPLASH_POS_Y == 0 ? (SPLASH_FONT_SIZE == 0 ? 16 : (SPLASH_FONT_SIZE == 1 ? 22 : 24)) : SPLASH_POS_Y;
    
    display.setCursor(xPos, yPos);
    display.print(SPLASH_TEXT);
    display.display();
    delay(SPLASH_DURATION_MS);
    resetDisplayState();
}

void initDisplay() {
    serialPrintf("[DISPLAY] Starting initialization...\n");
    
    for(int attempt = 1; attempt <= 5; attempt++) {
        serialPrintf("[DISPLAY] Attempt %d/5\n", attempt);
        
        Wire.end();
        delay(100);
        
        Wire.begin(SDA_PIN, SCL_PIN);
        Wire.setClock(50000);
        #ifdef ESP32
        Wire.setTimeOut(1000);
        #endif
        
        delay(100);
        
        Wire.beginTransmission(OLED_ADDRESS);
        uint8_t error = Wire.endTransmission();
        
        if(error != 0) {
            serialPrintf("[DISPLAY] I2C test error: %d\n", error);
            delay(200);
            continue;
        }
        
        if(display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
            displayInitialized = true;
            displayReady = true;
            resetAnimation();
            
            // Tampilkan splash screen hanya SEKALI
            showSplashScreen();
            
            display.clearDisplay();
            display.display();
            serialPrintf("[DISPLAY] Initialized successfully\n");
            return;
        }
        
        serialPrintf("[DISPLAY] OLED init failed\n");
        delay(200);
    }
    
    displayInitialized = false;
    displayReady = false;
    serialPrintf("[DISPLAY] ERROR: Failed to initialize\n");
}

// =============================================
// FUNGSI RESET FONT DISPLAY
// =============================================
void resetDisplayFont() {
    if (!displayInitialized) return;
    
    if (safeI2COperation(I2C_MUTEX_TIMEOUT_MS)) {
        display.setFont();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setTextWrap(false);
        display.setCursor(0, 0);
        releaseI2C();
    }
}

// =============================================
// UPDATE DISPLAY IN APP MODE - FONT SPLASH SCREEN
// =============================================
void updateAppModeDisplay() {
    if (!displayInitialized || !displayReady) return;
    
    if (safeI2COperationWithBackoff(I2C_MUTEX_TIMEOUT_MS)) {
        // Clear seluruh display
        display.clearDisplay();
        
        // Tampilkan "APP MODE" dengan font splash screen (9pt)
        display.setFont(&FreeSansBold9pt7b);
        display.setTextSize(1);
        
        // Hitung posisi tengah untuk "APP MODE"
        int textWidth = 7 * 10; // "APP MODE" = 7 karakter * 10px (approx)
        int textX = (SCREEN_WIDTH - textWidth) / 2;
        display.setCursor(textX, 20); // Y=20 agar pas di tengah
        display.print("APP MODE");
        
        // Tampilkan status koneksi di bagian bawah
        display.setFont();
        display.setTextSize(1);
        
        // Gunakan deviceConnected dari BLE
        if (deviceConnected) {
            display.setCursor((SCREEN_WIDTH - 50) / 2, 55);
            display.print("connected");
        } else {
            display.setCursor((SCREEN_WIDTH - 60) / 2, 55);
            display.print("waiting...");
        }
        
        display.display();
        releaseI2C();
    }
}

// =============================================
// DISPLAY BLE OFF - DURASI LEBIH LAMA
// =============================================
void showBleOffDisplay() {
    if (!displayReady) return;
    
    if (safeI2COperation(I2C_MUTEX_TIMEOUT_MS)) {
        display.setFont();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setTextWrap(false);
        
        display.clearDisplay();
        
        // Tampilkan "BLE OFF" dengan font splash screen (9pt)
        display.setFont(&FreeSansBold9pt7b);
        display.setTextSize(1);
        
        int textWidth = 6 * 10; // "BLE OFF" = 6 karakter * 10px
        int textX = (SCREEN_WIDTH - textWidth) / 2;
        display.setCursor(textX, 20);
        display.print("BLE OFF");
        
        // Tampilkan teks tambahan
        display.setFont();
        display.setTextSize(1);
        display.setCursor((SCREEN_WIDTH - 70) / 2, 35);
        display.print("Disconnected");
        
        display.display();
        releaseI2C();
        
        serialPrintfln("[DISPLAY] BLE OFF shown");
    }
}

// =============================================
// APP MODE DISPLAY (PUBLIC)
// =============================================
void showAppModeDisplay() {
    updateAppModeDisplay();
}

// =============================================
// TRANSISI HALUS DARI APP MODE KE JAM
// =============================================
void transitionFromAppModeToClock() {
    if (!displayInitialized || !displayReady) return;
    
    serialPrintfln("[DISPLAY] Starting smooth transition");
    
    if (safeI2COperation(I2C_MUTEX_TIMEOUT_MS)) {
        
        // Reset font dulu
        display.setFont();
        display.setTextSize(1);
        
        display.clearDisplay();
        display.display();
        delay(20);
        
        resetDisplayState();
        delay(10);
        
        RTCDateTime dt = getRTC();
        
        display.setFont(&FreeSansBold18pt7b);
        display.setCursor(CLOCK_TIME_POS_X, CLOCK_TIME_POS_Y);
        display.printf("%02d:%02d", dt.hour, dt.minute);
        
        display.setFont();
        display.setTextSize(1);
        
        int hariIndex = dt.dayOfWeek - 1;
        if(hariIndex < 0) hariIndex = 0;
        if(hariIndex > 6) hariIndex = 6;
        
        int bulanIndex = dt.month - 1;
        if(bulanIndex < 0) bulanIndex = 0;
        if(bulanIndex > 11) bulanIndex = 11;
        
        display.setCursor(CLOCK_DAY_POS_X, CLOCK_DAY_POS_Y);
        display.print(DAY_NAMES[hariIndex]);
        
        display.setCursor(CLOCK_DATE_POS_X, CLOCK_DATE_POS_Y);
        display.printf("%d %s", dt.day, MONTH_NAMES[bulanIndex]);
        
        display.setCursor(CLOCK_YEAR_POS_X, CLOCK_YEAR_POS_Y);
        display.printf("%04d", dt.year);
        
        display.display();
        delay(10);
        display.display();
        
        releaseI2C();
        
        serialPrintfln("[DISPLAY] Transition complete");
    } else {
        serialPrintfln("[DISPLAY] ERROR: Cannot get I2C for transition");
    }
}

// =============================================
// safeDisplayUpdate
// =============================================
bool safeDisplayUpdate(int page) {
    #ifdef ESP32
    if (isInAppMode()) {
        return false;
    }
    #endif
    
    if(!displayInitialized || !displayReady) return false;
    
    updateDisplay(page);
    
    return true;
}

void resetDisplay() {
    if(!displayInitialized) return;
    if(safeI2COperation(I2C_MUTEX_TIMEOUT_MS)) {
        resetDisplayState();
        display.clearDisplay();
        display.display();
        releaseI2C();
    }
}

// =============================================
// updateDisplay
// =============================================
void updateDisplay(int page) {
    if(!displayInitialized) return;
    
    #ifdef ESP32
    if (isInAppMode()) {
        return;
    }
    #endif
    
    if (!safeI2COperation(10)) {
        serialPrintfln("[DISPLAY] I2C busy, skipping update");
        return;
    }
    
    resetDisplayState();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    #ifdef ESP32
    if(isChargingModeActive() && CHARGING_PAGE_ENABLED) {
        display.setTextSize(2);
        display.setCursor(0, 0);
        display.print("LOCKED");
        display.display();
        releaseI2C();
        return;
    }
    #endif
    
    if(page == 1) {
        RTCDateTime dt = getRTC();
        
        display.setFont(&FreeSansBold18pt7b);
        display.setCursor(CLOCK_TIME_POS_X, CLOCK_TIME_POS_Y);
        display.printf("%02d:%02d", dt.hour, dt.minute);
        
        display.setFont();
        display.setTextSize(1);
        
        int hariIndex = dt.dayOfWeek - 1;
        if(hariIndex < 0) hariIndex = 0;
        if(hariIndex > 6) hariIndex = 6;
        
        int bulanIndex = dt.month - 1;
        if(bulanIndex < 0) bulanIndex = 0;
        if(bulanIndex > 11) bulanIndex = 11;
        
        display.setCursor(CLOCK_DAY_POS_X, CLOCK_DAY_POS_Y);
        display.print(DAY_NAMES[hariIndex]);
        
        display.setCursor(CLOCK_DATE_POS_X, CLOCK_DATE_POS_Y);
        display.printf("%d %s", dt.day, MONTH_NAMES[bulanIndex]);
        
        display.setCursor(CLOCK_YEAR_POS_X, CLOCK_YEAR_POS_Y);
        display.printf("%04d", dt.year);
        
    } else if(page == 2) {
        display.setTextSize(1);
        
        display.setCursor(TEMP_LABEL_ECU_POS_X, TEMP_LABEL_ECU_POS_Y);
        display.print(TEMP_LABEL_ECU);
        display.setCursor(TEMP_LABEL_MOTOR_POS_X, TEMP_LABEL_MOTOR_POS_Y);
        display.print(TEMP_LABEL_MOTOR);
        display.setCursor(TEMP_LABEL_BATT_POS_X, TEMP_LABEL_BATT_POS_Y);
        display.print(TEMP_LABEL_BATT);
        
        display.setTextSize(2);
        display.setCursor(TEMP_VALUE_ECU_POS_X, TEMP_VALUE_ECU_POS_Y);
        display.print(getTempCtrl());
        display.setCursor(TEMP_VALUE_MOTOR_POS_X, TEMP_VALUE_MOTOR_POS_Y);
        display.print(getTempMotor());
        display.setCursor(TEMP_VALUE_BATT_POS_X, TEMP_VALUE_BATT_POS_Y);
        display.print(getTempBatt());
        
    } else if(page == 3) {
        #ifdef ESP32
        if(!isChargingModeActive()) {
            updateAnimationTargets();
            updateAnimation();
        }
        #endif
        
        float displayVoltage = animatedVoltage;
        float displayCurrent = animatedCurrent;
        
        display.setTextSize(1);
        display.setCursor(BMS_LABEL_VOLTAGE_POS_X, BMS_LABEL_VOLTAGE_POS_Y);
        display.print(BMS_LABEL_VOLTAGE);
        
        display.setTextSize(2);
        String voltageStr = formatVoltage(displayVoltage);
        int voltageWidth = calculateWidthFont2(voltageStr);
        display.setCursor(BMS_VALUE_VOLTAGE_POS_X, BMS_VALUE_VOLTAGE_POS_Y);
        display.print(voltageStr);
        
        display.setTextSize(1);
        display.setCursor(BMS_VALUE_VOLTAGE_POS_X + voltageWidth + 6, BMS_VALUE_VOLTAGE_POS_Y + 6);
        display.print("V");
        
        display.setTextSize(1);
        display.setCursor(BMS_LABEL_CURRENT_POS_X, BMS_LABEL_CURRENT_POS_Y);
        display.print(BMS_LABEL_CURRENT);
        
        bool dataFresh = isDataFresh();
        
        if(!dataFresh && fabs(displayCurrent) < 0.1f) {
            display.setTextSize(2);
            display.setCursor(BMS_VALUE_CURRENT_POS_X + 12, BMS_VALUE_CURRENT_POS_Y);
            display.print("0");
            
            display.setTextSize(1);
            display.setCursor(BMS_VALUE_CURRENT_POS_X + 40, BMS_VALUE_CURRENT_POS_Y + 6);
            display.print("A");
        } else {
            float deadzone = CURRENT_DISPLAY_DEADZONE;
            #ifdef ESP32
            if(isChargingModeActive()) deadzone = CHARGING_CURRENT_DEADZONE;
            #endif
            
            if(fabs(displayCurrent) < deadzone) {
                display.setTextSize(2);
                display.setCursor(BMS_VALUE_CURRENT_POS_X + 12, BMS_VALUE_CURRENT_POS_Y);
                display.print("0");
                
                display.setTextSize(1);
                display.setCursor(118, BMS_VALUE_CURRENT_POS_Y + 6);
                display.print("A");
            } else {
                String currentStr = formatCurrent(displayCurrent);
                bool isNegative = (displayCurrent < -deadzone);
                
                int digitCount = currentStr.length();
                int currentX = BMS_VALUE_CURRENT_POS_X;
                
                if (digitCount == 1) currentX += 12;
                else if (digitCount == 2) currentX += 6;
                else if (digitCount == 3) currentX += 0;
                else currentX -= 6;
                
                if (isNegative) {
                    display.setTextSize(1);
                    display.setCursor(currentX - 8, BMS_VALUE_CURRENT_POS_Y + 6);
                    display.print("-");
                }
                
                display.setTextSize(2);
                display.setCursor(currentX, BMS_VALUE_CURRENT_POS_Y);
                display.print(currentStr);
                
                display.setTextSize(1);
                int unitX;
                
                if (digitCount == 1) unitX = currentX + 12;
                else if (digitCount == 2) unitX = currentX + 24;
                else if (digitCount == 3) unitX = currentX + 36;
                else unitX = currentX + 48;
                
                if (unitX > 118) unitX = 118;
                
                display.setCursor(unitX, BMS_VALUE_CURRENT_POS_Y + 6);
                display.print("A");
            }
        }
        
        display.setCursor(120, 0);
        if(isDataFresh()) 
            display.print("");
        else 
            display.print("x");
        
    } else if(page == 4) {
        #ifdef ESP32
        if(!isChargingModeActive()) {
            updateAnimationTargets();
            updateAnimation();
        }
        #endif
        
        float displayPower = animatedPower;
        
        if(displayPower > MAX_DISPLAY_POWER) displayPower = MAX_DISPLAY_POWER;
        if(displayPower < MIN_DISPLAY_POWER) displayPower = MIN_DISPLAY_POWER;
        
        String powerStr = formatPower(displayPower);
        int digits = powerStr.length();
        bool isThousand = digits >= 4;
        
        if(displayPower > 0.1f) {
            display.setTextSize(2);
            display.setCursor(10, 4);
            display.print("+");
        } else if(displayPower < -0.1f) {
            display.setTextSize(2);
            display.setCursor(10, 4);
            display.print("-");
        }
        
        display.setTextSize(3);
        int numberX;
        if(displayPower > 0.1f || displayPower < -0.1f) {
            numberX = isThousand ? 28 : 36;
        } else {
            numberX = isThousand ? 32 : 40;
        }
        
        display.setCursor(numberX, 2);
        display.print(powerStr);
        
        display.setTextSize(1);
        if(isThousand) {
            int approxNumWidth = digits * 18;
            int wattX = numberX + (approxNumWidth / 2) - 10;
            display.setCursor(wattX, 26);
        } else {
            int numberEndX = numberX + (digits * 18);
            display.setCursor(numberEndX + 4, 14);
        }
        display.print("watt");
        
    } else {
        updateDisplay(1);
        releaseI2C();
        return;
    }
    
    display.display();
    releaseI2C();
}

void showSetupMode(bool blinkState) {
    if(!displayInitialized) return;
    resetDisplayState();
    display.clearDisplay();
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(SSD1306_WHITE);
    
    int xPos = SETUP_MODE_POS_X;
    int yPos = SETUP_MODE_POS_Y;
    
    if(SETUP_MODE_POS_X == 0) {
        int textWidth = strlen(SETUP_TEXT) * 10;
        xPos = (SCREEN_WIDTH - textWidth) / 2;
        yPos = 22;
    }
    
    if(blinkState) {
        display.setCursor(xPos, yPos);
        display.print(SETUP_TEXT);
    }
    
    display.display();
}

bool isDisplayInitialized() {
    return displayInitialized;
}

// =============================================
// INIT DISPLAY TASK
// =============================================
void initDisplayTask() {
    #ifdef ESP32
    if (!DISPLAY_TASK_ENABLED) return;
    
    // Create queue for display commands
    displayQueue = xQueueCreate(DISPLAY_QUEUE_SIZE, sizeof(DisplayCommand));
    
    if (displayQueue == NULL) {
        serialPrintflnAlways("[DISPLAY] ERROR: Failed to create display queue");
        return;
    }
    
    serialPrintflnAlways("[DISPLAY] Display queue created");
    #endif
}

// =============================================
// SEND COMMAND TO DISPLAY TASK
// =============================================
void sendDisplayCommand(DisplayCommandType type, int page, bool blinkState) {
    #ifdef ESP32
    if (displayQueue == NULL) return;
    
    DisplayCommand cmd;
    cmd.type = type;
    cmd.page = page;
    cmd.blinkState = blinkState;
    cmd.timestamp = millis();
    
    // Try to send to queue, don't block if full
    if (xQueueSend(displayQueue, &cmd, 0) != pdTRUE) {
        // Queue full, drop command
        serialPrintfln("[DISPLAY] Queue full, dropping command %d", type);
    }
    #endif
}

// =============================================
// CHECK IF DISPLAY TASK IS BUSY
// =============================================
bool isDisplayTaskBusy() {
    #ifdef ESP32
    if (displayQueue == NULL) return false;
    return (uxQueueMessagesWaiting(displayQueue) > 0);
    #else
    return false;
    #endif
}

// =============================================
// DISPLAY TASK FUNCTION - RUN ON CORE 1
// =============================================
void displayTask(void *pvParameters) {
    #ifdef ESP32
    TickType_t xLastWakeTime = xTaskGetTickCount();
    DisplayCommand cmd;
    unsigned long lastUpdateTime = 0;
    bool inAppMode = false;
    bool showingBleOff = false;
    unsigned long bleOffStartTime = 0;
    
    serialPrintflnAlways("[DISPLAY] Task started on Core %d", xPortGetCoreID());
    
    // Tunggu display siap dari setup
    int waitCount = 0;
    while (!displayReady && waitCount < 50) {  // Tunggu max 2.5 detik
        vTaskDelay(pdMS_TO_TICKS(50));
        waitCount++;
    }
    
    if (!displayReady) {
        serialPrintflnAlways("[DISPLAY] ERROR: Display not ready after waiting");
    }
    
    // Clear display once at start
    if (displayReady) {
        if (safeI2COperationWithBackoff(I2C_MUTEX_TIMEOUT_MS)) {
            display.clearDisplay();
            display.display();
            serialPrintflnAlways("[DISPLAY] Initial clear done");
        }
    }
    
    while (true) {
        // Check for commands in queue (non-blocking)
        if (xQueueReceive(displayQueue, &cmd, 0) == pdTRUE) {
            // Process command
            if (!displayReady) continue;
            
            switch (cmd.type) {
                case DISPLAY_CMD_UPDATE_PAGE:
                    if (!inAppMode && !showingBleOff) {
                        safeI2COperationWithBackoff(I2C_MUTEX_TIMEOUT_MS);
                        updateDisplay(cmd.page);
                        lastUpdateTime = millis();
                    }
                    break;
                    
                case DISPLAY_CMD_UPDATE_CLOCK:
                    if (inAppMode && !showingBleOff) {
                        updateAppModeDisplay();
                        lastUpdateTime = millis();
                    }
                    break;
                    
                case DISPLAY_CMD_TRANSITION_TO_CLOCK:
                    if (!showingBleOff) {
                        inAppMode = false;
                        transitionFromAppModeToClock();
                        lastUpdateTime = millis();
                    }
                    break;
                    
                case DISPLAY_CMD_CLEAR:
                    if (!showingBleOff) {
                        if (safeI2COperationWithBackoff(I2C_MUTEX_TIMEOUT_MS)) {
                            display.clearDisplay();
                            display.display();
                        }
                    }
                    break;
                    
                case DISPLAY_CMD_SHOW_BLE_OFF:
                    // Tampilkan BLE OFF dan mulai timer
                    showBleOffDisplay();
                    showingBleOff = true;
                    bleOffStartTime = millis();
                    break;
                    
                case DISPLAY_CMD_RESET:
                    hardResetI2C();
                    break;
                    
                default:
                    break;
            }
        }
        
        // Auto-update based on mode
        if (displayReady) {
            unsigned long now = millis();
            
            // Cek apakah sedang menampilkan BLE OFF
            if (showingBleOff) {
                if (now - bleOffStartTime >= 3000) {
                    showingBleOff = false;
                    // Kembali ke mode normal
                    inAppMode = isInAppMode();
                    if (!inAppMode) {
                        transitionFromAppModeToClock();
                    }
                }
            } else {
                uint32_t updateInterval = inAppMode ? DISPLAY_APP_MODE_UPDATE_MS : DISPLAY_UPDATE_INTERVAL_MS;
                
                if (now - lastUpdateTime >= updateInterval) {
                    if (inAppMode) {
                        updateAppModeDisplay();
                    } else {
                        safeI2COperationWithBackoff(I2C_MUTEX_TIMEOUT_MS);
                        updateDisplay(currentPage);
                    }
                    lastUpdateTime = now;
                }
            }
        }
        
        // Check for mode change (set by main loop via flag)
        if (!showingBleOff) {
            inAppMode = isInAppMode();
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100)); // 100ms = 10Hz check
    }
    #endif
}
