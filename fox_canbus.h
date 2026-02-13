#ifndef CANBUS_H
#define CANBUS_H

#include <Arduino.h>

#ifdef ESP32
#include <driver/twai.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>

// ATOMIC VARIABLES
extern std::atomic<float> realtimeVoltage;
extern std::atomic<float> realtimeCurrent;
extern std::atomic<uint32_t> realtimeUpdateTime;

// Charger detection atomic variables
extern std::atomic<bool> chargerConnected;
extern std::atomic<bool> oriChargerDetected;
extern std::atomic<uint32_t> lastChargerMsgTime;
extern std::atomic<uint32_t> lastOriChargerMsgTime;

// Charging mode flag
extern std::atomic<bool> isChargingMode;

// System health monitor
extern std::atomic<uint32_t> lastSuccessfulLoop;
extern std::atomic<uint32_t> systemErrorCount;

// CAN TASK FUNCTION
void canTask(void *pvParameters);
#endif

// =============================================
// INITIALIZATION
// =============================================
bool initCAN();
void resetCANData();

// =============================================
// CHARGING MODE FUNCTIONS
// =============================================
bool isChargingModeActive();
bool isOriChargerActive();
uint32_t getChargerMessageAge();
void setChargingMode(bool enable);
void updateSystemHealth();

// NEW: Charging page enabled check
bool isChargingPageEnabled();

// =============================================
// REAL-TIME DATA GETTERS
// =============================================
float getRealtimeVoltage();
float getRealtimeCurrent();
unsigned long getRealtimeUpdateTime();
bool isDataFresh();

// Charging mode getters
float getChargingVoltage();
float getChargingCurrent();

// =============================================
// COMPATIBILITY FUNCTIONS
// =============================================
float getBatteryVoltage();
float getBatteryCurrent();

// =============================================
// TEMPERATURE GETTERS (ENABLED)
// =============================================
int getTempCtrl();
int getTempMotor();
int getTempBatt();

// =============================================
// MODE DATA GETTERS (COMPATIBILITY)
// =============================================
uint8_t getCurrentModeByte();
bool isSportMode();
bool isCruiseMode();
bool isCutoffMode();

// =============================================
// OTHER GETTERS
// =============================================
uint8_t getBatterySOC();
bool isChargingCurrent();
bool isChargerConnected();
bool isOriChargerDetected();

// Data access for display
void getBMSDataForDisplay(float &voltage, float &current, uint8_t &soc, bool &isCharging);

// =============================================
// STATISTICS & DEBUG
// =============================================
uint32_t getCANMessageCount();
uint32_t getCANMessagesPerSecond();
void resetCANStatistics();

// =============================================
// SYSTEM HEALTH
// =============================================
uint32_t getSystemErrorCount();
void resetSystemErrorCount();

#endif
