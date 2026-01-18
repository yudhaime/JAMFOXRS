#ifndef FOX_CANBUS_H
#define FOX_CANBUS_H

#include <Arduino.h>

// Basic CAN functions
bool foxCANInit();
void foxCANUpdate();
bool foxCANIsInitialized();

// Enhanced CAN functions (event-driven)
bool foxCANInitEnhanced();
void foxCANUpdateEventDriven();
unsigned long foxCANGetMessageRate();
void foxCANSetThrottleInterval(unsigned long interval);

// Performance monitoring
unsigned long foxCANGetLastMessageTime();
uint32_t foxCANGetMessagesProcessed();
void foxCANResetStatistics();

// Error handling
int foxCANGetErrorCount();
void foxCANClearErrors();

#endif
