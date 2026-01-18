#ifndef FOX_DISPLAY_H
#define FOX_DISPLAY_H

#include <Arduino.h>
#include "fox_config.h" 

// Forward declaration
struct FoxVehicleData;

// =============================================
// DISPLAY INITIALIZATION & CONTROL
// =============================================

// Basic initialization
void foxDisplayInit();
bool foxDisplayIsInitialized();

// Enhanced initialization with watchdog
void foxDisplayInitEnhanced();
void foxDisplayWatchdogCheck();

// Smart update system
void foxDisplayUpdateSmart(int page, bool forceFullUpdate);
void foxDisplayForceUpdate();
void foxDisplayForceSportUpdate();
bool foxDisplayCheckUpdateNeeded(const FoxVehicleData& vehicleData);

// Special modes
void foxDisplayShowSetupMode(bool blinkState);
void foxDisplayUpdateChargingMode();
void foxDisplayUpdateCruiseMode(bool blinkState);  // TAMBAH INI

// Performance monitoring
int foxDisplayGetSmartUpdateCount();
int foxDisplayGetFallbackUpdateCount();
int foxDisplayGetSmartUpdateSuccessRate();

// I2C error handling
void recoverI2C();
int getI2CErrorCount();
unsigned long getLastI2CErrorTime();

// =============================================
// INTERNAL FUNCTIONS (untuk fox_display.cpp)
// =============================================

// Page display functions
void displayPageClock(bool forceFull);
void displayPageTemperature(const FoxVehicleData& vehicleData, bool forceFull);
void displayPageElectrical(const FoxVehicleData& vehicleData, bool forceFull);
void displayPageSport(const FoxVehicleData& vehicleData, bool forceFull);

// Partial update functions
void updateClockPartial();
void updateCurrentPartial(float current);
void updateVoltagePartial(float voltage);
void updateSpeedPartial(uint16_t speed);
void updateTemperaturePartial(uint8_t tempCtrl, uint8_t tempMotor, uint8_t tempBatt);

// Sport page helper functions
void updateBlinkState();
void displayCruiseMode();
void displaySportModeLowSpeed();
void displaySportModeHighSpeed(uint16_t speedKmh);

// Zone management
void markZoneDirty(DisplayZone zone);
void clearAllZonesDirty();
bool isAnyZoneDirty();
void updateDirtyZones(const FoxVehicleData& vehicleData);

#endif
