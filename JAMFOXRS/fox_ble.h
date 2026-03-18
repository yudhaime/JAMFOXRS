#ifndef FOX_BLE_H
#define FOX_BLE_H

#include <Arduino.h>
#include "fox_config.h"

// BLE Configuration
#define BLE_DEVICE_NAME "Votol_BLE"
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// BLE Timing
#define BLE_FAST_MIN_MS 100
#define BLE_FAST_MAX_MS 500
#define BLE_SLOW_MIN_MS 1000
#define BLE_SLOW_MAX_MS 2000

#define BLE_SAFE_CHUNK 240
#define BLE_PUMP_BUDGET_US 10000
#define BLE_PUMP_MAX_CHUNKS 4

// Vehicle mode for adaptive timing
typedef enum {
    MODE_PARK = 0,
    MODE_STAND,
    MODE_CHARGING,
    MODE_DRIVE,
    MODE_SPORT,
    MODE_REVERSE,
    MODE_BRAKE
} VehicleMode;

// Function declarations
void activateBLE();
void deactivateBLE();
void processBLE();
void updateBLEData();
bool isBLEConnected();
bool isBLEActive();
bool isInAppMode();
bool isBLEActivationPending();
void setBLEActivationPending(bool pending, unsigned long startTime = 0);
void resetBLEActivation();
void printBLEStatus();

// Display functions for BLE
void showAppModeDisplay();
void showBleOffDisplay();

// External variable for display
extern volatile bool deviceConnected;

#endif
