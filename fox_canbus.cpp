#include "fox_canbus.h"
#include "fox_config.h"
#include "fox_vehicle.h"
#include <Arduino.h>
#include <Wire.h>

// ESP32 FreeRTOS
#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/twai.h>
#endif

// =============================================
// EXTERN VARIABLES
// =============================================
extern VehicleData vehicle;

// Mutex dari main file
#ifdef ESP32
extern SemaphoreHandle_t dataMutex;
#endif

// =============================================
// THREAD-SAFE DATA ACCESS FUNCTIONS
// =============================================
bool safeUpdateTemperature(int *target, int newValue) {
#ifdef ESP32
    if(dataMutex != NULL) {
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            *target = newValue;
            xSemaphoreGive(dataMutex);
            return true;
        }
        return false;
    }
#endif
    *target = newValue;
    return true;
}

bool safeUpdateFloat(float *target, float newValue) {
#ifdef ESP32
    if(dataMutex != NULL) {
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            *target = newValue;
            xSemaphoreGive(dataMutex);
            return true;
        }
        return false;
    }
#endif
    *target = newValue;
    return true;
}

bool safeUpdateUint8(uint8_t *target, uint8_t newValue) {
#ifdef ESP32
    if(dataMutex != NULL) {
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            *target = newValue;
            xSemaphoreGive(dataMutex);
            return true;
        }
        return false;
    }
#endif
    *target = newValue;
    return true;
}

bool safeUpdateUint16(uint16_t *target, uint16_t newValue) {
#ifdef ESP32
    if(dataMutex != NULL) {
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            *target = newValue;
            xSemaphoreGive(dataMutex);
            return true;
        }
        return false;
    }
#endif
    *target = newValue;
    return true;
}

// =============================================
// INITIALIZATION
// =============================================
bool initCAN() {
#ifdef ESP32
    twai_general_config_t g_config = {
        .mode = TWAI_MODE_LISTEN_ONLY,
        .tx_io = (gpio_num_t)CAN_TX_PIN,
        .rx_io = (gpio_num_t)CAN_RX_PIN,
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 0,
        .rx_queue_len = 5,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0
    };
    
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        return false;
    }
    
    if (twai_start() != ESP_OK) {
        return false;
    }
    
    return true;
#else
    return false;
#endif
}

// =============================================
// CAN PROCESSING (ESP32 only)
// =============================================
#ifdef ESP32
bool readCANMessage() {
    twai_message_t message;
    
    if(twai_receive(&message, pdMS_TO_TICKS(5)) == ESP_OK) {
        vehicle.lastMessageTime = millis();
        
        // Skip charger messages
        if(message.identifier == ID_CHARGER_FAST) {
            return true;
        }
        
        // Process motor controller message
        if(message.identifier == ID_CTRL_MOTOR && message.data_length_code >= 6) {
            uint8_t modeByte = message.data[1];
            bool chargingDetected = isChargingFromModeByte(modeByte);
            
            // Update dengan mutex protection
            #ifdef ESP32
            if(dataMutex != NULL) {
                xSemaphoreTake(dataMutex, portMAX_DELAY);
                if(chargingDetected != vehicle.isCharging) {
                    vehicle.isCharging = chargingDetected;
                }
                vehicle.lastModeByte = modeByte;
                xSemaphoreGive(dataMutex);
            } else {
            #endif
                if(chargingDetected != vehicle.isCharging) {
                    vehicle.isCharging = chargingDetected;
                }
                vehicle.lastModeByte = modeByte;
            #ifdef ESP32
            }
            #endif
            
            // Update temperatures
            safeUpdateTemperature(&vehicle.tempCtrl, message.data[4]);
            safeUpdateTemperature(&vehicle.tempMotor, message.data[5]);
            
            return true;
        }
        
        // Process battery temperature
        else if(message.identifier == ID_BATT_5S && message.data_length_code >= 5) {
            int maxTemp = -100;
            for(int i = 0; i < 5; i++) {
                if(message.data[i] > maxTemp) maxTemp = message.data[i];
            }
            if(maxTemp != -100) {
                safeUpdateTemperature(&vehicle.tempBatt, maxTemp);
            }
            return true;
        }
        
        // PROCESS VOLTAGE & CURRENT DATA - OPTIMIZED
        else if(message.identifier == ID_VOLTAGE_CURRENT && message.data_length_code >= 4) {
            float voltage = parseVoltageFromCAN(message.data);
            float current = parseCurrentFromCAN(message.data);
            
            // ============================================
            // HAPUS DEADZONE DISINI - Update langsung tanpa filter
            // ============================================
            
            // Update dengan mutex
            safeUpdateFloat(&vehicle.batteryVoltage, voltage);
            safeUpdateFloat(&vehicle.batteryCurrent, current);
            
            // Tentukan apakah sedang charging berdasarkan current
            safeUpdateUint8((uint8_t*)&vehicle.chargingCurrent, current > 0.0f);
            
            return true;
        }
        
        // PROCESS SOC DATA
        else if(message.identifier == ID_SOC && message.data_length_code >= 2) {
            // SOC: byte 0 dan 1 (16-bit value, big-endian)
            uint16_t socRaw = (message.data[0] << 8) | message.data[1];
            uint8_t socPercent = bmsToSOC(socRaw);
            
            // Update dengan mutex
            safeUpdateUint16(&vehicle.bmsSOCRaw, socRaw);
            safeUpdateUint8(&vehicle.batterySOC, socPercent);
            
            return true;
        }
    }
    
    return false;
}
#endif

