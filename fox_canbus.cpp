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

// NEW: Charger detection atomic variables
std::atomic<bool> chargerConnected{false};
std::atomic<bool> oriChargerDetected{false};
std::atomic<uint32_t> lastChargerMsgTime{0};
std::atomic<uint32_t> lastOriChargerMsgTime{0};

// NEW: Charging mode flag
std::atomic<bool> isChargingMode{false};

// NEW: System health monitor
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
        .rx_queue_len = 5,  // VERY SMALL queue untuk charging mode
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
        return 0xFFFFFFFF; // Not active
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

// =============================================
// SPAM MESSAGE DETECTION & FILTERING
// =============================================
bool isSpamChargerMessage(uint32_t id) {
    // Deteksi message spam charger
    if (id == ORI_CHARGER_SPAM_ID) {
        return true; // Spam utama
    }
    
    if (id == CHARGER_DATA_ID_1 || id == CHARGER_DATA_ID_2) {
        return true; // Charger data spam
    }
    
    // BMS charging flags juga bisa spam
    if (id == BMS_CHARGING_FLAG) {
        return true;
    }
    
    return false;
}

// =============================================
// ULTRA-LIGHT CAN PARSING - NO MUTEX DEADLOCK
// =============================================
void parseCANMessage(twai_message_t &message) {
    uint32_t id = message.identifier;
    unsigned long receivedTime = millis();
    
    // Update system health
    lastSuccessfulLoop.store(receivedTime, std::memory_order_release);
    
    // Count total messages
    canMessageCount.fetch_add(1, std::memory_order_relaxed);
    
    // ========== SPAM FILTERING FOR CHARGING ==========
    if (isSpamChargerMessage(id)) {
        // Update charger detection flags dengan rate limiting
        static unsigned long lastSpamProcess = 0;
        if (receivedTime - lastSpamProcess < 100) { // Max 10Hz untuk spam
            return; // Skip processing
        }
        lastSpamProcess = receivedTime;
        
        // Update charger status
        if (id == ORI_CHARGER_SPAM_ID) {
            oriChargerDetected.store(true, std::memory_order_release);
            lastOriChargerMsgTime.store(receivedTime, std::memory_order_release);
            oriChargerMessageCount.fetch_add(1, std::memory_order_relaxed);
            
            // Auto-activate charging mode
            if (!isChargingMode.load(std::memory_order_acquire)) {
                isChargingMode.store(true, std::memory_order_release);
            }
        } else {
            chargerConnected.store(true, std::memory_order_release);
            lastChargerMsgTime.store(receivedTime, std::memory_order_release);
            chargerMessageCount.fetch_add(1, std::memory_order_relaxed);
        }
        
        // Untuk spam messages, kita hanya update flags
        if (id != ID_VOLTAGE_CURRENT && id != ID_CTRL_MOTOR && id != ID_BATT_5S) {
            return; // Early exit untuk pure spam messages
        }
    }
    
    // ========== PRIORITY 1: VOLTAGE & CURRENT ==========
    if(id == ID_VOLTAGE_CURRENT && message.data_length_code >= 8) {
        // Voltage parsing - REAL-TIME
        uint16_t vRaw = (uint16_t)((message.data[0] << 8) | message.data[1]);
        float voltage = vRaw * 0.1f;
        
        // Current parsing - REAL-TIME
        uint16_t iRawU = (uint16_t)((message.data[2] << 8) | message.data[3]);
        int16_t iRawS = (int16_t)iRawU;
        float current = iRawS * 0.1f;
        
        // Adjust deadzone based on charging mode
        float deadzone = isChargingMode.load(std::memory_order_acquire) ? 
                        CHARGING_CURRENT_DEADZONE : CURRENT_DISPLAY_DEADZONE;
        
        if(fabs(current) < deadzone) {
            current = 0.0f;
        }
        
        // ATOMIC UPDATES ONLY - NO MUTEX!
        realtimeVoltage.store(voltage, std::memory_order_release);
        realtimeCurrent.store(current, std::memory_order_release);
        realtimeUpdateTime.store(receivedTime, std::memory_order_release);
        
        // Update vehicle data directly (atomic enough for our purposes)
        vehicle.batteryVoltage = voltage;
        vehicle.batteryCurrent = current;
        vehicle.chargingCurrent = (current > 0.0f);
        vehicle.lastMessageTime = receivedTime;
        
        return;
    }
    
    // ========== PRIORITY 2: TEMPERATURES & MODE ==========
    if(id == ID_CTRL_MOTOR && message.data_length_code >= 6) {
        // Update dengan rate limiting
        static unsigned long lastCtrlMotorUpdate = 0;
        unsigned long updateInterval = isChargingMode.load(std::memory_order_acquire) ? 
                                      2000 : 500;
        
        if(receivedTime - lastCtrlMotorUpdate > updateInterval) {
            vehicle.tempCtrl = message.data[4];
            vehicle.tempMotor = message.data[5];
            vehicle.lastModeByte = message.data[1];
            vehicle.lastMessageTime = receivedTime;
            lastCtrlMotorUpdate = receivedTime;
        }
        return;
    }
    
    // ID_BATT_5S: Battery temperature
    if(id == ID_BATT_5S && message.data_length_code >= 1) {
        // Update dengan rate limiting
        static unsigned long lastBattTempUpdate = 0;
        unsigned long updateInterval = isChargingMode.load(std::memory_order_acquire) ? 
                                      5000 : 2000;
        
        if(receivedTime - lastBattTempUpdate > updateInterval) {
            vehicle.tempBatt = message.data[0];
            vehicle.lastMessageTime = receivedTime;
            lastBattTempUpdate = receivedTime;
        }
        return;
    }
    
    // ========== IGNORE NON-ESSENTIAL MESSAGES DURING CHARGING ==========
    if (isChargingMode.load(std::memory_order_acquire)) {
        return; // Skip processing non-essential messages
    }
}
#endif

