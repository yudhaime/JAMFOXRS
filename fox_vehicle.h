#ifndef FOX_VEHICLE_H
#define FOX_VEHICLE_H

#include <Arduino.h>
#include "fox_config.h"

// Definisi struct dengan validity flags
struct FoxVehicleData {
    // Mode & Status
    FoxVehicleMode mode;
    bool sportActive;
    
    // Performance
    uint16_t rpm;
    uint16_t speedKmh;
    uint8_t throttlePercent;
    
    // Temperatures
    uint8_t tempController;
    uint8_t tempMotor;
    uint8_t tempBattery;
    
    // Electrical
    float voltage;
    float current;
    uint8_t soc;
    
    // Timestamp
    unsigned long lastUpdate;
    
    // Data validity flags
    bool rpmValid : 1;
    bool speedValid : 1;
    bool tempValid : 1;
    bool voltageValid : 1;
    bool currentValid : 1;
    bool socValid : 1;
    
    // Dirty flags for smart updates
    bool currentDirty : 1;
    bool voltageDirty : 1;
    bool speedDirty : 1;
    bool tempDirty : 1;
    bool socDirty : 1;
    bool modeDirty : 1;
};

// =============================================
// INITIALIZATION & CONTROL
// =============================================

// Basic initialization
void foxVehicleInit();
FoxVehicleData foxVehicleGetData();

// Enhanced initialization
void foxVehicleInitEnhanced();
void foxVehicleUpdateFromCANEnhanced(uint32_t canId, const uint8_t* data, uint8_t len);

// Data freshness
bool foxVehicleDataIsFresh(unsigned long timeoutMs = 1000);
unsigned long foxVehicleGetRecommendedCANInterval();

// Mode detection
bool foxVehicleIsSportMode();
String foxVehicleModeToString(FoxVehicleMode mode);

// =============================================
// CAN PARSING FUNCTIONS
// =============================================

// Main parsing function
void foxVehicleUpdateFromCAN(uint32_t canId, const uint8_t* data, uint8_t len);

// Individual parsers
void parseModeStatus(const uint8_t* data, uint8_t len);
void parseSpeedAndTemp(const uint8_t* data, uint8_t len);
void parseBatteryTemp5S(const uint8_t* data, uint8_t len);
void parseBatteryTempSingle(const uint8_t* data, uint8_t len);
void parseVoltageCurrent(const uint8_t* data, uint8_t len);
void parseSOC(const uint8_t* data, uint8_t len);

// =============================================
// ENHANCED PARSING FUNCTIONS (EVENT-DRIVEN)
// =============================================

void parseVoltageCurrentEnhanced(const uint8_t* data, uint8_t len);
void parseModeStatusEnhanced(const uint8_t* data, uint8_t len);
void parseSpeedAndTempEnhanced(const uint8_t* data, uint8_t len);
void parseBatteryTempEnhanced(uint32_t canId, const uint8_t* data, uint8_t len);
void parseSOCEnhanced(const uint8_t* data, uint8_t len);

// =============================================
// DEBUG & DIAGNOSTICS
// =============================================

// Unknown CAN handling
void foxVehicleEnableUnknownCapture(bool enable);
void foxVehicleClearUnknownList();
void captureUnknownCANData(uint32_t canId, const uint8_t* data, uint8_t len);

// Logging functions
void logUnknownMode(uint8_t modeByte);
void logModeChange(uint8_t modeByte);

// Performance monitoring
unsigned long foxVehicleGetLastUpdateTime();
void foxVehicleResetDirtyFlags();

#endif
