#include "fox_vehicle.h"
#include "fox_config.h"
#include "fox_utils.h"
#include <Arduino.h>

// Titik-titik kunci mapping BMS value -> SOC %
// Format: {BMS_value, SOC_percent}
const uint16_t socKeyPoints[][2] = {
    {60, 0},    // 0% SOC
    {140, 10},  // 10% SOC
    {320, 30},  // 30% SOC
    {500, 50},  // 50% SOC
    {680, 70},  // 70% SOC
    {860, 90},  // 90% SOC
    {950, 100}  // 100% SOC
};

uint8_t bmsToSOC(uint16_t bmsValue) {
    // Handle edge cases
    if(bmsValue <= 60) return 0;
    if(bmsValue >= 950) return 100;
    
    // Cari interval di antara key points
    for(int i = 0; i < 6; i++) { // 6 karena ada 7 titik (0-6)
        if(bmsValue >= socKeyPoints[i][0] && bmsValue < socKeyPoints[i+1][0]) {
            // Linear interpolation
            uint16_t bms1 = socKeyPoints[i][0];
            uint16_t bms2 = socKeyPoints[i+1][0];
            uint8_t soc1 = socKeyPoints[i][1];
            uint8_t soc2 = socKeyPoints[i+1][1];
            
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
    .currentValid = false,
    .socValid = false,
    .currentDirty = false,
    .voltageDirty = false,
    .speedDirty = false,
    .tempDirty = false,
    .socDirty = false,
    .modeDirty = false
};

// Unknown CAN tracking
bool captureUnknownCAN = false;
#define MAX_UNKNOWN_BYTES 30
uint8_t unknownBytesSeen[MAX_UNKNOWN_BYTES];
uint8_t unknownBytesCount = 0;
FoxVehicleMode unknownModeFallback = MODE_PARK;

// Performance tracking
unsigned long lastCriticalUpdate = 0;
unsigned long lastHighUpdate = 0;
unsigned long lastMediumUpdate = 0;

// Charging mode debouncing
unsigned long lastChargingModeChange = 0;
bool chargingModeActive = false;

// =============================================
// HELPER FUNCTIONS (TAMBAHKAN DI AWAL)
// =============================================

bool isByteAlreadySeen(uint8_t byte) {
    for(int i = 0; i < unknownBytesCount; i++) {
        if(unknownBytesSeen[i] == byte) {
            return true;
        }
    }
    return false;
}

void addByteToSeenList(uint8_t byte) {
    if(unknownBytesCount >= MAX_UNKNOWN_BYTES) {
        for(int i = 0; i < MAX_UNKNOWN_BYTES - 1; i++) {
            unknownBytesSeen[i] = unknownBytesSeen[i + 1];
        }
        unknownBytesSeen[MAX_UNKNOWN_BYTES - 1] = byte;
    } else {
        unknownBytesSeen[unknownBytesCount] = byte;
        unknownBytesCount++;
    }
}

FoxVehicleMode determineSafeFallback(uint8_t modeByte) {
    if ((modeByte & 0x0F) == 0x01) {
        return MODE_CHARGING;
    }
    else if (modeByte == 0x00) {
        return MODE_PARK;
    }
    else if ((modeByte & 0xF0) == 0x70) {
        return MODE_DRIVE;
    }
    else if ((modeByte & 0xF0) == 0xB0) {
        return MODE_SPORT;
    }
    return MODE_PARK;
}

// =============================================
// INITIALIZATION
// =============================================

void foxVehicleInit() {
    foxVehicleInitEnhanced(); // Use enhanced by default
}

void foxVehicleInitEnhanced() {
    Serial.println("[VEHICLE] Enhanced module initialized");
    vehicleData.lastUpdate = millis();
    
    // Initialize unknown bytes tracking
    memset(unknownBytesSeen, 0, sizeof(unknownBytesSeen));
    unknownBytesCount = 0;
    
    // Reset charging tracking
    lastChargingModeChange = 0;
    chargingModeActive = false;
    
    // Reset dirty flags
    foxVehicleResetDirtyFlags();
    
    Serial.println("[VEHICLE] Priority system enabled");
}

// =============================================
// DATA ACCESS
// =============================================

FoxVehicleData foxVehicleGetData() {
    return vehicleData;
}

bool foxVehicleDataIsFresh(unsigned long timeoutMs) {
    if (timeoutMs == 0) timeoutMs = DATA_FRESH_TIMEOUT_MS;
    return (millis() - vehicleData.lastUpdate) < timeoutMs;
}

unsigned long foxVehicleGetLastUpdateTime() {
    return vehicleData.lastUpdate;
}

void foxVehicleResetDirtyFlags() {
    vehicleData.currentDirty = false;
    vehicleData.voltageDirty = false;
    vehicleData.speedDirty = false;
    vehicleData.tempDirty = false;
    vehicleData.socDirty = false;
    vehicleData.modeDirty = false;
}

// =============================================
// EVENT-DRIVEN CAN PARSING (MAIN FUNCTION)
// =============================================

void foxVehicleUpdateFromCAN(uint32_t canId, const uint8_t* data, uint8_t len) {
    foxVehicleUpdateFromCANEnhanced(canId, data, len);
}

void foxVehicleUpdateFromCANEnhanced(uint32_t canId, const uint8_t* data, uint8_t len) {
    unsigned long now = millis();
    vehicleData.lastUpdate = now;
    
    // ========== FILTER UNWANTED MESSAGES ==========
    if(canId == FOX_CAN_CHARGER_1 || canId == FOX_CAN_CHARGER_2 || 
       canId == FOX_CAN_BMS_INFO) {
        return; // Skip charger and BMS info messages
    }
    
    // ========== PRIORITY-BASED PARSING ==========
    switch(canId) {
        case FOX_CAN_VOLTAGE_CURRENT:
            // HIGH PRIORITY: Voltage & Current (< 100ms target)
            parseVoltageCurrentEnhanced(data, len);
            lastCriticalUpdate = now;
            break;
            
        case FOX_CAN_MODE_STATUS:
            // HIGH PRIORITY: Mode, RPM, Speed (< 100ms target)
            parseModeStatusEnhanced(data, len);
            lastCriticalUpdate = now;
            break;
            
        case FOX_CAN_TEMP_CTRL_MOT:
            // MEDIUM PRIORITY: Speed & Temp (< 250ms target)
            parseSpeedAndTempEnhanced(data, len);
            lastHighUpdate = now;
            break;
            
        case FOX_CAN_TEMP_BATT_5S:
        case FOX_CAN_TEMP_BATT_SGL:
            // MEDIUM PRIORITY: Battery Temp (< 500ms target)
            parseBatteryTempEnhanced(canId, data, len);
            lastMediumUpdate = now;
            break;
            
        case FOX_CAN_SOC:
            // LOW PRIORITY: SOC (update on change only)
            parseSOCEnhanced(data, len);
            break;
            
        default:
            // Unknown message
            if(captureUnknownCAN) {
                captureUnknownCANData(canId, data, len);
            }
            break;
    }
}

// =============================================
// ENHANCED PARSING FUNCTIONS (EVENT-DRIVEN)
// =============================================

void parseVoltageCurrentEnhanced(const uint8_t* data, uint8_t len) {
    unsigned long currentTime = millis();
    
    if(len >= 4) {
        // VOLTAGE (byte 0-1)
        uint16_t voltageRaw = (data[0] << 8) | data[1];
        float newVoltage = voltageRaw * 0.1f;
        
        // CURRENT (byte 2-3)
        uint16_t rawCurrent = (data[2] << 8) | data[3];
        bool isDischarge = (rawCurrent & 0x8000) != 0;
        float newCurrent;
        
        if (isDischarge) {
            uint16_t complement = (0x10000 - rawCurrent);
            newCurrent = -(complement * 0.1f);
        } else {
            newCurrent = rawCurrent * 0.1f;
        }
        
        // Apply deadzone
        if (fabs(newCurrent) < BMS_DEADZONE_CURRENT) {
            newCurrent = 0.0f;
        }
        
        // PERBAIKAN PENTING: SELALU set dirty flags dan update data
        bool voltageChanged = fabs(newVoltage - vehicleData.voltage) > 0.01;
        bool currentChanged = fabs(newCurrent - vehicleData.current) > 0.005;
        
        if(voltageChanged || currentChanged) {
            vehicleData.voltage = newVoltage;
            vehicleData.voltageValid = true;
            vehicleData.voltageDirty = true;
            
            vehicleData.current = newCurrent;
            vehicleData.currentValid = true;
            vehicleData.currentDirty = true;
            
            // Log update
            static unsigned long lastBMSLog = 0;
            if(currentTime - lastBMSLog > 1000) {
                Serial.print("[BMS] UPDATE - V=");
                Serial.print(newVoltage, 1);
                Serial.print("V, I=");
                Serial.print(newCurrent, 2);
                Serial.println("A");
                lastBMSLog = currentTime;
            }
        }
    }
}

void parseModeStatusEnhanced(const uint8_t* data, uint8_t len) {
    if(len < 8) return;
    
    uint8_t modeByte = data[1];
    unsigned long now = millis();
    
    // ========== DEBOUNCE MODE PARSING ==========
    static unsigned long lastModeParse = 0;
    static uint8_t lastModeByte = 0xFF;
    
    // Skip jika mode byte sama dan baru saja diparse (dalam 50ms)
    if(modeByte == lastModeByte && (now - lastModeParse < 50)) {
        return;
    }
    
    lastModeParse = now;
    lastModeByte = modeByte;
    
    // ========== CHARGING MODE DETECTION WITH DEBOUNCE ==========
    if (IS_CHARGING_MODE(modeByte)) {
        // Debounce charging mode detection (500ms)
        if(!chargingModeActive || (now - lastChargingModeChange > 500)) {
            if (vehicleData.mode != MODE_CHARGING) {
                vehicleData.mode = MODE_CHARGING;
                vehicleData.modeDirty = true;
                chargingModeActive = true;
                lastChargingModeChange = now;
                
                Serial.println("=== CHARGING MODE ACTIVATED ===");
                Serial.print("Charging byte: 0x");
                if(modeByte < 0x10) Serial.print("0");
                Serial.println(modeByte, HEX);
            }
        }
        // Skip further processing saat charging
        return;
    }
    else {
        // Reset charging flag jika bukan charging mode
        if(chargingModeActive) {
            chargingModeActive = false;
            lastChargingModeChange = now;
        }
    }
    
    // ========== NORMAL MODE PARSING ==========
    FoxVehicleMode newMode = MODE_UNKNOWN;
    bool modeChanged = false;
    
    switch(modeByte) {
        case MODE_BYTE_PARK: newMode = MODE_PARK; break;
        case MODE_BYTE_DRIVE: newMode = MODE_DRIVE; break;
        case MODE_BYTE_SPORT: newMode = MODE_SPORT; break;
        case MODE_BYTE_CRUISE: newMode = MODE_CRUISE; break;
        case MODE_BYTE_SPORT_CRUISE: newMode = MODE_SPORT_CRUISE; break;
        case MODE_BYTE_CUTOFF_1: 
        case MODE_BYTE_CUTOFF_2: newMode = MODE_CUTOFF; break;
        case MODE_BYTE_STANDBY_1: 
        case MODE_BYTE_STANDBY_2: 
        case MODE_BYTE_STANDBY_3: newMode = MODE_STANDBY; break;
        case MODE_BYTE_REVERSE: newMode = MODE_REVERSE; break;
        case MODE_BYTE_NEUTRAL: newMode = MODE_NEUTRAL; break;
        default: 
            // Unknown mode handling
            if (!isByteAlreadySeen(modeByte)) {
                logUnknownMode(modeByte);
                addByteToSeenList(modeByte);
                unknownModeFallback = determineSafeFallback(modeByte);
            }
            newMode = unknownModeFallback;
            break;
    }
    
    if (vehicleData.mode != newMode) {
        vehicleData.mode = newMode;
        modeChanged = true;
        vehicleData.modeDirty = true;
    }
    
    // RPM
    uint16_t newRPM = data[2] | (data[3] << 8);
    if (newRPM != vehicleData.rpm) {
        vehicleData.rpm = newRPM;
        vehicleData.rpmValid = true;
        // RPM changes often, only set dirty if significant
        if (abs(newRPM - vehicleData.rpm) > 50) {
            vehicleData.speedDirty = true;
        }
    }
    
    // Speed calculation
    uint16_t newSpeed = newRPM * RPM_TO_KMPH_FACTOR;
    if (abs(newSpeed - vehicleData.speedKmh) > SPEED_UPDATE_THRESHOLD) {
        vehicleData.speedKmh = newSpeed;
        vehicleData.speedValid = true;
        vehicleData.speedDirty = true;
    }
    
    // Temperatures
    uint8_t newCtrlTemp = data[4];
    uint8_t newMotorTemp = data[5];
    
    if (abs(newCtrlTemp - vehicleData.tempController) > TEMP_UPDATE_THRESHOLD) {
        vehicleData.tempController = newCtrlTemp;
        vehicleData.tempDirty = true;
    }
    
    if (abs(newMotorTemp - vehicleData.tempMotor) > TEMP_UPDATE_THRESHOLD) {
        vehicleData.tempMotor = newMotorTemp;
        vehicleData.tempDirty = true;
    }
    
    vehicleData.tempValid = true;
    
    // Sport active flag
    bool newSportActive = (vehicleData.mode == MODE_SPORT || 
                          vehicleData.mode == MODE_SPORT_CRUISE);
    if (newSportActive != vehicleData.sportActive) {
        vehicleData.sportActive = newSportActive;
        vehicleData.modeDirty = true;
    }
    
    // Log mode changes
    if (modeChanged) {
        logModeChange(modeByte);
    }
}

void parseSpeedAndTempEnhanced(const uint8_t* data, uint8_t len) {
    if(len >= 6) {
        // Speed from this message (if available)
        uint8_t newSpeed = data[3];
        if (abs(newSpeed - vehicleData.speedKmh) > SPEED_UPDATE_THRESHOLD) {
            vehicleData.speedKmh = newSpeed;
            vehicleData.speedValid = true;
            vehicleData.speedDirty = true;
        }
    }
}

void parseBatteryTempEnhanced(uint32_t canId, const uint8_t* data, uint8_t len) {
    uint8_t newBattTemp = 0;
    
    if(canId == FOX_CAN_TEMP_BATT_5S && len >= 5) {
        // Take maximum of 5 cell temps
        for(int i = 0; i < 5; ++i) {
            if(data[i] > newBattTemp) newBattTemp = data[i];
        }
    }
    else if(canId == FOX_CAN_TEMP_BATT_SGL && len >= 6) {
        newBattTemp = data[5];
    }
    
    if (abs(newBattTemp - vehicleData.tempBattery) > TEMP_UPDATE_THRESHOLD) {
        vehicleData.tempBattery = newBattTemp;
        vehicleData.tempValid = true;
        vehicleData.tempDirty = true;
    }
}

void parseSOCEnhanced(const uint8_t* data, uint8_t len) {
    if(len < 2) return;
    
    uint16_t bmsValue = (data[0] << 8) | data[1];
    static uint16_t lastBmsValue = 0;
    
    // Filter noise kecil (perubahan < 5 diabaikan)
    if(abs((int)bmsValue - (int)lastBmsValue) < 5) {
        return;
    }
    
    if(bmsValue != lastBmsValue) {
        uint8_t newSOC = bmsToSOC(bmsValue);
        
        if (abs(newSOC - vehicleData.soc) > SOC_UPDATE_THRESHOLD) {
            vehicleData.soc = newSOC;
            vehicleData.socValid = true;
            vehicleData.socDirty = true;
            
            // Log SOC changes
            static uint8_t lastLoggedSOC = 0;
            static unsigned long lastSOCLog = 0;
            unsigned long now = millis();
            
            // Log hanya jika perubahan >= 1% atau sudah 60 detik
            if (abs(newSOC - lastLoggedSOC) >= 1 || (now - lastSOCLog > 60000)) {
                Serial.print("[SOC] BMS: ");
                Serial.print(bmsValue);
                Serial.print(" -> SOC: ");
                Serial.print(newSOC);
                Serial.println("%");
                lastLoggedSOC = newSOC;
                lastSOCLog = now;
            }
        }
        
        lastBmsValue = bmsValue;
    }
}

// =============================================
// LEGACY PARSING FUNCTIONS (for compatibility)
// =============================================

void parseModeStatus(const uint8_t* data, uint8_t len) {
    parseModeStatusEnhanced(data, len);
}

void parseSpeedAndTemp(const uint8_t* data, uint8_t len) {
    parseSpeedAndTempEnhanced(data, len);
}

void parseBatteryTemp5S(const uint8_t* data, uint8_t len) {
    uint32_t dummyId = FOX_CAN_TEMP_BATT_5S;
    parseBatteryTempEnhanced(dummyId, data, len);
}

void parseBatteryTempSingle(const uint8_t* data, uint8_t len) {
    uint32_t dummyId = FOX_CAN_TEMP_BATT_SGL;
    parseBatteryTempEnhanced(dummyId, data, len);
}

void parseVoltageCurrent(const uint8_t* data, uint8_t len) {
    parseVoltageCurrentEnhanced(data, len);
}

void parseSOC(const uint8_t* data, uint8_t len) {
    parseSOCEnhanced(data, len);
}

// =============================================
// DEBUG & DIAGNOSTIC FUNCTIONS
// =============================================

void captureUnknownCANData(uint32_t canId, const uint8_t* data, uint8_t len) {
    if(canId != 0x00000000 && canId != 0xFFFFFFFF) {
        static uint32_t lastUnknownCANId = 0;
        static unsigned long lastCaptureLog = 0;
        
        if(canId != lastUnknownCANId || millis() - lastCaptureLog > 30000) {
            Serial.print("[UNKNOWN] CAN ID: 0x");
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
    Serial.print("[UNKNOWN] Mode Byte: 0x");
    if(modeByte < 0x10) Serial.print("0");
    Serial.print(modeByte, HEX);
    Serial.print(" (");
    Serial.print(modeByte);
    Serial.print(") - Using fallback: ");
    Serial.println(foxVehicleModeToString(determineSafeFallback(modeByte)));
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

// =============================================
// PUBLIC INTERFACE FUNCTIONS
// =============================================

bool foxVehicleIsSportMode() {
    return vehicleData.sportActive;
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

void foxVehicleClearUnknownList() {
    memset(unknownBytesSeen, 0, sizeof(unknownBytesSeen));
    unknownBytesCount = 0;
    Serial.println("Unknown bytes list cleared");
}
