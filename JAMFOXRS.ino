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

// Array untuk page yang enabled
int enabledPages[4]; // Maks 3 user pages + 1 sport
int enabledPageCount = 0;
int currentPageIndex = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== JAMFOXRSBETA START (PAGE CONFIG SYSTEM) ===");
    Serial.println("Type HELP for command list");
    Serial.println("=============================================");
    
    // Inisialisasi array enabled pages
    initEnabledPages();
    
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
    
    // Tampilkan konfigurasi page
    printPageConfiguration();
    
    foxDisplayUpdate(currentPage);
    Serial.print("Showing page ");
    Serial.println(currentPage);
    
    Serial.println("=== SETUP COMPLETE ===");
}

void initEnabledPages() {
    enabledPageCount = 0;
    
    // User pages (button cycle)
    #if PAGE_CLOCK_ENABLED
        enabledPages[enabledPageCount++] = PAGE_CLOCK;
    #endif
    
    #if PAGE_TEMP_ENABLED
        enabledPages[enabledPageCount++] = PAGE_TEMP;
    #endif
    
    #if PAGE_ELECTRICAL_ENABLED
        enabledPages[enabledPageCount++] = PAGE_ELECTRICAL;
    #endif
    
    // Sport page (always last, auto-trigger only)
    enabledPages[enabledPageCount++] = PAGE_SPORT;
    
    Serial.print("Enabled pages: ");
    for(int i = 0; i < enabledPageCount; i++) {
        Serial.print(enabledPages[i]);
        Serial.print(" ");
    }
    Serial.println();
}

