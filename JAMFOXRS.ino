#include "fox_config.h"
#include "fox_display.h"
#include "fox_canbus.h"
#include "fox_vehicle.h"
#include "fox_rtc.h"

int currentPage = PAGE_CLOCK;
int lastNormalPage = PAGE_CLOCK;
bool lastButton = HIGH;
bool setupMode = false;
bool showDebugInfo = false;
unsigned long setupModeStart = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== JAMFOXRS START ===");
    Serial.println("Type HELP for command list");
    Serial.println("========================");
    
    // LANGSUNG INITIALIZE seperti kode lama yang berhasil
    foxDisplayInit();
    Serial.println("Display initialized");
    
    if(foxRTCInit()) {
        Serial.println("RTC: OK");
        foxRTCDebugPrint();
    } else {
        Serial.println("RTC: Not found");
    }
    
    if(foxCANInit()) {
        Serial.println("CAN: OK");
    } else {
        Serial.println("CAN: Failed");
    }
    
    foxVehicleInit();
    Serial.println("Vehicle module initialized");
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("Button configured");
    
    foxDisplayUpdate(currentPage);
    Serial.print("Showing page ");
    Serial.println(currentPage);
    
    Serial.println("=== SETUP COMPLETE ===");
}

void printHelp() {
    Serial.println("\n=== COMMAND LIST ===");
    Serial.println("HELP          - Show this help");
    Serial.println("DAY [1-7]     - Set day of week (1=Minggu, 7=SABTU)");
    Serial.println("TIME HH:MM:SS - Set time (24h format)");
    Serial.println("DATE DD/MM/YYYY - Set date");
    Serial.println("DEBUG         - Show RTC info");
    Serial.println("DEBUG ON/OFF  - Enable/disable periodic debug");
    Serial.println("SETUP         - Enter setup mode");
    Serial.println("SAVE          - Exit setup mode");
    Serial.println("PAGE [1|2]    - Switch display page");
    Serial.println("VEHICLE       - Show vehicle data");
    Serial.println("CAPTURE ON    - Enable unknown CAN ID capture");
    Serial.println("CAPTURE OFF   - Disable unknown CAN ID capture");
    Serial.println("==========================");
    Serial.println("Day mapping: 1=MINGGU, 2=SENIN, 3=SELASA,");
    Serial.println("             4=RABU, 5=KAMIS, 6=JUMAT, 7=SABTU");
    Serial.println("==========================");
}

void processSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        command.toUpperCase();
        
        handleCommand(command);
    }
}

void handleCommand(String command) {
    if (command == "HELP") {
        printHelp();
    }
    else if (command.startsWith("DAY ")) {
        handleDayCommand(command);
    }
    else if (command.startsWith("TIME ")) {
        handleTimeCommand(command);
    }
    else if (command.startsWith("DATE ")) {
        handleDateCommand(command);
    }
    else if (command == "DEBUG") {
        foxRTCDebugPrint();
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
    }
    else if (command == "SAVE") {
        setupMode = false;
        Serial.println("Setup mode exited");
        foxDisplayUpdate(currentPage);
    }
    else if (command.startsWith("PAGE ")) {
        handlePageCommand(command);
    }
    else if (command == "VEHICLE") {
        displayVehicleData();
    }
    else if (command == "CAPTURE ON") {
        foxVehicleEnableUnknownCapture(true);
        Serial.println("=== CAPTURE MODE ON ===");
    }
    else if (command == "CAPTURE OFF") {
        foxVehicleEnableUnknownCapture(false);
        Serial.println("Capture mode disabled");
    }
    else if (command.length() > 0) {
        Serial.println("Unknown command");
    }
}

void handleDayCommand(String command) {
    int dayNum = command.substring(4).toInt();
    if (dayNum >= 1 && dayNum <= 7) {
        if (foxRTCSetDayOfWeek(dayNum)) {
            Serial.println("OK - Day updated");
            foxDisplayUpdate(currentPage);
        }
    } else {
        Serial.println("ERROR - Day must be 1-7");
    }
}

void handleTimeCommand(String command) {
    String timeStr = command.substring(5);
    if (foxRTCSetTimeFromString(timeStr)) {
        Serial.println("OK - Time updated");
        foxDisplayUpdate(currentPage);
    } else {
        Serial.println("ERROR - Format: TIME HH:MM:SS or HH:MM");
    }
}

