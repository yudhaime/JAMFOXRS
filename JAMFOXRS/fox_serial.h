#ifndef FOX_SERIAL_H
#define FOX_SERIAL_H

#include <Arduino.h>

extern bool debugModeEnabled;

// Serial print functions
void serialPrint(const char* message);
void serialPrintf(const char* format, ...);
void serialPrintln(const char* message);
void serialPrintfln(const char* format, ...);

// Always print (important messages)
void serialPrintAlways(const char* message);
void serialPrintflnAlways(const char* format, ...);

// Command processing
void processSerialCommands();
void printSystemStartup();

// System functions
void toggleDebugMode(bool enable);
void printSystemStatus();
void printDetailedData();
void emergencyReset();
void printHelp();

#endif