void printPageConfiguration() {
    Serial.println("\n=== PAGE CONFIGURATION ===");
    Serial.print("Page 1 (Clock): ");
    Serial.println(PAGE_CLOCK_ENABLED ? "ENABLED" : "DISABLED");
    Serial.print("Page 2 (Temperature): ");
    Serial.println(PAGE_TEMP_ENABLED ? "ENABLED" : "DISABLED");
    Serial.print("Page 3 (Electrical): ");
    Serial.println(PAGE_ELECTRICAL_ENABLED ? "ENABLED" : "DISABLED");
    Serial.println("Page 9 (Sport): ALWAYS ENABLED (auto-trigger)");
    Serial.print("Total user pages: ");
    Serial.println(MAX_USER_PAGES);
    Serial.println("==========================\n");
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
    
    // Generate page command based on enabled pages
    Serial.print("PAGE [");
    #if PAGE_CLOCK_ENABLED
        Serial.print("1|");
    #endif
    #if PAGE_TEMP_ENABLED
        Serial.print("2|");
    #endif
    #if PAGE_ELECTRICAL_ENABLED
        Serial.print("3|");
    #endif
    Serial.println("9] - Switch display page");
    
    Serial.println("VEHICLE       - Show vehicle data");
    Serial.println("CAPTURE ON    - Enable unknown CAN ID capture");
    Serial.println("CAPTURE OFF   - Disable unknown CAN ID capture");
    Serial.println("CONFIG        - Show page configuration");
    Serial.println("==========================");
    
    printPageConfiguration();
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
    else if (command == "CONFIG") {
        printPageConfiguration();
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
    
    // Validasi page berdasarkan konfigurasi
    bool isValidPage = false;
    
    switch(page) {
        case PAGE_CLOCK:
            isValidPage = PAGE_CLOCK_ENABLED;
            break;
        case PAGE_TEMP:
            isValidPage = PAGE_TEMP_ENABLED;
            break;
        case PAGE_ELECTRICAL:
            isValidPage = PAGE_ELECTRICAL_ENABLED;
            break;
        case PAGE_SPORT:
            isValidPage = true; // Sport page always valid
            break;
        default:
            isValidPage = false;
    }
    
    if (isValidPage) {
        currentPage = page;
        
        // Jika bukan page sport, update lastNormalPage dan cari index
        if(page != PAGE_SPORT) {
            lastNormalPage = page;
            
            // Cari index page ini di array
            for(int i = 0; i < enabledPageCount; i++) {
                if(enabledPages[i] == page) {
                    currentPageIndex = i;
                    break;
                }
            }
        }
        
        Serial.print("Switched to page ");
        Serial.println(page);
        foxDisplayUpdate(currentPage);
    } else {
        Serial.print("ERROR - Page ");
        Serial.print(page);
        Serial.println(" is disabled or invalid");
        Serial.print("Enabled pages: ");
        
        #if PAGE_CLOCK_ENABLED
            Serial.print("1 ");
        #endif
        #if PAGE_TEMP_ENABLED
            Serial.print("2 ");
        #endif
        #if PAGE_ELECTRICAL_ENABLED
            Serial.print("3 ");
        #endif
        Serial.println("9");
    }
}

int getNextUserPage() {
    if(MAX_USER_PAGES == 0) return PAGE_CLOCK; // Fallback
    
    currentPageIndex++;
    if(currentPageIndex >= (enabledPageCount - 1)) { // -1 karena sport page di akhir
        currentPageIndex = 0;
    }
    
    return enabledPages[currentPageIndex];
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
    Serial.print("Temps - ECU:");
    Serial.print(data.tempController);
    Serial.print("°C Motor:");
    Serial.print(data.tempMotor);
    Serial.print("°C Batt:");
    Serial.print(data.tempBattery);
    Serial.println("°C");
    
    // Voltage dan Current (masih bisa baca walau tidak ditampilkan)
    Serial.print("Voltage: ");
    Serial.print(data.voltage, 1);
    Serial.print("V, Current: ");
    Serial.print(data.current, 1);
    Serial.println("A");
    
    if(data.soc > 0) {
        Serial.print("Battery SOC: ");
        Serial.print(data.soc);
        Serial.println("%");
    }
    
    Serial.print("Data Fresh: ");
    Serial.println(foxVehicleDataIsFresh() ? "YES" : "NO");
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
    
    // Handle page switching (Sport page auto-trigger)
    bool shouldBeOnSportPage = (vehicleData.sportActive || 
                               vehicleData.mode == MODE_SPORT_CRUISE ||
                               vehicleData.mode == MODE_CRUISE);
    
    if (shouldBeOnSportPage) {
        if (currentPage != PAGE_SPORT) {
            lastNormalPage = currentPage;
            currentPage = PAGE_SPORT;
            Serial.println("Auto-switched to SPORT page (9)");
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
    
    // Handle button press - Cycle melalui enabled pages saja
    if (!setupMode && currentPage != PAGE_SPORT && MAX_USER_PAGES > 0) {
        bool btn = digitalRead(BUTTON_PIN);
        if(btn == LOW && lastButton == HIGH) {
            delay(50);
            if(digitalRead(BUTTON_PIN) == LOW) {
                // Dapatkan next page dari array
                int nextPage = getNextUserPage();
                
                currentPage = nextPage;
                lastNormalPage = currentPage;
                
                Serial.print("Button pressed! Page ");
                Serial.println(currentPage);
                
                foxDisplayUpdate(currentPage);
            }
        }
        lastButton = btn;
    }
    
    // UPDATE LOGIC dengan interval berbeda per page
    if (setupMode) {
        if(now - lastUpdate > UPDATE_INTERVAL_SETUP_MS) {
            blinkState = !blinkState;
            foxDisplayShowSetupMode(blinkState);
            lastUpdate = now;
        }
        
        if (now - setupModeStart > SETUP_TIMEOUT_MS) {
            setupMode = false;
            Serial.println("Setup mode timeout - auto exit");
            foxDisplayUpdate(currentPage);
        }
    } 
    else if (currentPage == PAGE_SPORT) {
        // Sport page update sangat cepat
        if(now - lastUpdate > UPDATE_INTERVAL_SPORT_MS) {
            foxDisplayUpdate(currentPage);
            lastUpdate = now;
        }
    }
    else {
        // Normal pages dengan interval normal
        if(now - lastUpdate > UPDATE_INTERVAL_NORMAL_MS) {
            foxDisplayUpdate(currentPage);
            lastUpdate = now;
        }
    }
    
    // Debug info
    if(showDebugInfo && (now - lastDebug > DEBUG_INTERVAL_MS)) {
        if (!setupMode) {
            Serial.print("DEBUG - Page:");
            Serial.print(currentPage);
            Serial.print("(");
            if(currentPage == PAGE_SPORT) {
                Serial.print("SPORT");
            } else if(currentPage == PAGE_TEMP) {
                Serial.print("TEMP");
            } else if(currentPage == PAGE_ELECTRICAL) {
                Serial.print("ELECTRICAL");
            } else {
                Serial.print("CLOCK");
            }
            Serial.print(") Mode:");
            Serial.print(foxVehicleModeToString(vehicleData.mode));
            Serial.print(" RPM:");
            Serial.print(vehicleData.rpm);
            Serial.print(" Volt:");
            Serial.print(vehicleData.voltage, 1);
            Serial.print("V Curr:");
            Serial.print(vehicleData.current, 1);
            Serial.print("A Temp:");
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
