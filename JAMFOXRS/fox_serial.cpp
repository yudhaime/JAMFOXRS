#include "fox_serial.h"
#include "fox_config.h"
#include "fox_display.h"
#include "fox_rtc.h"
#include "fox_canbus.h"
#include "fox_page.h"
#include "fox_vehicle.h"
#include <Arduino.h>
#include <stdarg.h>

// =============================================
// GLOBAL DEBUG FLAG - DEFAULT OFF
// =============================================
bool debugModeEnabled = false;

// =============================================
// EXTERN VARIABLES DECLARATION
// =============================================
extern bool displayInitialized;
extern bool displayReady;
extern int currentPage;
extern VehicleData vehicle;

// =============================================
// FORWARD DECLARATIONS
// =============================================
bool isChargerConnected();
bool isOriChargerDetected();
void switchToPage(int page);
void resetDisplay();
void resetCANData();
bool safeDisplayUpdate(int page);
bool setDayOfWeek(uint8_t dayOfWeek);
bool setTimeFromString(String timeStr);
bool setDateFromString(String dateStr);
float getBatteryVoltage();
float getBatteryCurrent();
int getTempCtrl();
int getTempMotor();
int getTempBatt();

// =============================================
// SERIAL PRINT FUNCTIONS (DEBUG AWARE)
// =============================================
void serialPrint(const char* message) {
    if (debugModeEnabled) Serial.print(message);
}

void serialPrintf(const char* format, ...) {
    if (debugModeEnabled) {
        char buffer[128];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        Serial.print(buffer);
    }
}

void serialPrintln(const char* message) {
    if (debugModeEnabled) Serial.println(message);
}

void serialPrintfln(const char* format, ...) {
    if (debugModeEnabled) {
        char buffer[128];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        Serial.println(buffer);
    }
}

// =============================================
// SPECIAL FUNCTIONS (ALWAYS PRINT - IMPORTANT ONLY)
// =============================================
void serialPrintAlways(const char* message) {
    Serial.print(message);
}

void serialPrintflnAlways(const char* format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.println(buffer);
}

// =============================================
// SYSTEM STARTUP MESSAGE (MINIMAL)
// =============================================
void printSystemStartup() {
    serialPrintflnAlways("\n=== EV DISPLAY SYSTEM ===");
    serialPrintflnAlways("Build: %s %s", __DATE__, __TIME__);
    serialPrintflnAlways("Type HELP for commands");
    serialPrintflnAlways("========================");
}

// =============================================
// DEBUG MODE CONTROL
// =============================================
void toggleDebugMode(bool enable) {
    debugModeEnabled = enable;
    if (enable) {
        serialPrintflnAlways("DEBUG MODE ENABLED");
    } else {
        serialPrintflnAlways("DEBUG MODE DISABLED");
    }
}

// =============================================
// SYSTEM STATUS (SIMPLE VERSION)
// =============================================
void printSystemStatus() {
    serialPrintflnAlways("\n=== SYSTEM STATUS ===");
    serialPrintflnAlways("Uptime: %lu seconds", millis() / 1000);
    serialPrintflnAlways("Page: %d", currentPage);
    serialPrintflnAlways("Display: %s", displayReady ? "OK" : "ERROR");
    
    float voltage = getBatteryVoltage();
    float current = getBatteryCurrent();
    serialPrintflnAlways("Voltage: %.1fV", voltage);
    serialPrintflnAlways("Current: %.1fA", current);
    
    if (voltage > 0.1f && fabs(current) > 0.1f) {
        serialPrintflnAlways("Power: %.1fW", voltage * current);
    }
    
    serialPrintflnAlways("CAN: %s", 
        (millis() - vehicle.lastMessageTime < 5000) ? "ACTIVE" : "NO DATA");
    
    serialPrintflnAlways("Charger: %s", 
        isChargerConnected() ? "CONNECTED" : "NOT CONNECTED");
    
    serialPrintflnAlways("====================");
}

// =============================================
// DETAILED DATA (DEBUG MODE ONLY)
// =============================================
void printDetailedData() {
    if (!debugModeEnabled) return;
    
    serialPrintfln("\n=== DETAILED DATA ===");
    serialPrintfln("Voltage: %.1fV, Current: %.1fA", 
                  getBatteryVoltage(), getBatteryCurrent());
    serialPrintfln("Temperatures: ECU=%dC, Motor=%dC, Batt=%dC",
                  getTempCtrl(), getTempMotor(), getTempBatt());
    serialPrintfln("CAN Last Msg: %lu ms ago", millis() - vehicle.lastMessageTime);
    serialPrintfln("Charger: %s, ORI: %s",
                  isChargerConnected() ? "YES" : "NO",
                  isOriChargerDetected() ? "YES" : "NO");
    serialPrintfln("Free Heap: %d bytes", ESP.getFreeHeap());
    serialPrintfln("======================");
}

