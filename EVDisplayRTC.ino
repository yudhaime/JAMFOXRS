#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "canbus.h"
#include "rtc.h"

int currentPage = 1;
bool lastButton = HIGH;
bool setupMode = false;
unsigned long setupModeStart = 0;
bool showDebugInfo = false;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== EV DISPLAY RTC ===");
    Serial.println("Type HELP for command list");
    Serial.println("==========================");
    
    initDisplay();
    Serial.println("Display initialized");
    
    if(initRTC()) {
        Serial.println("RTC: OK");
        printRTCDebug();
    } else {
        Serial.println("RTC: Not found");
    }
    
    if(initCAN()) {
        Serial.println("CAN: OK");
    } else {
        Serial.println("CAN: Failed");
    }
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("Button configured");
    
    updateDisplay(currentPage);
    Serial.print("Showing page ");
    Serial.println(currentPage);
    
    Serial.println("=== SETUP COMPLETE ===");
}

void printHelp() {
    Serial.println("\n=== COMMAND LIST ===");
    Serial.println("HELP          - Show this help");
    Serial.println("DAY [1-7]     - Set day of week (1=Minggu, 7=Sabtu)");
    Serial.println("TIME HH:MM:SS - Set time (24h format)");
    Serial.println("DATE DD/MM/YYYY - Set date");
    Serial.println("DEBUG         - Show RTC info");
    Serial.println("DEBUG ON/OFF  - Enable/disable periodic debug");
    Serial.println("SETUP         - Enter setup mode");
    Serial.println("SAVE          - Exit setup mode");
    Serial.println("PAGE [1|2]    - Switch display page");
    Serial.println("==========================");
    Serial.println("Day mapping: 1=MINGGU, 2=SENIN, 3=SELASA,");
    Serial.println("             4=RABU, 5=KAMIS, 6=JUMAT, 7=SABTU");
}

void processSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        command.toUpperCase();
        
        if (command == "HELP") {
            printHelp();
        }
        else if (command.startsWith("DAY ")) {
            // Command: DAY 1 sampai DAY 7
            int dayNum = command.substring(4).toInt();
            if (dayNum >= 1 && dayNum <= 7) {
                if (setDayOfWeek(dayNum)) {
                    Serial.println("OK - Day updated");
                    updateDisplay(currentPage);
                } else {
                    Serial.println("ERROR - Failed to set day");
                }
            } else {
                Serial.println("ERROR - Day must be 1-7 (1=Minggu, 7=Sabtu)");
            }
        }
        else if (command.startsWith("TIME ")) {
            String timeStr = command.substring(5);
            if (setTimeFromString(timeStr)) {
                Serial.println("OK - Time updated");
                updateDisplay(currentPage);
            } else {
                Serial.println("ERROR - Format: TIME HH:MM:SS or HH:MM");
            }
        }
        else if (command.startsWith("DATE ")) {
            String dateStr = command.substring(5);
            if (setDateFromString(dateStr)) {
                Serial.println("OK - Date updated");
                updateDisplay(currentPage);
            } else {
                Serial.println("ERROR - Format: DATE DD/MM/YYYY");
            }
        }
        else if (command == "DEBUG") {
            printRTCDebug();
        }
        else if (command == "DEBUG ON") {
            showDebugInfo = true;
            Serial.println("Periodic debug enabled");
        }
        else if (command == "DEBUG OFF") {
            showDebugInfo = false;
            Serial.println("Periodic debug disabled");
        }
        else if (command == "SETUP") {
            setupMode = true;
            setupModeStart = millis();
            Serial.println("Setup mode active");
            Serial.println("Use TIME and DATE commands to adjust");
            Serial.println("Type SAVE to exit setup mode");
        }
        else if (command == "SAVE") {
            setupMode = false;
            Serial.println("Setup mode exited");
            updateDisplay(currentPage);
        }
        else if (command.startsWith("PAGE ")) {
            int page = command.substring(5).toInt();
            if (page == 1 || page == 2) {
                currentPage = page;
                Serial.print("Switched to page ");
                Serial.println(page);
                updateDisplay(currentPage);
            } else {
                Serial.println("ERROR - Page must be 1 or 2");
            }
        }
        else if (command.length() > 0) {
            Serial.println("Unknown command");
            Serial.println("Type HELP for command list");
        }
    }
}

void loop() {
    static unsigned long lastUpdate = 0;
    static unsigned long lastDebug = 0;
    static bool blinkState = false;
    
    // Process serial commands
    processSerialCommands();
    
    // Update CAN data
    updateCAN();
    
    // Check button (disabled in setup mode)
    if (!setupMode) {
        bool btn = digitalRead(BUTTON_PIN);
        if(btn == LOW && lastButton == HIGH) {
            delay(50); // debounce
            if(digitalRead(BUTTON_PIN) == LOW) {
                currentPage++;
                if(currentPage > 2) currentPage = 1;
                
                Serial.print("Button pressed! Page ");
                Serial.println(currentPage);
                
                updateDisplay(currentPage);
            }
        }
        lastButton = btn;
    }
    
    // Periodic display update
    if(millis() - lastUpdate > 1000) {
        if (setupMode) {
            // Blink "SETUP" on display in setup mode
            blinkState = !blinkState;
            showSetupMode(blinkState);
            
            // Auto-exit setup mode after 30 seconds
            if (millis() - setupModeStart > 30000) {
                setupMode = false;
                Serial.println("Setup mode timeout - auto exit");
                updateDisplay(currentPage);
            }
        } else {
            updateDisplay(currentPage);
        }
        lastUpdate = millis();
    }
    
    // Debug info setiap 10 detik (HANYA jika showDebugInfo = true)
    if(showDebugInfo && (millis() - lastDebug > 10000)) {
        if (!setupMode) {
            Serial.print("Page:");
            Serial.print(currentPage);
            Serial.print(" Temps:C");
            Serial.print(tempCtrl);
            Serial.print(" M");
            Serial.print(tempMotor);
            Serial.print(" B");
            Serial.println(tempBatt);
        }
        lastDebug = millis();
    }
    
    delay(100);
}