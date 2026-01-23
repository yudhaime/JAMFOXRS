#include "fox_vehicle.h"
#include "fox_config.h"
#include <Arduino.h>

// =============================================
// GLOBAL VEHICLE DATA
// =============================================
VehicleData vehicle = {
    .tempCtrl = DEFAULT_TEMP,
    .tempMotor = DEFAULT_TEMP,
    .tempBatt = DEFAULT_TEMP,
    .batteryVoltage = 0.0f,
    .batteryCurrent = 0.0f,
    .bmsSOCRaw = 0,
    .batterySOC = 0,
    .chargingCurrent = false,
    .isCharging = false,
    .lastModeByte = 0,
    .lastLoggedMode = 0,
    .lastMessageTime = 0
};

// =============================================
// SOC MAPPING TABLE
// =============================================
const SOCPoint socKeyPoints[] = {
    {60, 0},    // 0% SOC
    {140, 10},  // 10% SOC
    {320, 30},  // 30% SOC
    {500, 50},  // 50% SOC
    {680, 70},  // 70% SOC
    {860, 90},  // 90% SOC
    {950, 100}  // 100% SOC
};

const uint8_t socKeyPointsCount = 7;

// =============================================
// PARSING FUNCTIONS - OPTIMIZED FOR RESPONSIVENESS
// =============================================
float parseVoltageFromCAN(const uint8_t* data) {
    // Voltage: byte 0 dan 1 (big-endian: data[0] MSB, data[1] LSB)
    // Format: 0x01E0 = 480 means 48.0V (0.1V per bit)
    uint16_t voltageRaw = (data[0] << 8) | data[1];
    return voltageRaw * 0.1f; // 0.1V per bit
}

float parseCurrentFromCAN(const uint8_t* data) {
    // Current: byte 2 dan 3
    // BIT7 (bit 15) adalah sign bit: 0=charge, 1=discharge
    // Current discharge menggunakan two's complement
    
    uint16_t rawCurrent = (data[2] << 8) | data[3];
    
    // Check sign bit (bit 15) - OPTIMIZED VERSION
    if (rawCurrent & 0x8000) {
        // Discharge: two's complement conversion
        // Formula: -(0x10000 - rawValue) * 0.1
        return -((0x10000 - rawCurrent) * 0.1f);
    } else {
        // Charge: direct value
        return rawCurrent * 0.1f;
    }
}

uint8_t bmsToSOC(uint16_t bmsValue) {
    // Handle edge cases
    if(bmsValue <= 60) return 0;
    if(bmsValue >= 950) return 100;
    
    // Cari interval di antara key points
    for(int i = 0; i < 6; i++) { // 6 karena ada 7 titik (0-6)
        if(bmsValue >= socKeyPoints[i].bmsValue && bmsValue < socKeyPoints[i+1].bmsValue) {
            // Linear interpolation
            uint16_t bms1 = socKeyPoints[i].bmsValue;
            uint16_t bms2 = socKeyPoints[i+1].bmsValue;
            uint8_t soc1 = socKeyPoints[i].socPercent;
            uint8_t soc2 = socKeyPoints[i+1].socPercent;
            
            // Hitung ratio dan interpolasi
            float ratio = (float)(bmsValue - bms1) / (float)(bms2 - bms1);
            uint8_t soc = soc1 + (uint8_t)(ratio * (soc2 - soc1));
            
            // Pastikan dalam range
            if(soc > 100) soc = 100;
            
            return soc;
        }
    }
    
    // Fallback jika tidak ditemukan
    return 50;
}

// =============================================
// MODE DETECTION FUNCTIONS
// =============================================
bool isChargingFromModeByte(uint8_t modeByte) {
    return IS_CHARGING_MODE(modeByte);
}

bool isSportFromModeByte(uint8_t modeByte) {
    return IS_SPORT_MODE(modeByte);
}

bool isCruiseFromModeByte(uint8_t modeByte) {
    return IS_CRUISE_MODE(modeByte);
}

bool isCutoffFromModeByte(uint8_t modeByte) {
    return IS_CUTOFF_MODE(modeByte);
}

const char* getModeName(uint8_t modeByte) {
    switch(modeByte) {
        case MODE_BYTE_PARK: return "PARK";
        case MODE_BYTE_DRIVE: return "DRIVE";
        case MODE_BYTE_SPORT: return "SPORT";
        case MODE_BYTE_CRUISE: return "CRUISE";
        case MODE_BYTE_SPORT_CRUISE: return "SPORT+CRUISE";
        case MODE_BYTE_CUTOFF_DRIVE: return "CUTOFF_DRIVE";
        case MODE_BYTE_CUTOFF_SPORT: return "CUTOFF_SPORT";
        case MODE_BYTE_STANDBY_1: return "STANDBY_1";
        case MODE_BYTE_STANDBY_2: return "STANDBY_2";
        case MODE_BYTE_STANDBY_3: return "STANDBY_3";
        case MODE_BYTE_CHARGING_1: return "CHARGING_1";
        case MODE_BYTE_CHARGING_2: return "CHARGING_2";
        case MODE_BYTE_CHARGING_3: return "CHARGING_3";
        case MODE_BYTE_CHARGING_4: return "CHARGING_4";
        case MODE_BYTE_REVERSE: return "REVERSE";
        case MODE_BYTE_NEUTRAL: return "NEUTRAL";
        default: return "UNKNOWN";
    }
}