// =============================================
// EMERGENCY RESET
// =============================================
void emergencyReset() {
    serialPrintflnAlways("\n!!! EMERGENCY RESET !!!");
    
    // Reset display
    resetDisplay();
    
    // Reset CAN data
    resetCANData();
    
    // Reset page
    currentPage = 1;
    
    // Re-initialize display
    safeDisplayUpdate(currentPage);
    
    serialPrintflnAlways("OK - System reset complete");
}

// =============================================
// HELP MENU (SIMPLIFIED)
// =============================================
void printHelp() {
    serialPrintflnAlways("\n=== AVAILABLE COMMANDS ===");
    serialPrintflnAlways("HELP          - Show this help");
    serialPrintflnAlways("DAY [1-7]     - Set day of week");
    serialPrintflnAlways("TIME HH:MM:SS - Set time (24h format)");
    serialPrintflnAlways("DATE DD/MM/YYYY - Set date");
    serialPrintflnAlways("DEBUG [ON/OFF] - Enable/disable debug");
    serialPrintflnAlways("PAGE [1-4]    - Switch display page");
    serialPrintflnAlways("STATUS        - System status");
    serialPrintflnAlways("DIAG          - Same as STATUS");
    serialPrintflnAlways("RESET         - Emergency reset");
    serialPrintflnAlways("DATA          - Detailed data (debug mode)");
    serialPrintflnAlways("");
    serialPrintflnAlways("=== DAY MAPPING ===");
    serialPrintflnAlways("1=MINGGU, 2=SENIN, 3=SELASA, 4=RABU");
    serialPrintflnAlways("5=KAMIS, 6=JUMAT, 7=SABTU");
    serialPrintflnAlways("==========================");
}

// =============================================
// COMMAND PROCESSOR - SIMPLIFIED
// =============================================
void processSerialCommands() {
    if (!Serial.available()) return;
    
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.length() == 0) return;
    
    // Parse command
    int spaceIndex = command.indexOf(' ');
    String cmd = command;
    String param = "";
    
    if (spaceIndex > 0) {
        cmd = command.substring(0, spaceIndex);
        param = command.substring(spaceIndex + 1);
    }
    
    cmd.toUpperCase();
    
    // ========== ESSENTIAL COMMANDS ==========
    if (cmd == "HELP") {
        printHelp();
    }
    else if (cmd == "DAY") {
        int dayNum = param.toInt();
        if (dayNum >= 1 && dayNum <= 7) {
            if (setDayOfWeek(dayNum)) {
                serialPrintflnAlways("OK - Day updated");
                safeDisplayUpdate(currentPage);
            } else {
                serialPrintflnAlways("ERROR - Failed to set day");
            }
        } else {
            serialPrintflnAlways("ERROR - Day must be 1-7");
        }
    }
    else if (cmd == "TIME") {
        if (setTimeFromString(param)) {
            serialPrintflnAlways("OK - Time updated");
            safeDisplayUpdate(currentPage);
        } else {
            serialPrintflnAlways("ERROR - Format: HH:MM:SS or HH:MM");
        }
    }
    else if (cmd == "DATE") {
        if (setDateFromString(param)) {
            serialPrintflnAlways("OK - Date updated");
            safeDisplayUpdate(currentPage);
        } else {
            serialPrintflnAlways("ERROR - Format: DD/MM/YYYY");
        }
    }
    else if (cmd == "DEBUG") {
        if (param == "ON" || param == "1") {
            toggleDebugMode(true);
        } else if (param == "OFF" || param == "0") {
            toggleDebugMode(false);
        } else {
            toggleDebugMode(!debugModeEnabled);
        }
    }
    else if (cmd == "PAGE") {
        int pageNum = param.toInt();
        if (pageNum >= 1 && pageNum <= 4) {
            switchToPage(pageNum);
            serialPrintflnAlways("OK - Page %d", pageNum);
        } else {
            serialPrintflnAlways("ERROR - Page must be 1-4");
        }
    }
    else if (cmd == "PAGES") {
        serialPrintflnAlways("\n=== PAGE CONFIGURATION ===");
        serialPrintflnAlways("Order: [%d, %d, %d, %d]", 
                            PAGE_ORDER[0], PAGE_ORDER[1], 
                            PAGE_ORDER[2], PAGE_ORDER[3]);
        serialPrintflnAlways("Enabled: 1=%s, 2=%s, 3=%s, 4=%s",
                            PAGE_1_ENABLE ? "YES" : "NO",
                            PAGE_2_ENABLE ? "YES" : "NO",
                            PAGE_3_ENABLE ? "YES" : "NO",
                            PAGE_4_ENABLE ? "YES" : "NO");
        serialPrintflnAlways("Current: %d", currentPage);
        serialPrintflnAlways("==========================");
    }
    else if (cmd == "STATUS" || cmd == "DIAG") {
        printSystemStatus();
    }
    else if (cmd == "RESET") {
        emergencyReset();
    }
    else if (cmd == "DATA") {
        if (!debugModeEnabled) {
            serialPrintflnAlways("ERROR - Enable debug mode first: DEBUG ON");
            return;
        }
        printDetailedData();
    }
    else {
        serialPrintflnAlways("ERROR - Unknown command");
        serialPrintflnAlways("Type HELP for available commands");
    }
}
