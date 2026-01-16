#ifndef FOX_VEHICLE_H
#define FOX_VEHICLE_H

#include <Arduino.h>
#include "fox_config.h"

// Definisi struct
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
    bool socValid : 1;
};

// Function prototypes
void foxVehicleInit();
void foxVehicleUpdateFromCAN(uint32_t canId, const uint8_t* data, uint8_t len);
FoxVehicleData foxVehicleGetData();
bool foxVehicleIsSportMode();
bool foxVehicleDataIsFresh(unsigned long timeoutMs = 1000);
String foxVehicleModeToString(FoxVehicleMode mode);
void foxVehicleEnableUnknownCapture(bool enable);

// Deklarasi fungsi helper internal
void parseModeStatus(const uint8_t* data, uint8_t len);
void parseSpeedAndTemp(const uint8_t* data, uint8_t len);
void parseBatteryTemp5S(const uint8_t* data, uint8_t len);
void parseBatteryTempSingle(const uint8_t* data, uint8_t len);
void parseVoltageCurrent(const uint8_t* data, uint8_t len);
void parseSOC(const uint8_t* data, uint8_t len);
void captureUnknownCANData(uint32_t canId, const uint8_t* data, uint8_t len);
void logUnknownMode(uint8_t modeByte);
void logModeChange(uint8_t modeByte);

#endif
