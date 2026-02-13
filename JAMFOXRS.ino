#include <Arduino.h>
#include <Wire.h>
#include "fox_config.h"
#include "fox_display.h"
#include "fox_canbus.h"
#include "fox_rtc.h"
#include "fox_page.h"
#include "fox_task.h"
#include "fox_serial.h"

// =============================================
// GLOBAL VARIABLES
// =============================================
bool systemReady = false;
unsigned long systemStartTime = 0;

// Extern variables
extern bool displayInitialized;
extern bool displayReady;
extern int currentPage;
extern uint8_t currentSpecialMode;
extern bool setupMode;

// =============================================
// SETUP FUNCTION
// =============================================
void setup() {
    // Phase 1: Serial
    Serial.begin(115200);
    delay(100);
    
    systemStartTime = millis();
    printSystemStartup();
    
    // Phase 2: I2C
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    #ifdef ESP32
    Wire.setTimeOut(500);
    #endif
    delay(100);
    
    // Phase 3: Display
    initDisplay();
    
    // Phase 4: RTC
    if (initRTC()) {
        // Success
    } else {
        setRTCFromCompileTime();
    }
    
    // Phase 5: FreeRTOS & CAN
    #ifdef ESP32
    initFreeRTOS();
    if (!initCAN()) {
        serialPrintflnAlways("[SYSTEM] WARNING: CAN initialization failed");
    }
    createTasks();
    #endif
    
    // Phase 6: Button
    initButton();
    
    // Phase 7: Set initial page - SIMPLIFIED WITH NEW SYSTEM
    currentPage = getFirstEnabledPage();
    
    // Phase 8: Signal ready
    displayReady = true;
    systemReady = true;
    
    // Phase 9: Initial display
    if(displayReady) {
        safeDisplayUpdate(currentPage);
    }
    
    serialPrintflnAlways("\n[SYSTEM] Setup Complete - FLEXIBLE PAGE ORDER VERSION");
    serialPrintflnAlways("[SYSTEM] Features: Anti-freeze charging, Flexible page order");
    serialPrintflnAlways("[SYSTEM] Initial page: %d", currentPage);
    
    // Tampilkan konfigurasi page
    serialPrintflnAlways("[SYSTEM] Page order: [%d, %d, %d, %d]", 
                         PAGE_ORDER[0], PAGE_ORDER[1], 
                         PAGE_ORDER[2], PAGE_ORDER[3]);
    serialPrintflnAlways("[SYSTEM] Enabled pages: 1=%s, 2=%s, 3=%s, 4=%s",
                         PAGE_1_ENABLE ? "YES" : "NO",
                         PAGE_2_ENABLE ? "YES" : "NO",
                         PAGE_3_ENABLE ? "YES" : "NO",
                         PAGE_4_ENABLE ? "YES" : "NO");
    #ifdef ESP32
    serialPrintflnAlways("[SYSTEM] FreeRTOS Active - Charging mode ready");
    #endif
}

// =============================================
// MAIN LOOP - ANTI-FREEZE VERSION
// =============================================
void loop() {
#ifdef ESP32
    unsigned long now = millis();
    
    // 1. SYSTEM HEALTH MONITOR
    static unsigned long lastHealthUpdate = now;
    static uint32_t localErrorCount = 0;
    
    // Update system health setiap detik
    if(now - lastHealthUpdate > 1000) {
        #ifdef ESP32
        updateSystemHealth();
        
        // Check for system stall
        if(now - lastSuccessfulLoop.load(std::memory_order_acquire) > 5000) {
            localErrorCount++;
            systemErrorCount.fetch_add(1, std::memory_order_relaxed);
            
            // Jika 5 error berturut-turut, restart
            if(localErrorCount >= 5) {
                ESP.restart();
            }
        } else {
            localErrorCount = 0; // Reset jika system sehat
        }
        #endif
        lastHealthUpdate = now;
    }
    
    // 2. SERIAL COMMANDS (minimal processing saat charging)
    if(!isChargingModeActive() || (now % 2000 < 100)) { // Setiap 2 detik saat charging
        processSerialCommands();
    }
    
    // 3. MODE DETECTION
    static unsigned long lastModeCheck = 0;
    if(now - lastModeCheck > 2000) { // 2 detik sekali
        updateModeDetection();
        lastModeCheck = now;
    }
    
    // 4. BUTTON - ALWAYS WORKING, JUST SLOWER DURING CHARGING
    if(checkButtonPress()) {
        #ifdef ESP32
        // Rate limiting saat charging, tapi TIDAK disable
        if(isChargingModeActive()) {
            static unsigned long lastChargingButton = 0;
            if(now - lastChargingButton > 1000) { // Max 1 press per detik
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
    
    // 5. DISPLAY UPDATE - WITH CHARGING PAGE CONDITIONAL
    static unsigned long lastDisplayUpdate = 0;
    static int lastDisplayedPage = -1;
    static uint32_t displayErrorCount = 0;
    
    bool pageChanged = (currentPage != lastDisplayedPage);
    bool forceUpdate = pageChanged;
    
    // Determine update rate
    uint32_t displayUpdateRate = 500; // default
    
    #ifdef ESP32
    if(isChargingModeActive() && CHARGING_PAGE_ENABLED) {
        // CHARGING MODE: Update lambat untuk charging page
        displayUpdateRate = CHARGING_DISPLAY_UPDATE_MS; // 5000ms dari config
    } else {
    #endif
        // NORMAL MODE
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
    
    // Force update jika ada banyak error (visual feedback)
    if(getSystemErrorCount() > 0 && (now % 3000 < 100)) {
        forceUpdate = true;
    }
    
    // Update jika perlu
    if(forceUpdate || (now - lastDisplayUpdate > displayUpdateRate)) {
        bool success = safeDisplayUpdate(currentPage);
        
        if(success) {
            lastDisplayUpdate = now;
            lastDisplayedPage = currentPage;
            displayErrorCount = 0;
        } else {
            displayErrorCount++;
            
            // Jika display gagal 3x berturut-turut, reset I2C
            if(displayErrorCount >= 3) {
                #ifdef ESP32
                recoverI2CBus();
                #endif
                displayErrorCount = 0;
            }
        }
    }
    
    // 6. WATCHDOG - ADAPTIVE TIMEOUT
    static unsigned long lastLoopHeartbeat = now;
    static uint32_t loopCounter = 0;
    
    loopCounter++;
    if(loopCounter % 100 == 0) {
        lastLoopHeartbeat = now;
    }
    
    // Timeout lebih panjang saat charging
    uint32_t watchdogTimeout = isChargingModeActive() ? 60000 : 30000;
    
    if(now - lastLoopHeartbeat > watchdogTimeout) {
        #ifdef ESP32
        ESP.restart();
        #else
        // Non-ESP32 restart
        asm volatile ("jmp 0");
        #endif
    }
    
    // 7. MINIMAL DELAY - PRIORITIZE SYSTEM RESPONSIVENESS
    delay(1); // 1ms delay minimum
    
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
