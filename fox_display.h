#ifndef FOX_DISPLAY_H
#define FOX_DISPLAY_H

#include <Arduino.h>
#include "fox_config.h" 

// Forward declaration
struct FoxVehicleData;

// Display initialization and control
void foxDisplayInit();
void foxDisplayUpdate(int page);
void foxDisplayShowSetupMode(bool blinkState);
void foxDisplayForceSportUpdate();
bool foxDisplayIsInitialized();

// I2C error handling
void recoverI2C();
int getI2CErrorCount();
unsigned long getLastI2CErrorTime();

// =============================================
// INTERNAL FUNCTIONS (untuk fox_display.cpp)
// =============================================

// Page display functions
void displayPageClock();
void displayPageTemperature(const FoxVehicleData& vehicleData);
void displayPageElectrical(const FoxVehicleData& vehicleData);
void displayPageSport(const FoxVehicleData& vehicleData);

// Sport page helper functions
void updateBlinkState();
void displayCruiseMode();
void displaySportModeLowSpeed();
void displaySportModeHighSpeed(uint8_t speedKmh);

#endif
