#ifndef VEHICLE_H
#define VEHICLE_H

#include <Arduino.h>

struct VehicleData {
    // Voltage & current (ESSENTIAL)
    float batteryVoltage;
    float batteryCurrent;
    
    // Temperature (ESSENTIAL)
    int tempCtrl;
    int tempMotor;
    int tempBatt;
    
    // Mode data
    uint8_t lastModeByte;
    bool chargingCurrent;
    
    // SOC data
    uint8_t batterySOC;
    uint16_t bmsSOCRaw;
    
    // Charger data
    bool chargerConnected;
    bool oriChargerDetected;
    unsigned long lastChargerMessage;
    
    // Timing
    unsigned long lastMessageTime;
    
    // Charging flag
    bool isCharging;
};

// Global instance
extern VehicleData vehicle;

// Initialization
void initVehicleData();

// Temperature functions
bool isTemperatureValid(int temp);

// Voltage/Current validation
bool isVoltageValid(float voltage);
bool isCurrentValid(float current);

// SOC validation
bool isSOCValid(uint8_t soc);

// Power calculations
float calculatePower(float voltage, float current);

// State checks
bool isVehicleMoving();
bool isVehicleCharging();

// Data reset
void resetTemperatureData();
void resetBMSData();
void resetChargerData();
void resetAllVehicleData();

// Debug functions
void printVehicleData();

// Data persistence
void saveSOCToStorage(uint8_t soc);
uint8_t loadSOCFromStorage();

// SOC correction
void correctSOCBasedOnVoltage();

#endif
