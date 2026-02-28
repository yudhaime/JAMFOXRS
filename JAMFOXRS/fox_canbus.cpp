#include "fox_canbus.h"
#include "fox_config.h"
#include "fox_vehicle.h"
#include "fox_serial.h"
#include <Arduino.h>
#include <Wire.h>

#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/twai.h>
#include <atomic>
#endif

// =============================================
// EXTERN VARIABLES
// =============================================
extern VehicleData vehicle;
#ifdef ESP32
extern SemaphoreHandle_t dataMutex;
#endif

// =============================================
// GLOBAL REAL-TIME ATOMIC VARIABLES
// =============================================
#ifdef ESP32
// ATOMIC variables for real-time voltage/current
std::atomic<float> realtimeVoltage{0.0f};
std::atomic<float> realtimeCurrent{0.0f};
std::atomic<uint32_t> realtimeUpdateTime{0};

// Charger detection atomic variables
std::atomic<bool> chargerConnected{false};
std::atomic<bool> oriChargerDetected{false};
std::atomic<uint32_t> lastChargerMsgTime{0};
std::atomic<uint32_t> lastOriChargerMsgTime{0};

// Charging mode flag
std::atomic<bool> isChargingMode{false};

// System health monitor
std::atomic<uint32_t> lastSuccessfulLoop{0};
std::atomic<uint32_t> systemErrorCount{0};

// CAN statistics
std::atomic<uint32_t> canMessageCount{0};
std::atomic<uint32_t> canMessagesPerSecond{0};

// Charger message counters
std::atomic<uint32_t> chargerMessageCount{0};
std::atomic<uint32_t> oriChargerMessageCount{0};
#endif

// =============================================
// CAN INITIALIZATION
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
        .rx_queue_len = 20,
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
    
    // Initialize atomic variables
    realtimeVoltage.store(0.0f);
    realtimeCurrent.store(0.0f);
    realtimeUpdateTime.store(0);
    canMessageCount.store(0);
    canMessagesPerSecond.store(0);
    
    // Initialize charger variables
    chargerConnected.store(false);
    oriChargerDetected.store(false);
    lastChargerMsgTime.store(0);
    lastOriChargerMsgTime.store(0);
    isChargingMode.store(false);
    
    // Initialize system health
    lastSuccessfulLoop.store(millis());
    systemErrorCount.store(0);
    
    return true;
#else
    return false;
#endif
}

// =============================================
// CHARGING MODE HELPER FUNCTIONS
// =============================================
#ifdef ESP32
bool isChargingModeActive() {
    return isChargingMode.load(std::memory_order_acquire);
}

bool isOriChargerActive() {
    return oriChargerDetected.load(std::memory_order_acquire);
}

uint32_t getChargerMessageAge() {
    if (!oriChargerDetected.load(std::memory_order_acquire)) {
        return 0xFFFFFFFF;
    }
    return millis() - lastOriChargerMsgTime.load(std::memory_order_acquire);
}

void setChargingMode(bool enable) {
    isChargingMode.store(enable, std::memory_order_release);
}

void updateSystemHealth() {
    lastSuccessfulLoop.store(millis(), std::memory_order_release);
}

uint32_t getSystemErrorCount() {
    return systemErrorCount.load(std::memory_order_acquire);
}

void resetSystemErrorCount() {
    systemErrorCount.store(0, std::memory_order_release);
}

bool isChargingPageEnabled() {
    return CHARGING_PAGE_ENABLED;
}

// =============================================
// SPAM MESSAGE DETECTION
// =============================================
bool isSpamChargerMessage(uint32_t id) {
    if (id == ORI_CHARGER_SPAM_ID) return true;
    if (id == CHARGER_DATA_ID_1 || id == CHARGER_DATA_ID_2) return true;
    if (id == BMS_CHARGING_FLAG) return true;
    return false;
}

