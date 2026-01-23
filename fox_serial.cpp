#include "fox_serial.h"
#include "fox_config.h"
#include "fox_display.h"
#include "fox_rtc.h"
#include "fox_canbus.h"
#include "fox_page.h"
#include "fox_vehicle.h"
#include <Arduino.h>

// =============================================
// COMMAND PROCESSOR - SIMPLIFIED VERSION
// =============================================
void processSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.length() == 0) return;
        
        // Parse command dan parameter
        int spaceIndex = command.indexOf(' ');
        String cmd = command;
        String param = "";
        
        if (spaceIndex > 0) {
            cmd = command.substring(0, spaceIndex);
            param = command.substring(spaceIndex + 1);
        }
        
        cmd.toUpperCase();
        
        // HANYA 4 COMMAND YANG DIPERTAHANKAN
        if (cmd == "HELP") {
            handleHelpCommand();
        }
        else if (cmd == "DAY") {
            handleDayCommand(param);
        }
        else if (cmd == "TIME") {
            handleTimeCommand(param);
        }
        else if (cmd == "DATE") {
            handleDateCommand(param);
        }
        else {
            Serial.println("Unknown command. Type HELP for available commands.");
        }
    }
}

// =============================================
// HANYA 4 COMMAND HANDLERS YANG DIPERTAHANKAN
// =============================================
void handleHelpCommand() {
    printHelp();
}

void handleDayCommand(String param) {
    int dayNum = param.toInt();
    if (dayNum >= 1 && dayNum <= 7) {
        if (setDayOfWeek(dayNum)) {
            Serial.println("OK - Day updated");
            safeDisplayUpdate(currentPage);
        } else {
            Serial.println("ERROR - Failed to set day");
        }
    } else {
        Serial.println("ERROR - Day must be 1-7 (1=Minggu, 7=Sabtu)");
    }
}

void handleTimeCommand(String param) {
    if (setTimeFromString(param)) {
        Serial.println("OK - Time updated");
        safeDisplayUpdate(currentPage);
    } else {
        Serial.println("ERROR - Format: TIME HH:MM:SS or HH:MM");
    }
}

void handleDateCommand(String param) {
    if (setDateFromString(param)) {
        Serial.println("OK - Date updated");
        safeDisplayUpdate(currentPage);
    } else {
        Serial.println("ERROR - Format: DATE DD/MM/YYYY");
    }
}

// =============================================
// HELPER FUNCTIONS - SIMPLIFIED
// =============================================
void printHelp() {
    Serial.println("\n=== AVAILABLE COMMANDS ===");
    Serial.println("HELP          - Show this help");
    Serial.println("DAY [1-7]     - Set day of week (1=Minggu, 7=Sabtu)");
    Serial.println("TIME HH:MM:SS - Set time (24h format)");
    Serial.println("DATE DD/MM/YYYY - Set date");
    Serial.println("==========================");
    Serial.println("Day mapping: 1=MINGGU, 2=SENIN, 3=SELASA,");
    Serial.println("             4=RABU, 5=KAMIS, 6=JUMAT, 7=SABTU");
    Serial.println("==========================");
}
