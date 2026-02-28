#include "fox_vehicle.h"
#include "fox_config.h"
#include <Arduino.h>

// =============================================
// GLOBAL VEHICLE DATA INSTANCE
// =============================================
VehicleData vehicle;

// =============================================
// SOC LOOKUP TABLE
// =============================================
const uint16_t socToBms[101] = {
  0, 60,70,80,90,95,105,115,125,135,140,150,160,170,180,185,195,205,215,225,
  230,240,250,260,270,275,285,295,305,315,320,330,340,350,360,365,375,385,395,405,
  410,420,430,440,450,455,465,475,485,495,500,510,520,530,540,550,555,565,575,585,
  590,600,610,620,630,635,645,655,665,675,680,690,700,710,720,725,735,745,755,765,
  770,780,790,800,810,815,825,835,845,855,860,870,880,890,900,905,915,925,935,945,950
};

// =============================================
// VEHICLE DATA INITIALIZATION
// =============================================
void initVehicleData() {
    Serial.println("Initializing vehicle data...");
    
    // Mode data
    vehicle.lastModeByte = MODE_BYTE_PARK;
    vehicle.isCharging = false;
    vehicle.chargingCurrent = false;
    
    // RPM & Speed
    vehicle.rpm = 0;
    vehicle.speed = 0;
    
    // Temperature data
    vehicle.tempCtrl = DEFAULT_TEMP;
    vehicle.tempMotor = DEFAULT_TEMP;
    vehicle.tempBatt = DEFAULT_TEMP;
    
    // BMS data - basic
    vehicle.batteryVoltage = 0.0f;
    vehicle.batteryCurrent = 0.0f;
    
    // BMS data - health
    vehicle.batterySOC = 0;
    vehicle.batterySOH = 100;
    vehicle.batteryCycleCount = 0;
    vehicle.remainingCapacity = 0.0f;
    vehicle.fullCapacity = 0.0f;
    
    // Cell voltages
    for(int i = 0; i < MAX_CELLS; i++) {
        vehicle.cellVoltages[i] = 0;
    }
    vehicle.cellHighestVolt = 0;
    vehicle.cellHighestNum = 0;
    vehicle.cellLowestVolt = 0;
    vehicle.cellLowestNum = 0;
    vehicle.cellAvgVolt = 0;
    vehicle.cellDelta = 0;
    
    // BMS temperatures
    for(int i = 0; i < MAX_TEMP_SENSORS; i++) {
        vehicle.cellTemps[i] = DEFAULT_TEMP;
    }
    vehicle.tempMax = DEFAULT_TEMP;
    vehicle.tempMaxCell = 0;
    vehicle.tempMin = DEFAULT_TEMP;
    vehicle.tempMinCell = 0;
    
    // Balance status
    vehicle.balanceMode = 0;
    vehicle.balanceStatus = 0;
    for(int i = 0; i < 4; i++) {
        vehicle.balanceBits[i] = 0;
    }
    
    // Charger data
    vehicle.chargerConnected = false;
    vehicle.oriChargerDetected = false;
    vehicle.lastChargerMessage = 0;
    vehicle.chargerVoltage = 0.0f;
    vehicle.chargerCurrent = 0.0f;
    vehicle.chargerStatus = 0;
    
    // Odometer
    vehicle.odometer = 0;
    
    // Raw data
    vehicle.rawCurrentHex = 0;
    vehicle.rawVoltageHex = 0;
    vehicle.rawSOCHex = 0;
    vehicle.rawBalanceHex = "00 00 00 00 00 00";
    
    // Timing
    vehicle.lastMessageTime = 0;
    
    Serial.println("Vehicle data initialized.");
}

// =============================================
// SOC LOOKUP FUNCTION
// =============================================
float getSoCFromLookup(uint16_t raw) {
    if (raw >= socToBms[100]) return 100.0f;
    if (raw <= socToBms[0]) return 0.0f;

    for (int i = 0; i < 100; i++) {
        if (raw >= socToBms[i] && raw <= socToBms[i + 1]) {
            float range = (float)(socToBms[i + 1] - socToBms[i]);
            float delta = (float)(raw - socToBms[i]);
            if (range == 0) return (float)i;
            return (float)i + (delta / range);
        }
    }
    return 0.0f;
}

// =============================================
// CELL VOLTAGE STATISTICS
// =============================================
uint16_t getMinCellVoltage() {
    uint16_t minVal = 65535;
    for(int i = 0; i < MAX_CELLS; i++) {
        if(vehicle.cellVoltages[i] > 0 && vehicle.cellVoltages[i] < minVal) {
            minVal = vehicle.cellVoltages[i];
        }
    }
    return (minVal == 65535) ? 0 : minVal;
}

uint16_t getMaxCellVoltage() {
    uint16_t maxVal = 0;
    for(int i = 0; i < MAX_CELLS; i++) {
        if(vehicle.cellVoltages[i] > maxVal) {
            maxVal = vehicle.cellVoltages[i];
        }
    }
    return maxVal;
}

