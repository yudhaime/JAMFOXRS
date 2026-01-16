#include "fox_vehicle.h"
#include "fox_config.h"
#include <Arduino.h>

// Lookup table SOC to BMS value (0-100%) - untuk referensi
const uint16_t socToBms[101] = {
    0, 60,70,80,90,95,105,115,125,135,140,150,160,170,180,185,195,205,215,225,
    230,240,250,260,270,275,285,295,305,315,320,330,340,350,360,365,375,385,395,405,
    410,420,430,440,450,455,465,475,485,495,500,510,520,530,540,550,555,565,575,585,
    590,600,610,620,630,635,645,655,665,675,680,690,700,710,720,725,735,745,755,765,
    770,780,790,800,810,815,825,835,845,855,860,870,880,890,900,905,915,925,935,945,950
};

// Global variable
FoxVehicleData vehicleData = {
    .mode = MODE_UNKNOWN,
    .sportActive = false,
    .rpm = 0,
    .speedKmh = 0,
    .throttlePercent = 0,
    .tempController = DEFAULT_TEMP,
    .tempMotor = DEFAULT_TEMP,
    .tempBattery = DEFAULT_TEMP,
    .voltage = 0.0,
    .current = 0.0,
    .soc = 0,
    .lastUpdate = 0,
    .rpmValid = false,
    .speedValid = false,
    .tempValid = false,
    .voltageValid = false,
    .socValid = false
};

bool captureUnknownCAN = false;

// ========== [ENHANCED] UNKNOWN MODE PROTECTION ==========
#define MAX_UNKNOWN_BYTES 30
uint8_t unknownBytesSeen[MAX_UNKNOWN_BYTES];
uint8_t unknownBytesCount = 0;
FoxVehicleMode unknownModeFallback = MODE_PARK;  // Safe fallback mode

// Function prototypes untuk internal functions
void parseSOC(const uint8_t* data, uint8_t len);
void logUnknownMode(uint8_t modeByte);
void logModeChange(uint8_t modeByte);
bool isByteAlreadySeen(uint8_t byte);
void addByteToSeenList(uint8_t byte);
FoxVehicleMode determineSafeFallback(uint8_t modeByte);

void foxVehicleInit() {
    Serial.println("Vehicle module initialized");
    vehicleData.lastUpdate = millis();
    
    // Initialize unknown bytes tracking
    memset(unknownBytesSeen, 0, sizeof(unknownBytesSeen));
    unknownBytesCount = 0;
}

// Fungsi: Cek apakah byte sudah pernah dilihat
bool isByteAlreadySeen(uint8_t byte) {
    for(int i = 0; i < unknownBytesCount; i++) {
        if(unknownBytesSeen[i] == byte) {
            return true;
        }
    }
    return false;
}

// Fungsi: Tambah byte ke list yang sudah dilihat
void addByteToSeenList(uint8_t byte) {
    if(unknownBytesCount >= MAX_UNKNOWN_BYTES) {
        // List penuh, geser semua elemen ke kiri
        for(int i = 0; i < MAX_UNKNOWN_BYTES - 1; i++) {
            unknownBytesSeen[i] = unknownBytesSeen[i + 1];
        }
        unknownBytesSeen[MAX_UNKNOWN_BYTES - 1] = byte;
    } else {
        unknownBytesSeen[unknownBytesCount] = byte;
        unknownBytesCount++;
    }
}

// Fungsi: Tentukan safe fallback mode berdasarkan byte
FoxVehicleMode determineSafeFallback(uint8_t modeByte) {
    // Analisis byte untuk pattern yang aman
    if ((modeByte & 0x0F) == 0x01) {
        return MODE_CHARGING;  // Pattern berakhiran 1 = charging
    }
    else if (modeByte == 0x00) {
        return MODE_PARK;
    }
    else if ((modeByte & 0xF0) == 0x70) {
        return MODE_DRIVE;  // Pattern 0x7X = drive
    }
    else if ((modeByte & 0xF0) == 0xB0) {
        return MODE_SPORT;  // Pattern 0xBX = sport
    }
    
    // Default safe fallback
    return MODE_PARK;
}

// Fungsi: Convert BMS raw value to SOC percentage
uint8_t bmsToSOC(uint16_t bmsValue) {
    if(bmsValue >= 950) return 100;
    if(bmsValue <= 50) return 0;
    
    float exactSOC = (bmsValue - 50) / 9.0;
    uint8_t socPercent = (uint8_t)exactSOC;
    
    if(socPercent > 100) socPercent = 100;
    
    return socPercent;
}

