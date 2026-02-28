#include "fox_task.h"
#include "fox_config.h"
#include "fox_canbus.h"
#include "fox_serial.h"

#ifdef ESP32

// =============================================
// FREERTOS HANDLES
// =============================================
TaskHandle_t canTaskHandle = NULL;

// Semaphores & Mutexes
SemaphoreHandle_t dataMutex = NULL;
SemaphoreHandle_t i2cMutex = NULL;

// =============================================
// FREERTOS INITIALIZATION
// =============================================
void initFreeRTOS() {
    // Create mutexes
    dataMutex = xSemaphoreCreateMutex();
    i2cMutex = xSemaphoreCreateMutex();
    
    if(dataMutex == NULL || i2cMutex == NULL) {
        serialPrintflnAlways("[FreeRTOS] ERROR: Failed to create mutexes!");
        while(1); // Critical error - halt
    }
    
    serialPrintflnAlways("[FreeRTOS] Mutexes created successfully");
}

// =============================================
// CREATE TASKS
// =============================================
void createTasks() {
    delay(100); // Short delay for stability
    
    // Create CAN Task on Core 0 (High Priority)
    xTaskCreatePinnedToCore(
        canTask,                 // Task function
        "CAN_Task",              // Task name
        STACK_SIZE_CAN,          // Stack size
        NULL,                    // Parameters
        TASK_PRIORITY_CAN,       // Priority (HIGHEST)
        &canTaskHandle,          // Task handle
        CORE_CAN                 // Core 0
    );
    
    serialPrintflnAlways("[FreeRTOS] CAN Task created on Core %d", CORE_CAN);
    serialPrintflnAlways("[FreeRTOS] Display/UI will run on Core %d", CORE_DISPLAY);
}

#endif // ESP32