// =============================================
// REAL-TIME CAN PARSING - LENGKAP
// =============================================
void parseCANMessage(twai_message_t &message) {
    uint32_t id = message.identifier;
    unsigned long receivedTime = millis();
    
    // Update system health
    lastSuccessfulLoop.store(receivedTime, std::memory_order_release);
    
    // Count total messages
    canMessageCount.fetch_add(1, std::memory_order_relaxed);
    
    // ========== CHARGER DETECTION ==========
    if (id == ORI_CHARGER_SPAM_ID || id == CHARGER_DATA_ID_1 || id == CHARGER_DATA_ID_2 || id == BMS_CHARGING_FLAG) {
        if (id == ORI_CHARGER_SPAM_ID) {
            oriChargerDetected.store(true, std::memory_order_release);
            lastOriChargerMsgTime.store(receivedTime, std::memory_order_release);
            oriChargerMessageCount.fetch_add(1, std::memory_order_relaxed);
            
            if (CHARGING_PAGE_ENABLED && !isChargingMode.load(std::memory_order_acquire)) {
                isChargingMode.store(true, std::memory_order_release);
            }
        } else {
            chargerConnected.store(true, std::memory_order_release);
            lastChargerMsgTime.store(receivedTime, std::memory_order_release);
            chargerMessageCount.fetch_add(1, std::memory_order_relaxed);
        }
        
        // Update vehicle charger data
        vehicle.chargerConnected = chargerConnected.load();
        vehicle.oriChargerDetected = oriChargerDetected.load();
        vehicle.lastChargerMessage = receivedTime;
        
        // Jangan return untuk message yang juga mengandung data
        if (id != ID_VOLTAGE_CURRENT && id != ID_CTRL_MOTOR && id != ID_BATT_5S) {
            return;
        }
    }
    
    // ========== CHARGER DATA (0x1810D0F3 or 0x1811D0F3) ==========
    if ((id == 0x1810D0F3 || id == 0x1811D0F3) && message.data_length_code >= 5) {
        uint16_t vRaw = (uint16_t)((message.data[0] << 8) | message.data[1]);
        vehicle.chargerVoltage = vRaw * 0.1f;
        
        uint16_t iRaw = (uint16_t)((message.data[2] << 8) | message.data[3]);
        vehicle.chargerCurrent = iRaw * 0.1f;
        
        vehicle.chargerStatus = message.data[4];
        vehicle.chargerConnected = true;
        vehicle.lastChargerMessage = receivedTime;
        return;
    }
    
    // ========== CONTROLLER BASIC (0x0A010810) ==========
    if(id == 0x0A010810 && message.data_length_code >= 8) {
        uint8_t m = message.data[1];
        
        // Mode
        vehicle.lastModeByte = m;
        
        // RPM dan Speed
        vehicle.rpm = message.data[2] | (message.data[3] << 8);
        vehicle.speed = (int)(vehicle.rpm * 0.1033f); // Approx conversion
        
        // Temperatures
        vehicle.tempCtrl = message.data[4];
        vehicle.tempMotor = message.data[5];
        vehicle.lastMessageTime = receivedTime;
        return;
    }
    
    // ========== BMS TEMPERATURES (0x0E6C0D09) ==========
    if (id == 0x0E6C0D09 && message.data_length_code >= 5) {
        int sum = 0;
        for (int i = 0; i < 5; i++) {
            vehicle.cellTemps[i] = message.data[i];  // direct °C
            sum += (int)vehicle.cellTemps[i];
        }
        vehicle.tempBatt = sum / 5;
        vehicle.lastMessageTime = receivedTime;
        return;
    }
    
    // ========== VOLTAGE & CURRENT (0x0A6D0D09) ==========
    if(id == ID_VOLTAGE_CURRENT && message.data_length_code >= 8) {
        // Voltage
        uint16_t vRaw = (uint16_t)((message.data[0] << 8) | message.data[1]);
        vehicle.rawVoltageHex = vRaw;
        float voltage = vRaw * 0.1f;
        
        // Current (signed)
        uint16_t iRawU = (uint16_t)((message.data[2] << 8) | message.data[3]);
        vehicle.rawCurrentHex = iRawU;
        int16_t iRawS = (int16_t)iRawU;
        float current = iRawS * 0.1f;
        
        // Deadzone
        if(fabs(current) < CURRENT_DISPLAY_DEADZONE) {
            current = 0.0f;
        }
        
        // Capacity
        uint16_t remainCap = (uint16_t)((message.data[4] << 8) | message.data[5]);
        vehicle.remainingCapacity = remainCap * 0.1f;
        
        uint16_t fullCap = (uint16_t)((message.data[6] << 8) | message.data[7]);
        vehicle.fullCapacity = fullCap * 0.1f;
        
        // Atomic updates
        realtimeVoltage.store(voltage, std::memory_order_release);
        realtimeCurrent.store(current, std::memory_order_release);
        realtimeUpdateTime.store(receivedTime, std::memory_order_release);
        
        // Update vehicle data
        vehicle.batteryVoltage = voltage;
        vehicle.batteryCurrent = current;
        vehicle.chargingCurrent = (current > 1.0f);
        vehicle.lastMessageTime = receivedTime;
        
        return;
    }
    
    // ========== BATTERY HEALTH & SOC (0x0A6E0D09) ==========
    if(id == 0x0A6E0D09 && message.data_length_code >= 6) {
        uint16_t socVal = (uint16_t)((message.data[0] << 8) | message.data[1]);
        vehicle.rawSOCHex = socVal;
        
        // Gunakan lookup table untuk SOC yang akurat
        vehicle.batterySOC = (int)getSoCFromLookup(socVal);
        if(vehicle.batterySOC > 100) vehicle.batterySOC = 100;
        if(vehicle.batterySOC < 0) vehicle.batterySOC = 0;
        
        uint16_t sohVal = (uint16_t)((message.data[2] << 8) | message.data[3]);
        vehicle.batterySOH = (int)(sohVal * 0.1f);
        if(vehicle.batterySOH > 100) vehicle.batterySOH = 100;
        
        vehicle.batteryCycleCount = (uint16_t)((message.data[4] << 8) | message.data[5]);
        vehicle.lastMessageTime = receivedTime;
        return;
    }
    
    // ========== CELL VOLTAGE STATS (0x0A6F0D09) ==========
    if(id == 0x0A6F0D09 && message.data_length_code >= 8) {
        vehicle.cellHighestVolt = (uint16_t)((message.data[0] << 8) | message.data[1]);
        vehicle.cellHighestNum = message.data[2];
        vehicle.cellLowestVolt = (uint16_t)((message.data[3] << 8) | message.data[4]);
        vehicle.cellLowestNum = message.data[5];
        vehicle.cellAvgVolt = (uint16_t)((message.data[6] << 8) | message.data[7]);
        vehicle.cellDelta = vehicle.cellHighestVolt - vehicle.cellLowestVolt;
        vehicle.lastMessageTime = receivedTime;
        return;
    }
    
    // ========== TEMPERATURE STATS (0x0A700D09) ==========
    if(id == 0x0A700D09 && message.data_length_code >= 6) {
        vehicle.tempMax = message.data[0];
        vehicle.tempMaxCell = message.data[1];
        vehicle.tempMin = message.data[4];
        vehicle.tempMinCell = message.data[5];
        vehicle.lastMessageTime = receivedTime;
        return;
    }
    
    // ========== BALANCE STATUS (0x0A730D09) ==========
    if(id == 0x0A730D09 && message.data_length_code >= 6) {
        vehicle.balanceMode = message.data[0];
        vehicle.balanceStatus = message.data[1];
        vehicle.balanceBits[0] = message.data[2];
        vehicle.balanceBits[1] = message.data[3];
        vehicle.balanceBits[2] = message.data[4];
        vehicle.balanceBits[3] = message.data[5];
        
        char buff[20];
        snprintf(buff, sizeof(buff), "%02X %02X %02X %02X %02X %02X",
                 message.data[0], message.data[1], message.data[2], 
                 message.data[3], message.data[4], message.data[5]);
        vehicle.rawBalanceHex = String(buff);
        vehicle.lastMessageTime = receivedTime;
        return;
    }
    
    // ========== CELL VOLTAGES BLOCKS (0x0E64-0x0E69) ==========
    if ((id & 0xFFF0FFFF) == 0x0E600D09) {
        int baseIndex = -1;
        switch (id) {
            case 0x0E640D09: baseIndex = 0;  break;
            case 0x0E650D09: baseIndex = 4;  break;
            case 0x0E660D09: baseIndex = 8;  break;
            case 0x0E670D09: baseIndex = 12; break;
            case 0x0E680D09: baseIndex = 16; break;
            case 0x0E690D09: baseIndex = 20; break;
            default: break;
        }
        if (baseIndex >= 0) {
            for (int i = 0; i < 4 && (baseIndex + i) < MAX_CELLS; i++) {
                int off = i * 2;
                if (off + 1 < message.data_length_code) {
                    vehicle.cellVoltages[baseIndex + i] = 
                        (uint16_t)((message.data[off] << 8) | message.data[off + 1]);
                }
            }
            
            // Update statistics setiap kali dapat data cell baru
            updateCellStatistics();
        }
        vehicle.lastMessageTime = receivedTime;
        return;
    }
}