// Fungsi: Convert SOC percentage to BMS raw value
uint16_t socToBMS(uint8_t socPercent) {
    if(socPercent > 100) socPercent = 100;
    return 50 + socPercent * 9;
}

// Fungsi parseSOC
void parseSOC(const uint8_t* data, uint8_t len) {
    if(len < 2) return;
    
    uint16_t bmsValue = (data[0] << 8) | data[1];
    static uint16_t lastBmsValue = 0;
    static uint8_t lastSOC = 0;
    
    if(bmsValue != lastBmsValue) {
        vehicleData.soc = bmsToSOC(bmsValue);
        vehicleData.socValid = true;
        lastSOC = vehicleData.soc;
        lastBmsValue = bmsValue;
        
        FoxVehicleData currentData = foxVehicleGetData();
        if(currentData.mode != MODE_CHARGING || lastSOC % 10 == 0) {
            Serial.print("SOC: ");
            Serial.print(vehicleData.soc);
            Serial.println("%");
        }
    }
}

// FUNGSI UTAMA DENGAN PROTECTION
void foxVehicleUpdateFromCAN(uint32_t canId, const uint8_t* data, uint8_t len) {
    // ========== FILTER CHARGER MESSAGES ==========
    if(canId == FOX_CAN_CHARGER_1 || canId == FOX_CAN_CHARGER_2) {
        return;
    }
    
    // ========== IGNORE BMS INFO MESSAGE ==========
    if(canId == FOX_CAN_BMS_INFO) {
        return;
    }
    
    // ========== WHITELIST HANYA MESSAGE YANG DIKENAL ==========
    bool knownMessage = false;
    
    switch(canId) {
        case FOX_CAN_MODE_STATUS:
        case FOX_CAN_TEMP_CTRL_MOT:
        case FOX_CAN_TEMP_BATT_5S:
        case FOX_CAN_TEMP_BATT_SGL:
        case FOX_CAN_VOLTAGE_CURRENT:
        case FOX_CAN_SOC:
            knownMessage = true;
            break;
        default:
            knownMessage = false;
            break;
    }
    
    if(!knownMessage) {
        if(captureUnknownCAN) {
            captureUnknownCANData(canId, data, len);
        }
        return;
    }
    
    // ========== THROTTLE CAN PROCESSING ==========
    static unsigned long lastCANProcess = 0;
    unsigned long now = millis();
    
    FoxVehicleData currentData = foxVehicleGetData();
    unsigned long minInterval = (currentData.mode == MODE_CHARGING) ? 100 : 10;
    
    if(now - lastCANProcess < minInterval) {
        return;
    }
    
    lastCANProcess = now;
    vehicleData.lastUpdate = now;
    
    // ========== PROSES MESSAGE YANG DIKENAL ==========
    if(canId == FOX_CAN_MODE_STATUS && len >= 8) {
        parseModeStatus(data, len);
    }
    else if(canId == FOX_CAN_TEMP_CTRL_MOT && len >= 6) {
        parseSpeedAndTemp(data, len);
    }
    else if(canId == FOX_CAN_TEMP_BATT_5S && len >= 5) {
        parseBatteryTemp5S(data, len);
    }
    else if(canId == FOX_CAN_TEMP_BATT_SGL && len >= 6) {
        parseBatteryTempSingle(data, len);
    }
    else if(canId == FOX_CAN_VOLTAGE_CURRENT && len >= 4) {
        parseVoltageCurrent(data, len);
    }
    else if(canId == FOX_CAN_SOC && len >= 2) {
        parseSOC(data, len);
    }
}