uint16_t getCellDelta() {
    uint16_t minVal = getMinCellVoltage();
    uint16_t maxVal = getMaxCellVoltage();
    return maxVal - minVal;
}

void updateCellStatistics() {
    vehicle.cellHighestVolt = getMaxCellVoltage();
    vehicle.cellLowestVolt = getMinCellVoltage();
    vehicle.cellDelta = vehicle.cellHighestVolt - vehicle.cellLowestVolt;
    
    // Find which cells
    for(int i = 0; i < MAX_CELLS; i++) {
        if(vehicle.cellVoltages[i] == vehicle.cellHighestVolt) {
            vehicle.cellHighestNum = i + 1; // 1-based index
        }
        if(vehicle.cellVoltages[i] == vehicle.cellLowestVolt) {
            vehicle.cellLowestNum = i + 1;
        }
    }
    
    // Calculate average
    uint32_t sum = 0;
    int count = 0;
    for(int i = 0; i < MAX_CELLS; i++) {
        if(vehicle.cellVoltages[i] > 0) {
            sum += vehicle.cellVoltages[i];
            count++;
        }
    }
    if(count > 0) {
        vehicle.cellAvgVolt = sum / count;
    }
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
    
    for(int i = 0; i < MAX_TEMP_SENSORS; i++) {
        vehicle.cellTemps[i] = DEFAULT_TEMP;
    }
}

void resetBMSData() {
    vehicle.batteryVoltage = 0.0f;
    vehicle.batteryCurrent = 0.0f;
    vehicle.batterySOC = 0;
    vehicle.batterySOH = 100;
    vehicle.batteryCycleCount = 0;
    vehicle.remainingCapacity = 0.0f;
    vehicle.fullCapacity = 0.0f;
    vehicle.chargingCurrent = false;
    
    for(int i = 0; i < MAX_CELLS; i++) {
        vehicle.cellVoltages[i] = 0;
    }
}

void resetChargerData() {
    vehicle.chargerConnected = false;
    vehicle.oriChargerDetected = false;
    vehicle.lastChargerMessage = 0;
    vehicle.chargerVoltage = 0.0f;
    vehicle.chargerCurrent = 0.0f;
    vehicle.chargerStatus = 0;
}

void resetAllVehicleData() {
    vehicle.lastModeByte = MODE_BYTE_PARK;
    vehicle.isCharging = false;
    vehicle.chargingCurrent = false;
    
    // Reset RPM & Speed
    vehicle.rpm = 0;
    vehicle.speed = 0;
    
    resetTemperatureData();
    resetBMSData();
    resetChargerData();
    
    vehicle.odometer = 0;
    vehicle.lastMessageTime = 0;
}

// =============================================
// DEBUG FUNCTIONS
// =============================================
void printVehicleData() {
    Serial.println("\n=== VEHICLE DATA ===");
    Serial.printf("RPM: %d, Speed: %d km/h\n", vehicle.rpm, vehicle.speed);
    Serial.printf("Voltage: %.1fV, Current: %.1fA, Power: %.0fW\n",
                  vehicle.batteryVoltage, vehicle.batteryCurrent, 
                  vehicle.batteryVoltage * vehicle.batteryCurrent);
    Serial.printf("SOC: %d%%, SOH: %d%%, Cycles: %d\n", 
                  vehicle.batterySOC, vehicle.batterySOH, vehicle.batteryCycleCount);
    Serial.printf("Capacity: %.1f/%.1f Ah\n", vehicle.remainingCapacity, vehicle.fullCapacity);
    Serial.printf("Temperatures: ECU=%dC, Motor=%dC, Batt=%dC\n", 
                  vehicle.tempCtrl, vehicle.tempMotor, vehicle.tempBatt);
    
    Serial.printf("Cell Voltages: Highest=%umV (Cell %d), Lowest=%umV (Cell %d), Delta=%umV\n",
                  vehicle.cellHighestVolt, vehicle.cellHighestNum,
                  vehicle.cellLowestVolt, vehicle.cellLowestNum,
                  vehicle.cellDelta);
    
    Serial.printf("BMS Temps: Max=%dC (Cell %d), Min=%dC (Cell %d)\n",
                  vehicle.tempMax, vehicle.tempMaxCell,
                  vehicle.tempMin, vehicle.tempMinCell);
    
    Serial.printf("Charger: Connected=%d, ORI=%d, %.1fV %.1fA\n",
                  vehicle.chargerConnected, vehicle.oriChargerDetected,
                  vehicle.chargerVoltage, vehicle.chargerCurrent);
    
    Serial.printf("Odometer: %lu km\n", vehicle.odometer / 1000);
    Serial.printf("Timing: LastMsg=%lums ago\n", millis() - vehicle.lastMessageTime);
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
