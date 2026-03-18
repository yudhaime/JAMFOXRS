#include "fox_task.h"
#include "fox_config.h"
#include "fox_canbus.h"
#include "fox_serial.h"
#include "fox_display.h"

#ifdef ESP32

// =============================================
// FREERTOS HANDLES
// =============================================
TaskHandle_t canTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t serialTaskHandle = NULL;

// Semaphores & Mutexes
SemaphoreHandle_t dataMutex = NULL;
SemaphoreHandle_t i2cMutex = NULL;
QueueHandle_t eventQueue = NULL;

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
    
    // Create event queue
    eventQueue = xQueueCreate(10, sizeof(EventMessage));
    
    serialPrintflnAlways("[FreeRTOS] Mutexes and queue created successfully");
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
    
    // Create Display Task on Core 1
    if (DISPLAY_TASK_ENABLED) {
        xTaskCreatePinnedToCore(
            displayTask,             // Task function
            "Display_Task",          // Task name
            DISPLAY_TASK_STACK_SIZE, // Stack size
            NULL,                    // Parameters
            DISPLAY_TASK_PRIORITY,   // Priority
            &displayTaskHandle,      // Task handle
            DISPLAY_TASK_CORE        // Core 1
        );
    }
    
    serialPrintflnAlways("[FreeRTOS] CAN Task created on Core %d", CORE_CAN);
    serialPrintflnAlways("[FreeRTOS] Display Task created on Core %d", DISPLAY_TASK_CORE);
}

// =============================================
// SEND EVENT TO QUEUE
// =============================================
void sendEvent(EventType type, int data) {
    if (eventQueue == NULL) return;
    
    EventMessage msg;
    msg.type = type;
    msg.data = data;
    
    xQueueSend(eventQueue, &msg, 0);
}

#endif // ESP32
