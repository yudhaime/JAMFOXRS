#include "fox_page.h"
#include "fox_config.h"
#include "fox_display.h"
#include "fox_canbus.h"

// =============================================
// GLOBAL VARIABLES
// =============================================
int currentPage = 1;
bool setupMode = false;
bool pageLocked = false;
bool wasCharging = false; // Tetap ada untuk kompatibilitas, tapi tidak digunakan
uint8_t currentSpecialMode = 0;
int lastNormalPage = 1;

// Button
bool lastButton = HIGH;
unsigned long setupModeStart = 0;
unsigned long lastButtonPress = 0;

// Cruise blink
unsigned long lastCruiseBlinkTime = 0;
bool cruiseBlinkState = true;

// Anti-bounce untuk mode detection
static unsigned long lastModeChangeTime = 0;
static const unsigned long MODE_CHANGE_DEBOUNCE = 500;

// =============================================
// ENTER SPECIAL MODE - HANYA UNTUK SPORT & CRUISE
// =============================================
void enterSpecialMode(uint8_t mode) {
    if(mode == MODE_NORMAL) return; // Hanya cek MODE_NORMAL saja
    
    // Simpan page normal
    if(currentSpecialMode == MODE_NORMAL) {
        lastNormalPage = currentPage;
    }
    
    // Update state
    currentSpecialMode = mode;
    
    // Tampilkan segera
    if(mode == MODE_CRUISE) {
        cruiseBlinkState = true;
        lastCruiseBlinkTime = millis();
        safeShowSpecialMode(MODE_CRUISE, true);
    } else if(mode == MODE_SPORT) {
        safeShowSpecialMode(MODE_SPORT, true);
    }
    // Tidak ada untuk mode lain (termasuk charging yang sudah dihapus)
}

// =============================================
// CRUISE BLINK HANDLER
// =============================================
void handleCruiseBlink() {
    unsigned long now = millis();
    
    if(now - lastCruiseBlinkTime > BLINK_INTERVAL_MS) {
        cruiseBlinkState = !cruiseBlinkState;
        safeShowSpecialMode(MODE_CRUISE, cruiseBlinkState);
        lastCruiseBlinkTime = now;
    }
}

// =============================================
// MODE DETECTION - TANPA CHARGING
// =============================================
void updateModeDetection() {
    unsigned long now = millis();
    
    // Debounce: minimal 500ms antara perubahan mode
    if(now - lastModeChangeTime < MODE_CHANGE_DEBOUNCE) {
        return;
    }
    
    // Dapatkan state SAAT INI (HANYA SPORT & CRUISE)
    bool sportNow = isSportMode();
    bool cruiseNow = isCruiseMode();
    
    // Tentukan mode target (HANYA SPORT, CRUISE, atau NORMAL)
    uint8_t targetMode = MODE_NORMAL;
    
    if(cruiseNow) {
        targetMode = MODE_CRUISE;
    }
    else if(sportNow) {
        targetMode = MODE_SPORT;
    }
    // Tidak ada kondisi untuk charging
    
    // Jika mode berubah
    if(targetMode != currentSpecialMode) {
        lastModeChangeTime = now;
        
        if(targetMode == MODE_NORMAL) {
            // Kembali ke normal
            returnToNormalMode();
        } else {
            // Masuk special mode (hanya sport/cruise)
            enterSpecialMode(targetMode);
        }
    }
    
    // Handle cruise blink
    if(currentSpecialMode == MODE_CRUISE) {
        handleCruiseBlink();
    }
}

// =============================================
// BUTTON FUNCTIONS (TETAP SAMA)
// =============================================
void initButton() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

bool checkButtonPress() {
    bool btn = digitalRead(BUTTON_PIN);
    
    if(btn == LOW && lastButton == HIGH) {
        delay(DEBOUNCE_DELAY);
        if(digitalRead(BUTTON_PIN) == LOW) {
            unsigned long now = millis();
            if((now - lastButtonPress) > BUTTON_COOLDOWN_MS) {
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
    if(currentSpecialMode != MODE_NORMAL || setupMode) {
        return;
    }
    
    int nextPage = currentPage;
    for(int i = 0; i < 3; i++) {
        nextPage++;
        if(nextPage > 3) nextPage = 1;
        
        bool enabled = false;
        if(nextPage == 1 && PAGE_1_ENABLE) enabled = true;
        else if(nextPage == 2 && PAGE_2_ENABLE) enabled = true;
        else if(nextPage == 3 && PAGE_3_ENABLE) enabled = true;
        
        if(enabled) {
            currentPage = nextPage;
            lastNormalPage = currentPage;
            safeDisplayUpdate(currentPage);
            return;
        }
    }
}

// =============================================
// MODE MANAGEMENT (legacy functions)
// =============================================
void switchToPage(int page) {
    if(page < 1 || page > 3) return;
    
    // Validasi page enable
    if((page == 1 && !PAGE_1_ENABLE) || 
       (page == 2 && !PAGE_2_ENABLE) ||
       (page == 3 && !PAGE_3_ENABLE)) {
        return;
    }
    
    // Hanya jika tidak dalam mode khusus atau setup
    if(currentSpecialMode != MODE_NORMAL) {
        return;
    }
    
    if(setupMode) {
        return;
    }
    
    currentPage = page;
    lastNormalPage = page;
    
    safeDisplayUpdate(currentPage);
}

void switchToSpecialMode(SpecialMode mode) {
    // Jangan izinkan switching ke mode yang sudah dihapus
    if(mode == MODE_NORMAL || currentSpecialMode == mode) return;
    
    // Hanya izinkan sport dan cruise
    if(mode != MODE_SPORT && mode != MODE_CRUISE) return;
    
    enterSpecialMode(mode);
}

void returnToNormalMode() {
    if(currentSpecialMode == MODE_NORMAL) return;
    
    currentSpecialMode = MODE_NORMAL;
    currentPage = lastNormalPage;
    
    safeDisplayUpdate(currentPage);
}

void forceNormalMode() {
    currentSpecialMode = MODE_NORMAL;
    wasCharging = false;
    currentPage = lastNormalPage;
    
    safeDisplayUpdate(currentPage);
}

// =============================================
// SPECIAL MODE DISPLAY HANDLER (for compatibility)
// =============================================
void handleSpecialModeDisplay() {
    if(currentSpecialMode == MODE_CRUISE) {
        handleCruiseBlink();
    }
}

// =============================================
// GETTER FUNCTIONS
// =============================================
uint8_t getCurrentSpecialMode() { 
    return currentSpecialMode; 
}

bool isInSpecialMode() { 
    return (currentSpecialMode != MODE_NORMAL); 
}

int getCurrentDisplayPage() { 
    return currentPage; 
}

int getLastNormalDisplayPage() { 
    return lastNormalPage; 
}
