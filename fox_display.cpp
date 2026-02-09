#include "fox_display.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "fox_config.h"
#include "fox_rtc.h"
#include "fox_canbus.h"
#include "fox_serial.h"
#include "fox_page.h"
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
#endif

// =============================================
// I2C SAFETY VARIABLES
// =============================================
static volatile bool i2cOperationInProgress = false;
static unsigned long lastI2CFailure = 0;
static uint32_t i2cFailureCount = 0;

// =============================================
// ANIMATION VARIABLES - ENHANCED
// =============================================
static float animatedVoltage = 0.0f;
static float animatedCurrent = 0.0f;
static float animatedPower = 0.0f;
static float targetVoltage = 0.0f;
static float targetCurrent = 0.0f;
static float targetPower = 0.0f;
static unsigned long lastAnimationUpdate = 0;
static bool animationInitialized = false;

// Enhanced animation untuk responsif
static float currentVelocity = 0.0f;
static const float MAX_VELOCITY = 5.0f;    // Maximum change per second
static const float ACCELERATION = 10.0f;   // Acceleration rate
static const float DAMPING = 0.9f;         // Damping factor

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
    
    #ifdef ESP32
    if(isChargingModeActive()) {
        newVoltage = getChargingVoltage();
        newCurrent = getChargingCurrent();
    }
    #endif
    
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
    
    // Update targets dengan threshold
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
    
    float deltaTime = (now - lastAnimationUpdate) / 1000.0f; // Convert to seconds
    lastAnimationUpdate = now;
    
    // Smooth voltage animation
    if(fabs(targetVoltage - animatedVoltage) > 0.01f) {
        animatedVoltage = animatedVoltage + (targetVoltage - animatedVoltage) * ANIMATION_SMOOTHNESS;
        if(fabs(targetVoltage - animatedVoltage) < 0.05f) {
            animatedVoltage = targetVoltage;
        }
    }
    
    // ENHANCED CURRENT ANIMATION dengan physics-based
    float currentDiff = targetCurrent - animatedCurrent;
    
    if(fabs(currentDiff) > CURRENT_CHANGE_THRESHOLD) {
        // Calculate acceleration towards target
        float acceleration = currentDiff * ACCELERATION;
        
        // Update velocity dengan acceleration dan damping
        currentVelocity = currentVelocity * DAMPING + acceleration * deltaTime;
        
        // Clamp velocity
        if(currentVelocity > MAX_VELOCITY) currentVelocity = MAX_VELOCITY;
        if(currentVelocity < -MAX_VELOCITY) currentVelocity = -MAX_VELOCITY;
        
        // Update animated current
        animatedCurrent += currentVelocity * deltaTime;
        
        // Snap to target jika sudah dekat
        if(fabs(currentDiff) < 0.05f) {
            animatedCurrent = targetCurrent;
            currentVelocity = 0.0f;
        }
        
        // Prevent overshoot
        if((targetCurrent > animatedCurrent && currentDiff < 0) || 
           (targetCurrent < animatedCurrent && currentDiff > 0)) {
            animatedCurrent = targetCurrent;
            currentVelocity = 0.0f;
        }
    } else {
        // Jika sudah dekat, langsung set ke target
        animatedCurrent = targetCurrent;
        currentVelocity = 0.0f;
    }
    
    // Smooth power animation
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

