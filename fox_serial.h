#ifndef FOX_SERIAL_H
#define FOX_SERIAL_H

#include <Arduino.h>

// =============================================
// SIMPLIFIED SERIAL COMMAND PROCESSING
// =============================================

// Command processor
void processSerialCommands();

// Command handlers (HANYA 4 INI)
void handleHelpCommand();
void handleDayCommand(String param);
void handleTimeCommand(String param);
void handleDateCommand(String param);

// Helper
void printHelp();

#endif