// =============================================
// CAN TASK - FAST & EFFICIENT
// =============================================
void canTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t localMsgCount = 0;
    uint32_t lastStatsTime = millis();
    
    while(true) {
        uint32_t taskDelay = CAN_UPDATE_RATE_DRIVING_MS;
        
        twai_message_t message;
        int processed = 0;
        
        while(twai_receive(&message, 0) == ESP_OK) {
            parseCANMessage(message);
            processed++;
            localMsgCount++;
            
            if (processed % 20 == 0) {
                taskYIELD();
            }
        }
        
        uint32_t currentTime = millis();
        if(currentTime - lastStatsTime >= 1000) {
            canMessagesPerSecond.store(localMsgCount, std::memory_order_release);
            localMsgCount = 0;
            lastStatsTime = currentTime;
        }
        
        if (oriChargerDetected.load(std::memory_order_acquire)) {
            if (currentTime - lastOriChargerMsgTime.load(std::memory_order_acquire) > CHARGER_TIMEOUT_MS) {
                oriChargerDetected.store(false, std::memory_order_release);
                
                if (isChargingMode.load(std::memory_order_acquire)) {
                    isChargingMode.store(false, std::memory_order_release);
                }
            }
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(taskDelay));
    }
}
#endif

// =============================================
// REAL-TIME DATA GETTERS (ATOMIC)
// =============================================
float getRealtimeVoltage() {
#ifdef ESP32
    return realtimeVoltage.load(std::memory_order_acquire);
#else
    return vehicle.batteryVoltage;
#endif
}

