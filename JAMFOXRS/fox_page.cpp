#include "fox_page.h"
#include "fox_config.h"
#include "fox_display.h"
#include "fox_canbus.h"
#include "fox_serial.h"
#include "fox_ble.h"

// =============================================
// GLOBAL VARIABLES
// =============================================
int currentPage = 1;
int lastNormalPage = 1;
bool setupMode = false;
uint8_t currentSpecialMode = 0;

// Button state
bool lastButton = HIGH;
unsigned long lastButtonPress = 0;

// =============================================
// PAGE ORDER FUNCTIONS
// =============================================
int getNextPageInOrder(int currentPage) {
    for (int i = 0; i < PAGE_ORDER_COUNT; i++) {
        if (PAGE_ORDER[i] == currentPage) {
            for (int j = 1; j <= PAGE_ORDER_COUNT; j++) {
                int nextIndex = (i + j) % PAGE_ORDER_COUNT;
                int nextPage = PAGE_ORDER[nextIndex];
                
                bool enabled = false;
                switch (nextPage) {
                    case 1: enabled = PAGE_1_ENABLE; break;
                    case 2: enabled = PAGE_2_ENABLE; break;
                    case 3: enabled = PAGE_3_ENABLE; break;
                    case 4: enabled = PAGE_4_ENABLE; break;
                }
                
                if (enabled) {
                    return nextPage;
                }
            }
            return currentPage;
        }
    }
    return getFirstEnabledPage();
}

int getFirstEnabledPage() {
    for (int i = 0; i < PAGE_ORDER_COUNT; i++) {
        int page = PAGE_ORDER[i];
        
        bool enabled = false;
        switch (page) {
            case 1: enabled = PAGE_1_ENABLE; break;
            case 2: enabled = PAGE_2_ENABLE; break;
            case 3: enabled = PAGE_3_ENABLE; break;
            case 4: enabled = PAGE_4_ENABLE; break;
        }
        
        if (enabled) {
            return page;
        }
    }
    return 1;
}

// =============================================
// MODE DETECTION
// =============================================
void updateModeDetection() {
    currentSpecialMode = 0;
}

// =============================================
// BUTTON FUNCTIONS - MODIFIED FOR BLE
// =============================================
void initButton() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

bool checkButtonPress() {
    bool btn = digitalRead(BUTTON_PIN);
    
    if (btn == LOW && lastButton == HIGH) {
        delay(DEBOUNCE_DELAY);
        if (digitalRead(BUTTON_PIN) == LOW) {
            unsigned long now = millis();
            if ((now - lastButtonPress) > BUTTON_COOLDOWN_MS) {
                lastButtonPress = now;
                lastButton = btn;
                return true;
            }
        }
    }
    lastButton = btn;
    return false;
}

void handleButtonPress() {
    unsigned long pressTime = millis();
    bool isLongPress = false;
    
    // Tunggu sampai button dilepas atau timeout untuk long press
    while (digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
        if (millis() - pressTime >= BLE_ACTIVATION_HOLD_MS) {
            isLongPress = true;
            break;
        }
    }
    
    // LONG PRESS DETECTED
    if (isLongPress) {
        #ifdef ESP32
        if (isBLEActive()) {
            // Matikan BLE - akan menampilkan BLE OFF
            deactivateBLE();
            serialPrintflnAlways("[BUTTON] BLE deactivated");
        } else {
            // Aktifkan BLE - akan menampilkan APP MODE
            activateBLE();
            serialPrintflnAlways("[BUTTON] BLE activated - APP MODE");
        }
        #endif
        return;
    }
    
    // SHORT PRESS - Normal page switching (hanya jika BLE tidak aktif)
    #ifdef ESP32
    if (isBLEActive()) {
        // Jika BLE aktif, short press tidak melakukan apa-apa
        return;
    }
    #endif
    
    // Normal page switching code
    if (setupMode) {
        return;
    }
    
    #ifdef ESP32
    if(isChargingModeActive()) {
        if(getChargerMessageAge() < 50) {
            return;
        }
        delay(50);
    }
    #endif
    
    int nextPage = getNextPageInOrder(currentPage);
    
    if (nextPage != currentPage) {
        currentPage = nextPage;
        lastNormalPage = currentPage;
        safeDisplayUpdate(currentPage);
        
        if (debugModeEnabled) {
            serialPrintfln("[PAGE] Changed to page %d", currentPage);
        }
    }
}

// =============================================
// PAGE MANAGEMENT
// =============================================
void switchToPage(int page) {
    if (page < 1 || page > 4) return;
    
    bool enabled = false;
    switch (page) {
        case 1: enabled = PAGE_1_ENABLE; break;
        case 2: enabled = PAGE_2_ENABLE; break;
        case 3: enabled = PAGE_3_ENABLE; break;
        case 4: enabled = PAGE_4_ENABLE; break;
    }
    
    if (!enabled) {
        if (debugModeEnabled) {
            serialPrintfln("[PAGE] Page %d is disabled", page);
        }
        return;
    }
    
    currentPage = page;
    lastNormalPage = currentPage;
    
    safeDisplayUpdate(currentPage);
    
    if (debugModeEnabled) {
        serialPrintfln("[PAGE] Manual switch to page %d", page);
    }
}

// =============================================
// GETTER FUNCTIONS
// =============================================
uint8_t getCurrentSpecialMode() { 
    return currentSpecialMode;
}

int getCurrentDisplayPage() { 
    return currentPage; 
}
