#ifndef FOX_DISPLAY_H
#define FOX_DISPLAY_H

#include <Arduino.h>
#include "fox_config.h"  // HARUS INCLUDE untuk PAGE_x_ENABLED

// Forward declaration saja
struct FoxVehicleData;

// Fungsi publik utama
void foxDisplayInit();
void foxDisplayUpdate(int page);
void foxDisplayShowSetupMode(bool blinkState);
void foxDisplayForceSportUpdate();
bool foxDisplayIsInitialized();

// Fungsi internal untuk tiap page
void displayPageClock();
void displayPageTemperature(const FoxVehicleData& vehicleData);
void displayPageElectrical(const FoxVehicleData& vehicleData); // SELALU DEKLARASI
void displayPageSport(const FoxVehicleData& vehicleData);

// Fungsi helper untuk sport page
void updateBlinkState();
void displayCruiseMode();
void displaySportModeLowSpeed();
void displaySportModeHighSpeed(uint8_t speedKmh);

#endif