float getRealtimeCurrent() {
#ifdef ESP32
    return realtimeCurrent.load(std::memory_order_acquire);
#else
    return vehicle.batteryCurrent;
#endif
}

unsigned long getRealtimeUpdateTime() {
#ifdef ESP32
    return realtimeUpdateTime.load(std::memory_order_acquire);
#else
    return vehicle.lastMessageTime;
#endif
}

bool isDataFresh() {
#ifdef ESP32
    unsigned long lastUpdate = realtimeUpdateTime.load(std::memory_order_acquire);
    uint32_t timeout = DATA_FRESH_TIMEOUT_NORMAL_MS;
    return (millis() - lastUpdate < timeout);
#else
    return (millis() - vehicle.lastMessageTime < 2000);
#endif
}

float getBatteryVoltage() {
    return getRealtimeVoltage();
}

float getBatteryCurrent() {
    return getRealtimeCurrent();
}

bool isChargerConnected() {
#ifdef ESP32
    return chargerConnected.load(std::memory_order_acquire);
#else
    return false;
#endif
}

bool isOriChargerDetected() {
#ifdef ESP32
    return oriChargerDetected.load(std::memory_order_acquire);
#else
    return false;
#endif
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

uint8_t getCurrentModeByte() {
    return vehicle.lastModeByte;
}

bool isSportMode() {
    return false;
}

bool isCruiseMode() {
    return false;
}

bool isCutoffMode() {
    return false;
}

uint8_t getBatterySOC() {
    return vehicle.batterySOC;
}

bool isChargingCurrent() {
    return vehicle.chargingCurrent;
}

void getBMSDataForDisplay(float &voltage, float &current, uint8_t &soc, bool &isCharging) {
    voltage = getRealtimeVoltage();
    current = getRealtimeCurrent();
    soc = vehicle.batterySOC;
    isCharging = isChargingCurrent();
}

uint32_t getCANMessageCount() {
#ifdef ESP32
    return canMessageCount.load(std::memory_order_acquire);
#else
    return 0;
#endif
}

uint32_t getCANMessagesPerSecond() {
#ifdef ESP32
    return canMessagesPerSecond.load(std::memory_order_acquire);
#else
    return 0;
#endif
}

void resetCANStatistics() {
#ifdef ESP32
    canMessageCount.store(0);
    canMessagesPerSecond.store(0);
#endif
}

void resetCANData() {
#ifdef ESP32
    realtimeVoltage.store(0.0f);
    realtimeCurrent.store(0.0f);
    realtimeUpdateTime.store(0);
    
    chargerConnected.store(false);
    oriChargerDetected.store(false);
    lastChargerMsgTime.store(0);
    lastOriChargerMsgTime.store(0);
    isChargingMode.store(false);
    
    resetCANStatistics();
#endif
    
    vehicle.batteryVoltage = 0.0f;
    vehicle.batteryCurrent = 0.0f;
    vehicle.tempCtrl = DEFAULT_TEMP;
    vehicle.tempMotor = DEFAULT_TEMP;
    vehicle.tempBatt = DEFAULT_TEMP;
    vehicle.lastModeByte = 0;
    vehicle.chargingCurrent = false;
    vehicle.lastMessageTime = 0;
}
