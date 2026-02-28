#ifndef VEHICLE_H
#define VEHICLE_H

#include <Arduino.h>

#define MAX_CELLS 23
#define MAX_TEMP_SENSORS 5

struct VehicleData {
    // ========== BASIC DATA ==========
    float batteryVoltage;
    float batteryCurrent;
    int tempCtrl;
    int tempMotor;
    int tempBatt;
    uint8_t lastModeByte;
    bool chargingCurrent;
    unsigned long lastMessageTime;
    bool isCharging;
    
    // ========== RPM & SPEED ==========
    int rpm;                    // Motor RPM
    int speed;                  // Speed in km/h
    
    // ========== BATTERY HEALTH & SOC ==========
    int batterySOC;              // State of Charge 0-100%
    int batterySOH;              // State of Health 0-100%
    uint16_t batteryCycleCount;  // Cycle count
    float remainingCapacity;     // Remaining capacity in Ah
    float fullCapacity;          // Full capacity in Ah
    
    // ========== CELL VOLTAGES ==========
    uint16_t cellVoltages[MAX_CELLS];        // 23 cell voltages in mV
    uint16_t cellHighestVolt;                 // Highest cell voltage
    uint8_t cellHighestNum;                    // Cell number with highest voltage
    uint16_t cellLowestVolt;                  // Lowest cell voltage
    uint8_t cellLowestNum;                     // Cell number with lowest voltage
    uint16_t cellAvgVolt;                      // Average cell voltage
    uint16_t cellDelta;                        // Difference between highest and lowest
    
    // ========== BMS TEMPERATURES ==========
    uint8_t cellTemps[MAX_TEMP_SENSORS];      // 5 temperature sensors (direct °C)
    uint8_t tempMax;                            // Maximum temperature
    uint8_t tempMaxCell;                         // Cell with max temperature
    uint8_t tempMin;                            // Minimum temperature
    uint8_t tempMinCell;                         // Cell with min temperature
    
    // ========== BALANCE STATUS ==========
    uint8_t balanceMode;          // Balance mode
    uint8_t balanceStatus;        // Balance status
    uint8_t balanceBits[4];       // 32 bits for 23 cells (4 bytes)
    
    // ========== CHARGER DATA ==========
    bool chargerConnected;
    bool oriChargerDetected;
    unsigned long lastChargerMessage;
    float chargerVoltage;
    float chargerCurrent;
    uint8_t chargerStatus;
    
    // ========== ODOMETER ==========
    uint32_t odometer;            // Odometer in meters
    
    // ========== RAW DATA FOR DEBUG ==========
    uint16_t rawCurrentHex;
    uint16_t rawVoltageHex;
    uint16_t rawSOCHex;
    String rawBalanceHex;
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

// Cell voltage functions
uint16_t getMinCellVoltage();
uint16_t getMaxCellVoltage();
uint16_t getCellDelta();
void updateCellStatistics();

// SOC lookup
float getSoCFromLookup(uint16_t raw);

// Debug functions
void printVehicleData();

// Data persistence
void saveSOCToStorage(uint8_t soc);
uint8_t loadSOCFromStorage();

// SOC correction
void correctSOCBasedOnVoltage();

#endif
