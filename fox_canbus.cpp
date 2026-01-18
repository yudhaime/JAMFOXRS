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

// Performance tracking
unsigned long lastCANMessageTime = 0;
uint32_t canMessagesProcessed = 0;
uint32_t canMessagesPerSecond = 0;
uint32_t canMessageCountThisSecond = 0;
unsigned long lastSecondCheck = 0;
int canErrorCount = 0;

// Throttling control
unsigned long canThrottleInterval = 50; // Default 20Hz
unsigned long lastCANProcess = 0;

bool foxCANInit() {
    return foxCANInitEnhanced(); // Use enhanced by default
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
    
    // Reset statistics
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
    foxCANUpdateEventDriven(); // Use enhanced by default
}

void foxCANUpdateEventDriven() {
#ifdef ESP32
    if (!canInitialized) return;
    
    unsigned long now = millis();
    
    // Check for CAN silence timeout
    if (now - lastCANMessageTime > CAN_SILENCE_TIMEOUT_MS) {
        static unsigned long lastSilenceLog = 0;
        if (now - lastSilenceLog > 10000) { // Log every 10 seconds
            Serial.println("[CAN] No messages for 2 seconds");
            lastSilenceLog = now;
        }
    }
    
    // Throttle control for high message rates
    if (now - lastCANProcess < canThrottleInterval) {
        return;
    }
    
    twai_message_t message;
    
    // Non-blocking receive with short timeout
    if(twai_receive(&message, pdMS_TO_TICKS(1)) == ESP_OK) {
        lastCANMessageTime = now;
        lastCANProcess = now;
        canMessagesProcessed++;
        canMessageCountThisSecond++;
        
        // Process message immediately (event-driven)
        foxVehicleUpdateFromCANEnhanced(message.identifier, message.data, message.data_length_code);
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