// =============================================
// CAN TASK - ULTRA-LIGHT FOR CHARGING
// =============================================
#ifdef ESP32
void canTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t localMsgCount = 0;
    uint32_t lastStatsTime = millis();
    
    while(true) {
        // Determine update rate based on charging mode
        uint32_t taskDelay = isChargingMode.load(std::memory_order_acquire) ? 
                            CAN_TASK_UPDATE_CHARGING_MS : CAN_TASK_UPDATE_DRIVING_MS;
        
        // Process messages dengan limits ketat
        twai_message_t message;
        int processed = 0;
        int maxMessages = isChargingMode.load(std::memory_order_acquire) ? 2 : 8;
        
        while(processed < maxMessages && twai_receive(&message, 0) == ESP_OK) {
            parseCANMessage(message);
            processed++;
            localMsgCount++;
            
            // Yield sangat sering saat charging
            if (isChargingMode.load(std::memory_order_acquire) && processed % 1 == 0) {
                taskYIELD();
            }
        }
        
        // Update statistics every second
        uint32_t currentTime = millis();
        if(currentTime - lastStatsTime >= 1000) {
            canMessagesPerSecond.store(localMsgCount, std::memory_order_release);
            localMsgCount = 0;
            lastStatsTime = currentTime;
        }
        
        // Check charger timeout
        if (oriChargerDetected.load(std::memory_order_acquire)) {
            if (currentTime - lastOriChargerMsgTime.load(std::memory_order_acquire) > CHARGER_TIMEOUT_MS) {
                oriChargerDetected.store(false, std::memory_order_release);
                
                // Deactivate charging mode jika sudah timeout
                if (isChargingMode.load(std::memory_order_acquire)) {
                    // Tunggu 10 detik lagi untuk pastikan
                    if (currentTime - lastOriChargerMsgTime.load(std::memory_order_acquire) > 10000) {
                        isChargingMode.store(false, std::memory_order_release);
                    }
                }
            }
        }
        
        // Adaptive task delay
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
    uint32_t timeout = isChargingMode.load(std::memory_order_acquire) ? 5000 : 2000;
    return (millis() - lastUpdate < timeout);
#else
    return (millis() - vehicle.lastMessageTime < 2000);
#endif
}

