#include "fox_canbus.h"
#include "fox_config.h"
#include "fox_vehicle.h"
#include <Arduino.h>

#ifdef ESP32
#include <driver/twai.h>
#endif

// Global variables
bool canInitialized = false;
bool canEnhancedMode = false;
unsigned long lastCANMessageTime = 0;
uint32_t canMessagesProcessed = 0;
uint32_t canMessagesPerSecond = 0;
uint32_t canMessageCountThisSecond = 0;
unsigned long lastSecondCheck = 0;
int canErrorCount = 0;
unsigned long canThrottleInterval = 50;
unsigned long lastCANProcess = 0;

// Basic CAN functions
bool foxCANInit() {
    return foxCANInitEnhanced();
}

bool foxCANInitEnhanced() {
#ifdef ESP32
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN,
        (gpio_num_t)CAN_RX_PIN,
        (twai_mode_t)CAN_MODE
    );
    
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        Serial.println("[CAN] Failed to install driver");
        canErrorCount++;
        return false;
    }
    
    if (twai_start() != ESP_OK) {
        Serial.println("[CAN] Failed to start CAN bus");
        canErrorCount++;
        return false;
    }

    Serial.println("[CAN] Enhanced mode initialized (250kbps)");
    canInitialized = true;
    canEnhancedMode = true;
    
    lastCANMessageTime = millis();
    canMessagesProcessed = 0;
    canMessageCountThisSecond = 0;
    lastSecondCheck = millis() / 1000;
    
    return true;
#else
    Serial.println("[CAN] Only for ESP32 platform");
    return false;
#endif
}

bool foxCANIsInitialized() {
    return canInitialized;
}

void foxCANUpdate() {
    foxCANUpdateEventDriven();
}

void foxCANUpdateEventDriven() {
#ifdef ESP32
    if (!canInitialized) return;
    
    unsigned long now = millis();
    
    // Check for CAN silence timeout
    if (now - lastCANMessageTime > CAN_SILENCE_TIMEOUT_MS) {
        static unsigned long lastSilenceLog = 0;
        if (now - lastSilenceLog > 10000) {
            Serial.println("[CAN] No messages for 2 seconds");
            lastSilenceLog = now;
        }
    }
    
    // Standard throttle control
    if (now - lastCANProcess < canThrottleInterval) {
        return;
    }
    
    twai_message_t message;
    
    // Non-blocking receive
    if(twai_receive(&message, pdMS_TO_TICKS(1)) == ESP_OK) {
        lastCANMessageTime = now;
        lastCANProcess = now;
        canMessagesProcessed++;
        canMessageCountThisSecond++;
        
        // Skip known charger/BMS messages yang sangat sering saat charging
        FoxVehicleData vehicleData = foxVehicleGetData();
        bool isChargingMode = (vehicleData.mode == MODE_CHARGING);
        
        bool skipMessage = false;
        
        if(isChargingMode) {
            // Skip beberapa charger message IDs saat charging
            switch(message.identifier) {
                case 0x1810D0F3:
                case 0x1811D0F3:
                case 0x18FFD0F3:
                case 0x1806D0F3:
                case 0x1807D0F3:
                case 0x0A010C10:
                    skipMessage = true;
                    break;
            }
            
            // Skip charger ID range
            if ((message.identifier & 0x1FFFFFF0) == 0x1800D0F0) {
                skipMessage = true;
            }
        }
        
        if (!skipMessage) {
            // Process message
            foxVehicleUpdateFromCANEnhanced(message.identifier, message.data, message.data_length_code);
        }
    }
    
    // Calculate messages per second
    unsigned long currentSecond = now / 1000;
    if (currentSecond != lastSecondCheck) {
        canMessagesPerSecond = canMessageCountThisSecond;
        canMessageCountThisSecond = 0;
        lastSecondCheck = currentSecond;
    }
#endif
}

unsigned long foxCANGetMessageRate() {
    return canMessagesPerSecond;
}

void foxCANSetThrottleInterval(unsigned long interval) {
    canThrottleInterval = interval;
    Serial.print("[CAN] Throttle interval set to ");
    Serial.print(interval);
    Serial.println("ms");
}

unsigned long foxCANGetLastMessageTime() {
    return lastCANMessageTime;
}

uint32_t foxCANGetMessagesProcessed() {
    return canMessagesProcessed;
}

void foxCANResetStatistics() {
    canMessagesProcessed = 0;
    canMessageCountThisSecond = 0;
    canMessagesPerSecond = 0;
    canErrorCount = 0;
    lastCANMessageTime = millis();
    Serial.println("[CAN] Statistics reset");
}

int foxCANGetErrorCount() {
    return canErrorCount;
}

void foxCANClearErrors() {
    canErrorCount = 0;
}
