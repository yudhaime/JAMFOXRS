#ifndef FOX_PAGE_H
#define FOX_PAGE_H

#include <Arduino.h>

// Mode types - SIMPLIFIED
typedef enum {
    MODE_NORMAL = 0,
} SpecialMode;

// Global variables
extern int currentPage;
extern bool setupMode;
extern uint8_t currentSpecialMode;

// NEW: Page order extern declarations
extern const uint8_t PAGE_ORDER[];
extern const uint8_t PAGE_ORDER_COUNT;

// Function declarations
void initButton();
bool checkButtonPress();
void handleButtonPress();
void updateModeDetection();

// Page management
void switchToPage(int page);

// NEW: Page order functions
int getNextPageInOrder(int currentPage);
int getFirstEnabledPage();

// Getter functions
uint8_t getCurrentSpecialMode();
int getCurrentDisplayPage();

#endif