// =============================================
// CHARGING MODE SPECIFIC GETTERS
// =============================================
float getChargingVoltage() {
#ifdef ESP32
    if (isChargingMode.load(std::memory_order_acquire)) {
        // Simple moving average untuk charging
        static float smoothedVoltage = 0.0f;
        float currentVoltage = realtimeVoltage.load(std::memory_order_acquire);
        
        if (smoothedVoltage == 0.0f) {
            smoothedVoltage = currentVoltage;
        } else {
            smoothedVoltage = smoothedVoltage * 0.95f + currentVoltage * 0.05f;
        }
        return smoothedVoltage;
    }
    return getRealtimeVoltage();
#else
    return vehicle.batteryVoltage;
#endif
}

float getChargingCurrent() {
#ifdef ESP32
    if (isChargingMode.load(std::memory_order_acquire)) {
        // Simple moving average untuk charging
        static float smoothedCurrent = 0.0f;
        float currentCurrent = realtimeCurrent.load(std::memory_order_acquire);
        
        if (smoothedCurrent == 0.0f) {
            smoothedCurrent = currentCurrent;
        } else {
            smoothedCurrent = smoothedCurrent * 0.95f + currentCurrent * 0.05f;
        }
        return smoothedCurrent;
    }
    return getRealtimeCurrent();
#else
    return vehicle.batteryCurrent;
#endif
}

// =============================================
// COMPATIBILITY FUNCTIONS
// =============================================
float getBatteryVoltage() {
    if (isChargingModeActive()) {
        return getChargingVoltage();
    }
    return getRealtimeVoltage();
}

float getBatteryCurrent() {
    if (isChargingModeActive()) {
        return getChargingCurrent();
    }
    return getRealtimeCurrent();
}

// =============================================
// CHARGER DETECTION FUNCTIONS
// =============================================
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

// =============================================
// TEMPERATURE GETTERS
// =============================================
int getTempCtrl() {
    return vehicle.tempCtrl;
}

int getTempMotor() {
    return vehicle.tempMotor;
}

int getTempBatt() {
    return vehicle.tempBatt;
}

// =============================================
// MODE DATA GETTERS
// =============================================
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

// =============================================
// OTHER GETTERS
// =============================================
uint8_t getBatterySOC() {
    return 0;
}

bool isChargingCurrent() {
    return vehicle.chargingCurrent;
}

void getBMSDataForDisplay(float &voltage, float &current, uint8_t &soc, bool &isCharging) {
    if (isChargingModeActive()) {
        voltage = getChargingVoltage();
        current = getChargingCurrent();
    } else {
        voltage = getRealtimeVoltage();
        current = getRealtimeCurrent();
    }
    soc = 0;
    isCharging = isChargingCurrent();
}

// =============================================
// STATISTICS
// =============================================
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
    // Reset atomic variables
    realtimeVoltage.store(0.0f);
    realtimeCurrent.store(0.0f);
    realtimeUpdateTime.store(0);
    
    // Reset charger states
    chargerConnected.store(false);
    oriChargerDetected.store(false);
    lastChargerMsgTime.store(0);
    lastOriChargerMsgTime.store(0);
    isChargingMode.store(false);
    
    // Reset statistics
    resetCANStatistics();
#endif
    
    // Reset vehicle data
    vehicle.batteryVoltage = 0.0f;
    vehicle.batteryCurrent = 0.0f;
    vehicle.tempCtrl = DEFAULT_TEMP;
    vehicle.tempMotor = DEFAULT_TEMP;
    vehicle.tempBatt = DEFAULT_TEMP;
    vehicle.lastModeByte = 0;
    vehicle.chargingCurrent = false;
    vehicle.lastMessageTime = 0;
}
