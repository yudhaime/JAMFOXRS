#include "fox_vehicle.h"
#include "fox_config.h"
#include <Arduino.h>

// =============================================
// GLOBAL VEHICLE DATA INSTANCE
// =============================================
VehicleData vehicle;

// =============================================
// VEHICLE DATA INITIALIZATION
// =============================================
void initVehicleData() {
    Serial.println("Initializing vehicle data...");
    
    // Mode data
    vehicle.lastModeByte = MODE_BYTE_PARK;
    vehicle.isCharging = false;
    vehicle.chargingCurrent = false;
    
    // Temperature data
    vehicle.tempCtrl = DEFAULT_TEMP;
    vehicle.tempMotor = DEFAULT_TEMP;
    vehicle.tempBatt = DEFAULT_TEMP;
    
    // BMS data
    vehicle.batteryVoltage = 0.0f;
    vehicle.batteryCurrent = 0.0f;
    vehicle.batterySOC = 0;
    vehicle.bmsSOCRaw = 0;
    
    // Charger data
    vehicle.chargerConnected = false;
    vehicle.oriChargerDetected = false;
    vehicle.lastChargerMessage = 0;
    
    // Timing
    vehicle.lastMessageTime = 0;
    
    Serial.println("Vehicle data initialized.");
}

// =============================================
// DATA VALIDATION FUNCTIONS
// =============================================
bool isTemperatureValid(int temp) {
    return (temp >= -40 && temp <= 150);
}

bool isVoltageValid(float voltage) {
    return (voltage >= 0.0f && voltage <= 100.0f);
}

bool isCurrentValid(float current) {
    return (current >= -300.0f && current <= 300.0f);
}

bool isSOCValid(uint8_t soc) {
    return (soc <= 100);
}

// =============================================
// POWER CALCULATION
// =============================================
float calculatePower(float voltage, float current) {
    return voltage * current;
}

// =============================================
// STATE CHECK FUNCTIONS
// =============================================
bool isVehicleMoving() {
    if(vehicle.batteryCurrent < -1.0f) {
        return true;
    }
    return false;
}

bool isVehicleCharging() {
    if(vehicle.batteryCurrent > 1.0f) {
        return true;
    }
    return false;
}

// =============================================
// DATA RESET FUNCTIONS
// =============================================
void resetTemperatureData() {
    vehicle.tempCtrl = DEFAULT_TEMP;
    vehicle.tempMotor = DEFAULT_TEMP;
    vehicle.tempBatt = DEFAULT_TEMP;
}

void resetBMSData() {
    vehicle.batteryVoltage = 0.0f;
    vehicle.batteryCurrent = 0.0f;
    vehicle.batterySOC = 0;
    vehicle.bmsSOCRaw = 0;
    vehicle.chargingCurrent = false;
}

void resetChargerData() {
    vehicle.chargerConnected = false;
    vehicle.oriChargerDetected = false;
    vehicle.lastChargerMessage = 0;
}

void resetAllVehicleData() {
    vehicle.lastModeByte = MODE_BYTE_PARK;
    vehicle.isCharging = false;
    vehicle.chargingCurrent = false;
    
    resetTemperatureData();
    resetBMSData();
    resetChargerData();
    
    vehicle.lastMessageTime = 0;
}

// =============================================
// DEBUG FUNCTIONS
// =============================================
void printVehicleData() {
    Serial.println("\n=== VEHICLE DATA ===");
    Serial.printf("Voltage: %.1fV, Current: %.1fA, SOC: %d%%\n",
                  vehicle.batteryVoltage, vehicle.batteryCurrent, vehicle.batterySOC);
    Serial.printf("Temperatures: ECU=%d°C, Motor=%d°C, Batt=%d°C\n", 
                  vehicle.tempCtrl, vehicle.tempMotor, vehicle.tempBatt);
    Serial.printf("Charger: Connected=%d, ORI=%d\n",
                  vehicle.chargerConnected, vehicle.oriChargerDetected);
    Serial.printf("Timing: LastMsg=%lums ago\n", 
                  millis() - vehicle.lastMessageTime);
    Serial.println("====================\n");
}

// =============================================
// DATA PERSISTENCE FUNCTIONS
// =============================================
void saveSOCToStorage(uint8_t soc) {
    // Placeholder for SOC storage
    Serial.printf("Saving SOC to storage: %d%%\n", soc);
}

uint8_t loadSOCFromStorage() {
    // Placeholder for SOC loading
    Serial.println("Loading SOC from storage");
    return 50;  // Default 50%
}

// =============================================
// AUTOMATIC SOC CORRECTION
// =============================================
void correctSOCBasedOnVoltage() {
    if(vehicle.batteryVoltage > 54.0f && vehicle.batterySOC < 95) {
        vehicle.batterySOC = 95;
        Serial.println("SOC corrected to 95% based on high voltage");
    } else if(vehicle.batteryVoltage < 42.0f && vehicle.batterySOC > 20) {
        vehicle.batterySOC = 20;
        Serial.println("SOC corrected to 20% based on low voltage");
    }
}
