#ifndef CANBUS_H
#define CANBUS_H

#include <Arduino.h>

// Inisialisasi dan update
bool initCAN();
void updateCAN();
void resetCANData();

// Getter functions
bool getChargingStatus();
uint8_t getCurrentModeByte();
bool isCurrentlyCharging();

// Temperature getters
int getTempCtrl();
int getTempMotor();
int getTempBatt();

// Mode detection
uint8_t getCurrentVehicleMode();
bool isSportMode();
bool isCruiseMode();
bool isCutoffMode();

// BMS DATA GETTERS
float getBatteryVoltage();
float getBatteryCurrent();
uint8_t getBatterySOC();
bool isChargingCurrent();

// Data access untuk display
void getBMSDataForDisplay(float &voltage, float &current, uint8_t &soc, bool &isCharging);

// Debug
void printModeDebugInfo();

#endif
