#include "fox_task.h"
#include "fox_config.h"
#include "fox_display.h"
#include "fox_canbus.h"
#include "fox_page.h"

#ifdef ESP32

// =============================================
// FREERTOS HANDLES
// =============================================
TaskHandle_t canTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t serialTaskHandle = NULL;

SemaphoreHandle_t i2cMutex = NULL;
SemaphoreHandle_t dataMutex = NULL;
QueueHandle_t eventQueue = NULL;

// =============================================
// FREERTOS INITIALIZATION
// =============================================
void initFreeRTOS() {
    i2cMutex = xSemaphoreCreateMutex();
    dataMutex = xSemaphoreCreateMutex();
    eventQueue = xQueueCreate(5, sizeof(EventMessage));
}

void createTasks() {
    delay(500);
    
    xTaskCreatePinnedToCore(
        displayTask,
        "Display Task",
        STACK_SIZE_DISPLAY,
        NULL,
        TASK_PRIORITY_DISPLAY,
        &displayTaskHandle,
        CORE_DISPLAY
    );
    
    xTaskCreatePinnedToCore(
        canTask,
        "CAN Task",
        STACK_SIZE_CAN,
        NULL,
        TASK_PRIORITY_CAN,
        &canTaskHandle,
        CORE_CAN
    );
    
    xTaskCreatePinnedToCore(
        serialTask,
        "Serial Task",
        STACK_SIZE_SERIAL,
        NULL,
        TASK_PRIORITY_SERIAL,
        &serialTaskHandle,
        CORE_SERIAL
    );
}

void sendEvent(EventType type, int data) {
    if(eventQueue == NULL) return;
    
    EventMessage msg;
    msg.type = type;
    msg.data = data;
    
    xQueueSend(eventQueue, &msg, 10);
}

// =============================================
// TASKS - MINIMAL VERSION
// =============================================
void canTask(void *parameter) {
    // Tunggu display ready
    while(!displayReady) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while(1) {
        updateCAN();
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(CAN_UPDATE_RATE_MS));
    }
}

void displayTask(void *parameter) {
    while(!displayReady) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while(1) {
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(DISPLAY_UPDATE_RATE_MS));
    }
}

void serialTask(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while(1) {
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(SERIAL_UPDATE_RATE_MS));
    }
}

#endif // ESP32
