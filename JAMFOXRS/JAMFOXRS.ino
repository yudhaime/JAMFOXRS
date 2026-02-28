#include <Arduino.h>
#include <Wire.h>
#include "fox_config.h"
#include "fox_display.h"
#include "fox_canbus.h"
#include "fox_rtc.h"
#include "fox_page.h"
#include "fox_task.h"
#include "fox_serial.h"
#include "fox_ble.h"

// =============================================
// GLOBAL VARIABLES
// =============================================
bool systemReady = false;
unsigned long systemStartTime = 0;

extern bool displayInitialized;
extern bool displayReady;
extern int currentPage;
extern uint8_t currentSpecialMode;
extern bool setupMode;

// =============================================
// SETUP FUNCTION
// =============================================
void setup() {
    Serial.begin(115200);
    delay(100);
    
    systemStartTime = millis();
    printSystemStartup();
    
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    #ifdef ESP32
    Wire.setTimeOut(500);
    #endif
    delay(100);
    
    initDisplay();
    
    if (initRTC()) {
    } else {
        setRTCFromCompileTime();
    }
    
    #ifdef ESP32
    initFreeRTOS();
    if (!initCAN()) {
        serialPrintflnAlways("[SYSTEM] WARNING: CAN initialization failed");
    }
    createTasks();
    #endif
    
    initButton();
    
    currentPage = getFirstEnabledPage();
    
    displayReady = true;
    systemReady = true;
    
    if(displayReady) {
        safeDisplayUpdate(currentPage);
    }
    
    serialPrintflnAlways("\n[SYSTEM] Setup Complete");
    serialPrintflnAlways("[SYSTEM] BLE: Activate with 5-second button hold");
    serialPrintflnAlways("[SYSTEM] Initial page: %d", currentPage);
}

// =============================================
// MAIN LOOP
// =============================================
void loop() {
#ifdef ESP32
    unsigned long now = millis();
    
    // 1. SYSTEM HEALTH MONITOR
    static unsigned long lastHealthUpdate = now;
    static uint32_t localErrorCount = 0;
    
    if(now - lastHealthUpdate > 1000) {
        #ifdef ESP32
        updateSystemHealth();
        
        if(now - lastSuccessfulLoop.load(std::memory_order_acquire) > 5000) {
            localErrorCount++;
            systemErrorCount.fetch_add(1, std::memory_order_relaxed);
            
            if(localErrorCount >= 5) {
                ESP.restart();
            }
        } else {
            localErrorCount = 0;
        }
        #endif
        lastHealthUpdate = now;
    }
    
    // 2. SERIAL COMMANDS
    if(!isChargingModeActive() || (now % 2000 < 100)) {
        processSerialCommands();
    }
    
    // 3. MODE DETECTION
    static unsigned long lastModeCheck = 0;
    if(now - lastModeCheck > 2000) {
        updateModeDetection();
        lastModeCheck = now;
    }
    
    // 4. BUTTON HANDLING
    if(checkButtonPress()) {
        #ifdef ESP32
        if(isChargingModeActive()) {
            static unsigned long lastChargingButton = 0;
            if(now - lastChargingButton > 1000) {
                handleButtonPress();
                lastChargingButton = now;
            }
        } else {
            handleButtonPress();
        }
        #else
        handleButtonPress();
        #endif
    }
    
    // 5. BLE PROCESSING
    #ifdef ESP32
    processBLE();
    #endif
    
    // 6. DISPLAY UPDATE - WITH TRANSITION DETECTION
    static unsigned long lastDisplayUpdate = 0;
    static int lastDisplayedPage = -1;
    static uint32_t displayErrorCount = 0;
    static bool wasInAppMode = false;
    
    #ifdef ESP32
    bool currentlyInAppMode = isInAppMode();
    
    // Deteksi transisi dari APP MODE ke normal
    if (wasInAppMode && !currentlyInAppMode) {
        serialPrintfln("[SYSTEM] Transition from APP MODE detected");
        // Reset timer display untuk menghindari update ganda
        lastDisplayUpdate = now;
    }
    wasInAppMode = currentlyInAppMode;
    
    if (currentlyInAppMode) {
        // Dalam APP MODE, jangan update display
    } else {
    #endif
    
        bool pageChanged = (currentPage != lastDisplayedPage);
        bool forceUpdate = pageChanged;
        
        uint32_t displayUpdateRate = 500;
        
        #ifdef ESP32
        if(isChargingModeActive() && CHARGING_PAGE_ENABLED) {
            displayUpdateRate = CHARGING_DISPLAY_UPDATE_MS;
        } else {
        #endif
            switch(currentPage) {
                case 1: displayUpdateRate = 10000; break;
                case 2: displayUpdateRate = 3000;  break;
                case 3: 
                case 4: displayUpdateRate = 500;   break;
                default: displayUpdateRate = 5000;
            }
        #ifdef ESP32
        }
        #endif
        
        if(getSystemErrorCount() > 0 && (now % 3000 < 100)) {
            forceUpdate = true;
        }
        
        if(forceUpdate || (now - lastDisplayUpdate > displayUpdateRate)) {
            bool success = safeDisplayUpdate(currentPage);
            
            if(success) {
                lastDisplayUpdate = now;
                lastDisplayedPage = currentPage;
                displayErrorCount = 0;
            } else {
                displayErrorCount++;
                
                if(displayErrorCount >= 3) {
                    #ifdef ESP32
                    recoverI2CBus();
                    #endif
                    displayErrorCount = 0;
                }
            }
        }
        
    #ifdef ESP32
    }
    #endif
    
    // 7. WATCHDOG
    static unsigned long lastLoopHeartbeat = now;
    static uint32_t loopCounter = 0;
    
    loopCounter++;
    if(loopCounter % 100 == 0) {
        lastLoopHeartbeat = now;
    }
    
    uint32_t watchdogTimeout = isChargingModeActive() ? 60000 : 30000;
    
    if(now - lastLoopHeartbeat > watchdogTimeout) {
        #ifdef ESP32
        ESP.restart();
        #else
        asm volatile ("jmp 0");
        #endif
    }
    
    delay(1);
    
#else
    // NON-ESP32 VERSION
    static unsigned long lastModeCheck = 0;
    unsigned long now = millis();
    
    processSerialCommands();
    
    if(now - lastModeCheck > 1000) {
        updateModeDetection();
        lastModeCheck = now;
    }
    
    if(checkButtonPress()) {
        handleButtonPress();
    }
    
    static unsigned long lastDisplayUpdate = 0;
    if(now - lastDisplayUpdate > 5000) {
        safeDisplayUpdate(currentPage);
        lastDisplayUpdate = now;
    }
    
    delay(100);
#endif
}