// PARSING VOLTAGE DAN CURRENT
void parseVoltageCurrent(const uint8_t* data, uint8_t len) {
    if(len >= 4) {
        // VOLTAGE
        uint16_t voltageRaw = (data[0] << 8) | data[1];
        float newVoltage = voltageRaw * 0.1f;
        
        // CURRENT
        uint16_t rawCurrent = (data[2] << 8) | data[3];
        bool isDischarge = (rawCurrent & 0x8000) != 0;
        float newCurrent;
        
        if (isDischarge) {
            uint16_t complement = (0x10000 - rawCurrent);
            newCurrent = -(complement * 0.1f);
        } else {
            newCurrent = rawCurrent * 0.1f;
        }
        
        if (fabs(newCurrent) < BMS_DEADZONE_CURRENT) {
            newCurrent = 0.0f;
        }
        
        vehicleData.voltage = newVoltage;
        vehicleData.current = newCurrent;
        vehicleData.voltageValid = true;
        
        FoxVehicleData currentData = foxVehicleGetData();
        if(currentData.mode != MODE_CHARGING) {
            static unsigned long lastLog = 0;
            if(millis() - lastLog > 5000) {
                Serial.print("BMS: V=");
                Serial.print(newVoltage, 1);
                Serial.print("V, I=");
                Serial.print(newCurrent, 1);
                Serial.println("A");
                lastLog = millis();
            }
        }
    }
}

// PARSING MODE STATUS DENGAN PROTECTION
void parseModeStatus(const uint8_t* data, uint8_t len) {
    uint8_t modeByte = data[1];
    
    // DETEKSI CHARGING
    if (IS_CHARGING_MODE(modeByte)) {
        vehicleData.mode = MODE_CHARGING;
    }
    else {
        switch(modeByte) {
            case MODE_BYTE_PARK: vehicleData.mode = MODE_PARK; break;
            case MODE_BYTE_DRIVE: vehicleData.mode = MODE_DRIVE; break;
            case MODE_BYTE_SPORT: vehicleData.mode = MODE_SPORT; break;
            case MODE_BYTE_CRUISE: vehicleData.mode = MODE_CRUISE; break;
            case MODE_BYTE_SPORT_CRUISE: vehicleData.mode = MODE_SPORT_CRUISE; break;
            case MODE_BYTE_CUTOFF_1: 
            case MODE_BYTE_CUTOFF_2: vehicleData.mode = MODE_CUTOFF; break;
            case MODE_BYTE_STANDBY_1: 
            case MODE_BYTE_STANDBY_2: 
            case MODE_BYTE_STANDBY_3: vehicleData.mode = MODE_STANDBY; break;
            case MODE_BYTE_REVERSE: vehicleData.mode = MODE_REVERSE; break;
            case MODE_BYTE_NEUTRAL: vehicleData.mode = MODE_NEUTRAL; break;
            default: 
                // ========== [ENHANCED] UNKNOWN MODE HANDLING ==========
                if (IS_KNOWN_MODE(modeByte)) {
                    // Mode known berdasarkan macro, tapi belum ada di switch
                    // Gunakan safe fallback
                    vehicleData.mode = determineSafeFallback(modeByte);
                    logUnknownMode(modeByte);
                } else {
                    // Mode benar-benar unknown
                    if (!isByteAlreadySeen(modeByte)) {
                        // Log pertama kali
                        logUnknownMode(modeByte);
                        addByteToSeenList(modeByte);
                        
                        // Set safe fallback untuk mencegah display crash
                        unknownModeFallback = determineSafeFallback(modeByte);
                    }
                    // Gunakan safe fallback untuk semua unknown modes
                    vehicleData.mode = unknownModeFallback;
                }
                break;
        }
    }
    
    vehicleData.rpm = data[2] | (data[3] << 8);
    vehicleData.rpmValid = true;
    
    vehicleData.tempController = data[4];
    vehicleData.tempMotor = data[5];
    vehicleData.tempValid = true;
    
    vehicleData.sportActive = (vehicleData.mode == MODE_SPORT || 
                               vehicleData.mode == MODE_SPORT_CRUISE);
    
    logModeChange(modeByte);
}

void parseSpeedAndTemp(const uint8_t* data, uint8_t len) {
    vehicleData.speedKmh = data[3];
    vehicleData.speedValid = true;
}

void parseBatteryTemp5S(const uint8_t* data, uint8_t len) {
    uint8_t maxTemp = 0;
    for(int i = 0; i < 5; ++i) {
        if(data[i] > maxTemp) maxTemp = data[i];
    }
    vehicleData.tempBattery = maxTemp;
    vehicleData.tempValid = true;
}

void parseBatteryTempSingle(const uint8_t* data, uint8_t len) {
    uint8_t battTemp = data[5];
    if(battTemp > vehicleData.tempBattery) {
        vehicleData.tempBattery = battTemp;
        vehicleData.tempValid = true;
    }
}

