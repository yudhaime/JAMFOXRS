#ifndef FOX_TASK_H
#define FOX_TASK_H

#include <Arduino.h>

#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// FreeRTOS Handles
extern TaskHandle_t canTaskHandle;
extern TaskHandle_t displayTaskHandle;
extern TaskHandle_t serialTaskHandle;

// Semaphores & Queues
extern SemaphoreHandle_t i2cMutex;
extern SemaphoreHandle_t dataMutex;
extern QueueHandle_t eventQueue;

// Event types
typedef enum {
    EVENT_NONE = 0,
    EVENT_CHARGING_START,
    EVENT_CHARGING_STOP,
    EVENT_SETUP_MODE,
    EVENT_SETUP_EXIT,
    EVENT_DISPLAY_READY,
    EVENT_FORCE_NORMAL,
    EVENT_MODE_CHANGED  // Ditambahkan untuk sync mode changes
} EventType;

typedef struct {
    EventType type;
    int data;
} EventMessage;

// Function declarations
void initFreeRTOS();
void createTasks();
void sendEvent(EventType type, int data = 0);

// Task functions
void canTask(void *parameter);
void displayTask(void *parameter);
void serialTask(void *parameter);
void debugTask(void *parameter);

#endif // ESP32

#endif // FOX_TASK_H