bool safeDisplayUpdate(int page) {
    if(!displayInitialized || !displayReady) return false;
    
    if(!safeI2COperation(I2C_MUTEX_TIMEOUT_MS)) return false;
    
    bool success = false;
    #ifdef ESP32
    try {
        updateDisplay(page);
        success = true;
    } catch(...) {
        success = false;
    }
    #else
    updateDisplay(page);
    success = true;
    #endif
    
    releaseI2C();
    return success;
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

void updateDisplay(int page) {
    if(!displayInitialized) return;
    
    resetDisplayState();
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    // ============================================
    // CHARGING MODE: SIMPLE DISPLAY
    // ============================================
    #ifdef ESP32
    if(isChargingModeActive()) {
        // SIMPLE CHARGING DISPLAY
        
        float voltage = getChargingVoltage();
        float current = getChargingCurrent();
        
        display.setTextSize(2);
        display.setCursor(10, 10);
        display.printf("%.1fV", voltage);
        
        display.setTextSize(1);
        display.setCursor(15, 28);
        if(current > 0.1f) {
            display.printf("+%.1fA", current);
        } else if(current < -0.1f) {
            display.printf("%.1fA", current);
        } else {
            display.print("0A");
        }
        
        display.setCursor(80, 28);
        display.print("CHARGE");
        
        display.display();
        return;
    }
    #endif
    
    // ============================================
    // NORMAL DISPLAY MODE
    // ============================================
    if(page == 1) {
        // Page 1: Clock
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
        // Page 2: Temperature
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
        // Page 3: BMS Data - DENGAN ANIMASI SMOOTH
        #ifdef ESP32
        if(!isChargingModeActive()) {
            updateAnimationTargets();
            updateAnimation(); // PASTIKAN ANIMASI UPDATE TERPANGGIL
        }
        #endif
        
        // GUNAKAN ANIMATED VALUES, BUKAN REAL-TIME
        float displayVoltage = animatedVoltage;
        float displayCurrent = animatedCurrent;
        
        // ========== VOLTAGE DISPLAY ==========
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
        
        // ========== CURRENT DISPLAY ==========
        display.setTextSize(1);
        display.setCursor(BMS_LABEL_CURRENT_POS_X, BMS_LABEL_CURRENT_POS_Y);
        display.print(BMS_LABEL_CURRENT);
        
        bool dataFresh = isDataFresh();
        
        if(!dataFresh && fabs(displayCurrent) < 0.1f) {
            // Tampilkan "--" jika data tidak fresh
            display.setTextSize(2);
            display.setCursor(BMS_VALUE_CURRENT_POS_X + 12, BMS_VALUE_CURRENT_POS_Y);
            display.print("0");
            
            display.setTextSize(1);
            display.setCursor(BMS_VALUE_CURRENT_POS_X + 40, BMS_VALUE_CURRENT_POS_Y + 6);
            display.print("A");
        } else {
            // Apply deadzone
            float deadzone = CURRENT_DISPLAY_DEADZONE;
            #ifdef ESP32
            if(isChargingModeActive()) deadzone = CHARGING_CURRENT_DEADZONE;
            #endif
            
            if(fabs(displayCurrent) < deadzone) {
                // CURRENT = 0A
                display.setTextSize(2);
                display.setCursor(BMS_VALUE_CURRENT_POS_X + 12, BMS_VALUE_CURRENT_POS_Y);
                display.print("0");
                
                display.setTextSize(1);
                display.setCursor(118, BMS_VALUE_CURRENT_POS_Y + 6);
                display.print("A");
            } else {
                // Format animated current
                String currentStr = formatCurrent(displayCurrent);
                bool isNegative = (displayCurrent < -deadzone);
                
                // Hitung digit count
                int digitCount = currentStr.length();
                
                // Tentukan X position
                int currentX = BMS_VALUE_CURRENT_POS_X;
                
                if (digitCount == 1) currentX += 12;
                else if (digitCount == 2) currentX += 6;
                else if (digitCount == 3) currentX += 0;
                else currentX -= 6;
                
                // Tampilkan tanda -
                if (isNegative) {
                    display.setTextSize(1);
                    display.setCursor(currentX - 8, BMS_VALUE_CURRENT_POS_Y + 6);
                    display.print("-");
                }
                
                // Tampilkan angka ANIMATED
                display.setTextSize(2);
                display.setCursor(currentX, BMS_VALUE_CURRENT_POS_Y);
                display.print(currentStr);
                
                // Tampilkan "A"
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
        
        // ========== FRESHNESS INDICATOR ==========
        display.setCursor(120, 0);
        if(isDataFresh()) 
            display.print("");
        else 
            display.print("x");
        
    } else if(page == 4) {
        // Page 4: Power Display
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
        
        // Tanda +/-
        if(displayPower > 0.1f) {
            display.setTextSize(2);
            display.setCursor(10, 4);
            display.print("+");
        } else if(displayPower < -0.1f) {
            display.setTextSize(2);
            display.setCursor(10, 4);
            display.print("-");
        }
        
        // Angka besar
        display.setTextSize(3);
        int numberX;
        if(displayPower > 0.1f || displayPower < -0.1f) {
            numberX = isThousand ? 28 : 36;
        } else {
            numberX = isThousand ? 32 : 40;
        }
        
        display.setCursor(numberX, 2);
        display.print(powerStr);
        
        // "watt" label
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
        return;
    }
    
    display.display();
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
