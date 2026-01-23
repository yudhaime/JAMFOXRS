#include <Arduino.h>
#include <Wire.h>
#include "fox_config.h"
#include "fox_display.h"
#include "fox_canbus.h"
#include "fox_rtc.h"
#include "fox_page.h"
#include "fox_task.h"
#include "fox_serial.h"
#include "fox_vehicle.h"

// =============================================
// SETUP FUNCTION - FINAL VERSION
// =============================================
void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("\n========================================");
    Serial.println("   EV DISPLAY - FINAL VERSION");
    Serial.println("========================================");
    
    // 1. Initialize I2C bus first
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    delay(100);
    
    // 2. Scan I2C devices (minimal)
    byte error, address;
    int nDevices = 0;
    
    for(address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        
        if (error == 0) {
            nDevices++;
        }
    }
    
    // 3. Initialize Display
    initDisplay();
    
    // 4. Initialize RTC
    initRTC();
    
    // 5. Initialize CAN
    initCAN();
    
    // 6. Initialize Button
    initButton();
    
    // Set initial page berdasarkan konfigurasi
    if(!PAGE_1_ENABLE && PAGE_2_ENABLE && PAGE_3_ENABLE) {
        currentPage = 2;
        lastNormalPage = 2;
    }
    else if(PAGE_1_ENABLE && !PAGE_2_ENABLE && PAGE_3_ENABLE) {
        currentPage = 1;
        lastNormalPage = 1;
    }
    else if(PAGE_1_ENABLE && PAGE_2_ENABLE && !PAGE_3_ENABLE) {
        currentPage = 1;
        lastNormalPage = 1;
    }
    else if(!PAGE_1_ENABLE && !PAGE_2_ENABLE && PAGE_3_ENABLE) {
        currentPage = 3;
        lastNormalPage = 3;
    }
    else if(PAGE_1_ENABLE && !PAGE_2_ENABLE && !PAGE_3_ENABLE) {
        currentPage = 1;
        lastNormalPage = 1;
    }
    else if(!PAGE_1_ENABLE && PAGE_2_ENABLE && !PAGE_3_ENABLE) {
        currentPage = 2;
        lastNormalPage = 2;
    }
    else {
        currentPage = 1;
        lastNormalPage = 1;
    }
    
    // 8. Initialize FreeRTOS (ESP32 only)
    #ifdef ESP32
    initFreeRTOS();
    createTasks();
    #endif
    
    // 9. Signal display ready
    displayReady = true;
    
    Serial.println("\nSystem Ready. Type HELP for commands.");
    
    // Initial display
    if(displayReady) {
        safeDisplayUpdate(currentPage);
    }
}

// =============================================
// MAIN LOOP - FINAL VERSION
// =============================================
void loop() {
    #ifdef ESP32
    // ESP32: serial commands
    processSerialCommands();
    
    // Mode detection dengan rate limiting
    static unsigned long lastModeCheck = 0;
    unsigned long now = millis();
    
    if(now - lastModeCheck > 500) {
        updateModeDetection();
        lastModeCheck = now;
    }
    
    // Button handling
    if(checkButtonPress()) {
        handleButtonPress();
    }
    
    // Simple display updates untuk normal mode
    static unsigned long lastDisplayUpdate = 0;
    static int lastDisplayedPage = -1;
    
    if(currentSpecialMode == MODE_NORMAL && !setupMode) {
        if(currentPage != lastDisplayedPage || (now - lastDisplayUpdate > 5000)) {
            safeDisplayUpdate(currentPage);
            lastDisplayedPage = currentPage;
            lastDisplayUpdate = now;
        }
    }
    
    delay(100);
    
    #else
    // Non-ESP32
    static unsigned long lastModeCheck = 0;
    unsigned long now = millis();
    
    // Serial commands
    processSerialCommands();
    
    // Update CAN
    updateCAN();
    
    // Mode detection setiap 500ms
    if(now - lastModeCheck > 500) {
        updateModeDetection();
        lastModeCheck = now;
    }
    
    // Button
    if(checkButtonPress()) {
        handleButtonPress();
    }
    
    delay(100);
    #endif
}