// =============================================
// MAIN CAN UPDATE - WITH FASTER RATE LIMITING
// =============================================
void updateCAN() {
#ifdef ESP32
    static unsigned long lastProcessTime = 0;
    
    // ============================================
    // RATE LIMITING: 33Hz max (30ms) - LEBIH CEPAT!
    // ============================================
    if(millis() - lastProcessTime < 30) {  // ← dari 100ms jadi 30ms
        return;
    }
    lastProcessTime = millis();
    
    // Process beberapa messages
    int processed = 0;
    while(processed < 5 && readCANMessage()) {
        processed++;
    }
    
    // Jika tidak ada message untuk waktu lama, reset state
    if(millis() - vehicle.lastMessageTime > DATA_TIMEOUT) {
        #ifdef ESP32
        if(dataMutex != NULL) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            vehicle.isCharging = false;
            vehicle.lastModeByte = 0;
            vehicle.batteryVoltage = 0.0f;
            vehicle.batteryCurrent = 0.0f;
            vehicle.batterySOC = 0;
            vehicle.chargingCurrent = false;
            xSemaphoreGive(dataMutex);
        } else {
        #endif
            vehicle.isCharging = false;
            vehicle.lastModeByte = 0;
            vehicle.batteryVoltage = 0.0f;
            vehicle.batteryCurrent = 0.0f;
            vehicle.batterySOC = 0;
            vehicle.chargingCurrent = false;
        #ifdef ESP32
        }
        #endif
    }
#else
    // Non-ESP32: simulation mode untuk testing
    static unsigned long lastSimUpdate = 0;
    if(millis() - lastSimUpdate > 2000) {
        // Simulasi data untuk testing
        uint16_t voltageRaw = 480 + random(-20, 20);
        vehicle.batteryVoltage = voltageRaw * 0.1f;
        
        int currentType = random(0, 3);
        float current;
        
        if(currentType == 0) {
            current = -(random(0, 50) * 0.1f);
            vehicle.chargingCurrent = false;
        } else if(currentType == 1) {
            current = random(50, 150) * 0.1f;
            vehicle.chargingCurrent = true;
        } else {
            current = -(random(100, 300) * 0.1f);
            vehicle.chargingCurrent = false;
        }
        
        vehicle.batteryCurrent = current;
        vehicle.batterySOC = random(20, 95);
        
        vehicle.tempCtrl = 45 + random(-5, 5);
        vehicle.tempMotor = 55 + random(-10, 10);
        vehicle.tempBatt = 35 + random(-3, 3);
        
        static uint8_t modes[] = {MODE_BYTE_DRIVE, MODE_BYTE_SPORT, MODE_BYTE_CRUISE, MODE_BYTE_SPORT_CRUISE};
        static int modeIndex = 0;
        vehicle.lastModeByte = modes[modeIndex];
        modeIndex = (modeIndex + 1) % 4;
        
        static bool simCharging = false;
        if(random(0, 100) > 95) {
            simCharging = !simCharging;
        }
        vehicle.isCharging = simCharging;
        
        lastSimUpdate = millis();
    }
#endif
}

// =============================================
// PUBLIC GETTER FUNCTIONS - NO DEBUG
// =============================================
bool getChargingStatus() {
#ifdef ESP32
    bool status = false;
    if(dataMutex != NULL) {
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            status = vehicle.isCharging;
            xSemaphoreGive(dataMutex);
        }
    } else {
        status = vehicle.isCharging;
    }
    return status;
#else
    return vehicle.isCharging;
#endif
}

