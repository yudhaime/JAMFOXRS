#ifndef FOX_VEHICLE_H
#define FOX_VEHICLE_H

#include <Arduino.h>

// =============================================
// CAN PARSING FUNCTIONS (from fox_canbus.cpp)
// =============================================

// SOC mapping structure
struct SOCPoint {
    uint16_t bmsValue;
    uint8_t socPercent;
};

// SOC mapping table
extern const SOCPoint socKeyPoints[];
extern const uint8_t socKeyPointsCount;

// Parsing functions
float parseVoltageFromCAN(const uint8_t* data);
float parseCurrentFromCAN(const uint8_t* data);
uint8_t bmsToSOC(uint16_t bmsValue);

// Mode detection helper functions
bool isChargingFromModeByte(uint8_t modeByte);
bool isSportFromModeByte(uint8_t modeByte);
bool isCruiseFromModeByte(uint8_t modeByte);
bool isCutoffFromModeByte(uint8_t modeByte);
const char* getModeName(uint8_t modeByte);

// Vehicle data structure
struct VehicleData {
    // Temperatures
    int tempCtrl;
    int tempMotor;
    int tempBatt;
    
    // BMS Data
    float batteryVoltage;
    float batteryCurrent;
    uint16_t bmsSOCRaw;
    uint8_t batterySOC;
    bool chargingCurrent;
    
    // Vehicle state
    bool isCharging;
    uint8_t lastModeByte;
    uint8_t lastLoggedMode;
    
    // Timing
    unsigned long lastMessageTime;
};

// Global vehicle data
extern VehicleData vehicle;

#endif