void handleDateCommand(String command) {
    String dateStr = command.substring(5);
    if (foxRTCSetDateFromString(dateStr)) {
        Serial.println("OK - Date updated");
        foxDisplayUpdate(currentPage);
    } else {
        Serial.println("ERROR - Format: DATE DD/MM/YYYY");
    }
}

void handlePageCommand(String command) {
    int page = command.substring(5).toInt();
    if (page == 1 || page == 2) {
        currentPage = page;
        lastNormalPage = page;
        Serial.print("Switched to page ");
        Serial.println(page);
        foxDisplayUpdate(currentPage);
    }
}

void displayVehicleData() {
    FoxVehicleData data = foxVehicleGetData();
    Serial.println("=== VEHICLE DATA ===");
    Serial.print("Mode: ");
    Serial.println(foxVehicleModeToString(data.mode));
    Serial.print("Sport Active: ");
    Serial.println(data.sportActive ? "YES" : "NO");
    Serial.print("RPM: ");
    Serial.println(data.rpm);
    Serial.print("Speed: ");
    Serial.println(data.speedKmh);
    Serial.print("Temps - ECU:");
    Serial.print(data.tempController);
    Serial.print(" Motor:");
    Serial.print(data.tempMotor);
    Serial.print(" Batt:");
    Serial.println(data.tempBattery);
    Serial.println("===================");
}

void loop() {
    static unsigned long lastUpdate = 0;
    static unsigned long lastDebug = 0;
    static bool blinkState = false;
    unsigned long now = millis();
    
    processSerialCommands();
    foxCANUpdate();
    
    FoxVehicleData vehicleData = foxVehicleGetData();
    
    // Handle page switching - SAMA SEPERTI KODE LAMA
    bool shouldBeOnSportPage = (vehicleData.sportActive || 
                               vehicleData.mode == MODE_SPORT_CRUISE ||
                               vehicleData.mode == MODE_CRUISE);
    
    if (shouldBeOnSportPage) {
        if (currentPage != PAGE_SPORT) {
            lastNormalPage = currentPage;
            currentPage = PAGE_SPORT;
            Serial.println("Auto-switched to SPORT page");
            foxDisplayForceSportUpdate();
            foxDisplayUpdate(currentPage);
        }
    } else {
        if (currentPage == PAGE_SPORT) {
            currentPage = lastNormalPage;
            Serial.println("Returned to normal page");
            foxDisplayUpdate(currentPage);
        }
    }
    
    // Handle button press - SAMA SEPERTI KODE LAMA
    if (!setupMode && currentPage != PAGE_SPORT) {
        bool btn = digitalRead(BUTTON_PIN);
        if(btn == LOW && lastButton == HIGH) {
            delay(50);
            if(digitalRead(BUTTON_PIN) == LOW) {
                currentPage = (currentPage == PAGE_CLOCK) ? PAGE_TEMP : PAGE_CLOCK;
                lastNormalPage = currentPage;
                
                Serial.print("Button pressed! Page ");
                Serial.println(currentPage);
                
                foxDisplayUpdate(currentPage);
            }
        }
        lastButton = btn;
    }
    
    // UPDATE LOGIC - SAMA SEPERTI KODE LAMA
    if (setupMode) {
        if(now - lastUpdate > 500) {
            blinkState = !blinkState;
            foxDisplayShowSetupMode(blinkState);
            lastUpdate = now;
        }
        
        if (now - setupModeStart > 30000) {
            setupMode = false;
            Serial.println("Setup mode timeout - auto exit");
            foxDisplayUpdate(currentPage);
        }
    } 
    else if (currentPage == PAGE_SPORT) {
        foxDisplayUpdate(currentPage);
    }
    else {
        if(now - lastUpdate > 1000) {
            foxDisplayUpdate(currentPage);
            lastUpdate = now;
        }
    }
    
    // Debug info
    if(showDebugInfo && (now - lastDebug > 10000)) {
        if (!setupMode) {
            Serial.print("System - Page:");
            Serial.print(currentPage);
            Serial.print(" Mode:");
            Serial.print(foxVehicleModeToString(vehicleData.mode));
            Serial.print(" Speed:");
            Serial.print(vehicleData.speedKmh);
            Serial.print("km/h RPM:");
            Serial.print(vehicleData.rpm);
            Serial.print(" Temp:");
            Serial.print(vehicleData.tempController);
            Serial.print("/");
            Serial.print(vehicleData.tempMotor);
            Serial.print("/");
            Serial.println(vehicleData.tempBattery);
        }
        lastDebug = now;
    }
    
    delay(10);
}