uint8_t getCurrentModeByte() {
    return vehicle.lastModeByte;
}

uint8_t getCurrentVehicleMode() {
    return vehicle.lastModeByte;
}

bool isSportMode() {
    uint8_t mode = getCurrentModeByte();
    return (mode == MODE_BYTE_SPORT || mode == MODE_BYTE_CUTOFF_SPORT);
}

bool isCruiseMode() {
    uint8_t mode = getCurrentModeByte();
    return (mode == MODE_BYTE_CRUISE || mode == MODE_BYTE_SPORT_CRUISE);
}

bool isCutoffMode() {
    uint8_t mode = getCurrentModeByte();
    return isCutoffFromModeByte(mode);
}

bool isCurrentlyCharging() {
    return getChargingStatus();
}

int getTempCtrl() {
    return vehicle.tempCtrl;
}

int getTempMotor() {
    return vehicle.tempMotor;
}

int getTempBatt() {
    return vehicle.tempBatt;
}

float getBatteryVoltage() {
#ifdef ESP32
    float voltage = 0.0f;
    if(dataMutex != NULL) {
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            voltage = vehicle.batteryVoltage;
            xSemaphoreGive(dataMutex);
        }
    } else {
        voltage = vehicle.batteryVoltage;
    }
    return voltage;
#else
    return vehicle.batteryVoltage;
#endif
}

float getBatteryCurrent() {
#ifdef ESP32
    float current = 0.0f;
    if(dataMutex != NULL) {
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            current = vehicle.batteryCurrent;
            xSemaphoreGive(dataMutex);
        }
    } else {
        current = vehicle.batteryCurrent;
    }
    return current;
#else
    return vehicle.batteryCurrent;
#endif
}

uint8_t getBatterySOC() {
#ifdef ESP32
    uint8_t soc = 0;
    if(dataMutex != NULL) {
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            soc = vehicle.batterySOC;
            xSemaphoreGive(dataMutex);
        }
    } else {
        soc = vehicle.batterySOC;
    }
    return soc;
#else
    return vehicle.batterySOC;
#endif
}

bool isChargingCurrent() {
#ifdef ESP32
    bool charging = false;
    if(dataMutex != NULL) {
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            charging = vehicle.chargingCurrent;
            xSemaphoreGive(dataMutex);
        }
    } else {
        charging = vehicle.chargingCurrent;
    }
    return charging;
#else
    return vehicle.chargingCurrent;
#endif
}

void getBMSDataForDisplay(float &voltage, float &current, uint8_t &soc, bool &isCharging) {
#ifdef ESP32
    if(dataMutex != NULL) {
        if(xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            voltage = vehicle.batteryVoltage;
            current = vehicle.batteryCurrent;
            soc = vehicle.batterySOC;
            isCharging = vehicle.chargingCurrent;
            xSemaphoreGive(dataMutex);
        }
    } else {
        voltage = vehicle.batteryVoltage;
        current = vehicle.batteryCurrent;
        soc = vehicle.batterySOC;
        isCharging = vehicle.chargingCurrent;
    }
#else
    voltage = vehicle.batteryVoltage;
    current = vehicle.batteryCurrent;
    soc = vehicle.batterySOC;
    isCharging = vehicle.chargingCurrent;
#endif
}

// =============================================
// RESET FUNCTION
// =============================================
void resetCANData() {
    #ifdef ESP32
    if(dataMutex != NULL) {
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        vehicle.isCharging = false;
        vehicle.lastModeByte = 0;
        vehicle.tempCtrl = DEFAULT_TEMP;
        vehicle.tempMotor = DEFAULT_TEMP;
        vehicle.tempBatt = DEFAULT_TEMP;
        vehicle.batteryVoltage = 0.0f;
        vehicle.batteryCurrent = 0.0f;
        vehicle.batterySOC = 0;
        vehicle.chargingCurrent = false;
        vehicle.lastMessageTime = 0;
        xSemaphoreGive(dataMutex);
    } else {
    #endif
        vehicle.isCharging = false;
        vehicle.lastModeByte = 0;
        vehicle.tempCtrl = DEFAULT_TEMP;
        vehicle.tempMotor = DEFAULT_TEMP;
        vehicle.tempBatt = DEFAULT_TEMP;
        vehicle.batteryVoltage = 0.0f;
        vehicle.batteryCurrent = 0.0f;
        vehicle.batterySOC = 0;
        vehicle.chargingCurrent = false;
        vehicle.lastMessageTime = 0;
    #ifdef ESP32
    }
    #endif
}