void captureUnknownCANData(uint32_t canId, const uint8_t* data, uint8_t len) {
    if(canId != 0x00000000 && canId != 0xFFFFFFFF && captureUnknownCAN) {
        static uint32_t lastUnknownCANId = 0;
        static unsigned long lastCaptureLog = 0;
        
        if(canId != lastUnknownCANId || millis() - lastCaptureLog > 30000) {
            Serial.print("UNKNOWN CAN ID: 0x");
            Serial.print(canId, HEX);
            Serial.print(" Len:");
            Serial.print(len);
            Serial.print(" Data:");
            
            for(int i = 0; i < len && i < 8; i++) {
                Serial.print(" ");
                if(data[i] < 0x10) Serial.print("0");
                Serial.print(data[i], HEX);
            }
            Serial.println();
            
            lastUnknownCANId = canId;
            lastCaptureLog = millis();
        }
    }
}

void logUnknownMode(uint8_t modeByte) {
    // Hanya log pertama kali untuk setiap byte
    if (!isByteAlreadySeen(modeByte)) {
        Serial.print("[UNKNOWN] Mode Byte: 0x");
        if(modeByte < 0x10) Serial.print("0");
        Serial.print(modeByte, HEX);
        Serial.print(" (");
        Serial.print(modeByte);
        Serial.print(") - Using fallback: ");
        Serial.println(foxVehicleModeToString(determineSafeFallback(modeByte)));
    }
}

void logModeChange(uint8_t modeByte) {
    static FoxVehicleMode lastMode = MODE_UNKNOWN;
    static unsigned long lastLogTime = 0;
    
    if(vehicleData.mode != lastMode) {
        if(millis() - lastLogTime < 1000) {
            lastMode = vehicleData.mode;
            return;
        }
        
        // Jika masuk charging mode
        if(vehicleData.mode == MODE_CHARGING && lastMode != MODE_CHARGING) {
            Serial.println("=== CHARGING MODE DETECTED ===");
            Serial.print("Byte: 0x");
            if(modeByte < 0x10) Serial.print("0");
            Serial.print(modeByte, HEX);
            Serial.println(" - Button disabled, simple display enabled");
        }
        // Jika keluar charging mode
        else if(lastMode == MODE_CHARGING && vehicleData.mode != MODE_CHARGING) {
            Serial.println("=== NORMAL MODE ===");
        }
        // Untuk mode lainnya
        else if(lastMode != MODE_UNKNOWN && vehicleData.mode != MODE_UNKNOWN) {
            Serial.print("Mode: ");
            Serial.print(foxVehicleModeToString(lastMode));
            Serial.print(" -> ");
            Serial.print(foxVehicleModeToString(vehicleData.mode));
            Serial.print(" (Byte: 0x");
            if(modeByte < 0x10) Serial.print("0");
            Serial.print(modeByte, HEX);
            Serial.println(")");
        }
        
        lastMode = vehicleData.mode;
        lastLogTime = millis();
    }
}

// Fungsi publik
FoxVehicleData foxVehicleGetData() {
    return vehicleData;
}

bool foxVehicleIsSportMode() {
    return vehicleData.sportActive;
}

bool foxVehicleDataIsFresh(unsigned long timeoutMs) {
    return (millis() - vehicleData.lastUpdate) < timeoutMs;
}

String foxVehicleModeToString(FoxVehicleMode mode) {
    switch(mode) {
        case MODE_PARK: return "PARK";
        case MODE_DRIVE: return "DRIVE";
        case MODE_SPORT: return "SPORT";
        case MODE_CUTOFF: return "CUTOFF";
        case MODE_STANDBY: return "STAND";
        case MODE_REVERSE: return "REVERSE";
        case MODE_NEUTRAL: return "NEUTRAL";
        case MODE_CRUISE: return "DRIVE+CRUISE";
        case MODE_SPORT_CRUISE: return "SPORT+CRUISE";
        case MODE_CHARGING: return "CHARGING";
        default: return "UNKNOWN";
    }
}

void foxVehicleEnableUnknownCapture(bool enable) {
    captureUnknownCAN = enable;
    if(enable) {
        Serial.println("=== UNKNOWN CAN CAPTURE ENABLED ===");
    } else {
        Serial.println("Unknown CAN capture disabled");
    }
}

// Fungsi untuk clear unknown bytes list
void foxVehicleClearUnknownList() {
    memset(unknownBytesSeen, 0, sizeof(unknownBytesSeen));
    unknownBytesCount = 0;
    Serial.println("Unknown bytes list cleared");
}
