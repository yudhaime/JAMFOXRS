#ifndef FOX_PAGE_H
#define FOX_PAGE_H

#include <Arduino.h>

// =============================================
// PAGE & MODE MANAGEMENT (TANPA CHARGING)
// =============================================

// Mode types - HANYA SPORT & CRUISE
typedef enum {
    MODE_NORMAL = 0,
    MODE_SPORT = 1,
    MODE_CRUISE = 2,
    // MODE_CHARGING = 3  // DIHAPUS
} SpecialMode;

// Global variables
extern int currentPage;
extern bool setupMode;
extern bool pageLocked;
extern bool wasCharging; // Masih ada untuk kompatibilitas
extern uint8_t currentSpecialMode;
extern int lastNormalPage;

// Button handling
extern bool lastButton;
extern unsigned long setupModeStart;
extern unsigned long lastButtonPress;

// Function declarations
void initButton();
bool checkButtonPress();
void handleButtonPress();
void updateModeDetection();
void handleSpecialModeDisplay();
void returnToNormalMode();

// Page management
void switchToPage(int page);
void switchToSpecialMode(SpecialMode mode);
void forceNormalMode();

// Getter functions
uint8_t getCurrentSpecialMode();
bool isInSpecialMode();
int getCurrentDisplayPage();
int getLastNormalDisplayPage();

// NEW FUNCTIONS
void enterSpecialMode(uint8_t mode);
void handleCruiseBlink();

#endif
