#include "fox_page.h"
#include "fox_config.h"
#include "fox_display.h"
#include "fox_canbus.h"
#include "fox_serial.h"

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
// PAGE ORDER FUNCTIONS (NEW)
// =============================================
int getNextPageInOrder(int currentPage) {
    // Cari posisi currentPage dalam PAGE_ORDER
    for (int i = 0; i < PAGE_ORDER_COUNT; i++) {
        if (PAGE_ORDER[i] == currentPage) {
            // Cari page enabled berikutnya (circular navigation)
            for (int j = 1; j <= PAGE_ORDER_COUNT; j++) {
                int nextIndex = (i + j) % PAGE_ORDER_COUNT;
                int nextPage = PAGE_ORDER[nextIndex];
                
                // Check if page is enabled
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
            // Jika tidak ada page enabled lainnya, tetap di page ini
            return currentPage;
        }
    }
    
    // Jika currentPage tidak ada di PAGE_ORDER, cari page enabled pertama
    return getFirstEnabledPage();
}

int getFirstEnabledPage() {
    // Cari page enabled pertama sesuai urutan
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
    
    // Fallback jika tidak ada page yang enabled
    return 1;
}

// =============================================
// MODE DETECTION - DISABLED
// =============================================
void updateModeDetection() {
    currentSpecialMode = 0;
    
    static bool firstLog = true;
    if(firstLog && debugModeEnabled) {
        serialPrintf("[MODE] Detection disabled - Always NORMAL mode\n");
        firstLog = false;
    }
}

// =============================================
// BUTTON FUNCTIONS (UPDATED WITH PAGE ORDER)
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
    if (setupMode) {
        return;
    }
    
    #ifdef ESP32
    // CHARGING MODE: Minimal delay
    if(isChargingModeActive()) {
        if(getChargerMessageAge() < 50) {
            return;
        }
        delay(50);
    }
    #endif
    
    // GUNAKAN SISTEM URUTAN PAGE BARU
    int nextPage = getNextPageInOrder(currentPage);
    
    if (nextPage != currentPage) {
        currentPage = nextPage;
        lastNormalPage = currentPage;
        
        // Fast display update
        safeDisplayUpdate(currentPage);
        
        if (debugModeEnabled) {
            serialPrintf("[PAGE] Changed to page %d\n", currentPage);
        }
    }
}

// =============================================
// PAGE MANAGEMENT (UPDATED)
// =============================================
void switchToPage(int page) {
    if (page < 1 || page > 4) return;
    
    // Validasi page enabled
    bool enabled = false;
    switch (page) {
        case 1: enabled = PAGE_1_ENABLE; break;
        case 2: enabled = PAGE_2_ENABLE; break;
        case 3: enabled = PAGE_3_ENABLE; break;
        case 4: enabled = PAGE_4_ENABLE; break;
    }
    
    if (!enabled) {
        if (debugModeEnabled) {
            serialPrintf("[PAGE] Page %d is disabled\n", page);
        }
        return;
        }
    
    currentPage = page;
    lastNormalPage = currentPage;
    
    safeDisplayUpdate(currentPage);
    
    if (debugModeEnabled) {
        serialPrintf("[PAGE] Manual switch to page %d\n", page);
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
