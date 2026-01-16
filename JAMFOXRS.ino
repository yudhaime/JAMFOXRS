#include "fox_config.h"
#include "fox_display.h"
#include "fox_canbus.h"
#include "fox_vehicle.h"
#include "fox_rtc.h"

// Global variables
int currentPage = PAGE_CLOCK;
int lastNormalPage = PAGE_CLOCK;
bool lastButton = HIGH;
bool setupMode = false;
bool showDebugInfo = false;
unsigned long setupModeStart = 0;

// Variables for I2C error handling
unsigned long lastModeChangeTime = 0;
FoxVehicleMode lastMode = MODE_UNKNOWN;

// Array untuk page yang enabled
int enabledPages[4]; // Maks 3 user pages + 1 sport
int enabledPageCount = 0;
int currentPageIndex = 0;

// Charging mode tracking
bool wasCharging = false;
unsigned long lastChargingDisplayUpdate = 0;
unsigned long lastChargingLog = 0;
unsigned long lastCANUpdate = 0;

// System protection tracking
unsigned long lastSystemCheck = 0;
unsigned long lastDisplayErrorLog = 0;
int displayErrorCount = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== JAMFOXRSBETA START (ENHANCED PROTECTION) ===");
    Serial.println("Type HELP for command list");
    Serial.println("==================================================");
    
    // Inisialisasi array enabled pages
    initEnabledPages();
    
    // Initialize display
    foxDisplayInit();
    
    // Initialize RTC
    if(foxRTCInit()) {
        Serial.println("RTC: OK");
        foxRTCDebugPrint();
    } else {
        Serial.println("RTC: Not found");
    }
    
    // Initialize CAN bus
    if(foxCANInit()) {
        Serial.println("CAN: OK");
    } else {
        Serial.println("CAN: Failed");
    }
    
    // Initialize vehicle module
    foxVehicleInit();
    Serial.println("Vehicle module initialized");
    
    // Configure button
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("Button configured");
    
    // Tampilkan konfigurasi page
    printPageConfiguration();
    
    // Initial display update hanya jika display initialized
    if(foxDisplayIsInitialized()) {
        foxDisplayUpdate(currentPage);
        Serial.print("Showing page ");
        Serial.println(currentPage);
    } else {
        Serial.println("WARNING: Display not initialized, skipping first update");
    }
    
    Serial.println("=== SETUP COMPLETE ===");
    Serial.println("System running with enhanced protection");
    Serial.println("========================================");
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
    Serial.println("I2CSTATUS     - Show I2C error statistics");
    Serial.println("CLEARUNKNOWN  - Clear unknown bytes list");
    Serial.println("SYSTEMSTATUS  - Show system health status");
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
        if(foxDisplayIsInitialized()) {
            foxDisplayUpdate(currentPage);
        }
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
    else if (command == "I2CSTATUS") {
        Serial.print("I2C Error Count: ");
        Serial.println(getI2CErrorCount());
        Serial.print("Last I2C Error: ");
        unsigned long lastError = getLastI2CErrorTime();
        if(lastError > 0) {
            Serial.print((millis() - lastError) / 1000);
            Serial.println(" seconds ago");
        } else {
            Serial.println("Never");
        }
    }
    else if (command == "CLEARUNKNOWN") {
        // Panggil fungsi clear unknown list jika ada
        // foxVehicleClearUnknownList();
        Serial.println("Feature not implemented in this version");
    }
    else if (command == "SYSTEMSTATUS") {
        displaySystemStatus();
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
            if(foxDisplayIsInitialized()) {
                foxDisplayUpdate(currentPage);
            }
        }
    } else {
        Serial.println("ERROR - Day must be 1-7");
    }
}

void handleTimeCommand(String command) {
    String timeStr = command.substring(5);
    if (foxRTCSetTimeFromString(timeStr)) {
        Serial.println("OK - Time updated");
        if(foxDisplayIsInitialized()) {
            foxDisplayUpdate(currentPage);
        }
    } else {
        Serial.println("ERROR - Format: TIME HH:MM:SS or HH:MM");
    }
}

void handleDateCommand(String command) {
    String dateStr = command.substring(5);
    if (foxRTCSetDateFromString(dateStr)) {
        Serial.println("OK - Date updated");
        if(foxDisplayIsInitialized()) {
            foxDisplayUpdate(currentPage);
        }
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
        if(foxDisplayIsInitialized()) {
            foxDisplayUpdate(currentPage);
        }
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
    
    // Voltage dan Current
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
    
    Serial.print("Speed: ");
    Serial.print(data.speedKmh);
    Serial.println(" km/h");
    
    Serial.print("Data Fresh: ");
    Serial.println(foxVehicleDataIsFresh() ? "YES" : "NO");
    Serial.println("===================");
}

void displaySystemStatus() {
    Serial.println("\n=== SYSTEM STATUS ===");
    
    // Display status
    Serial.print("Display: ");
    Serial.println(foxDisplayIsInitialized() ? "OK" : "NOT INITIALIZED");
    
    if(foxDisplayIsInitialized()) {
        Serial.print("I2C Errors: ");
        Serial.println(getI2CErrorCount());
    }
    
    // CAN status
    Serial.print("CAN: ");
    Serial.println(foxCANIsInitialized() ? "OK" : "NOT INITIALIZED");
    
    // RTC status
    Serial.print("RTC: ");
    Serial.println(foxRTCIsRunning() ? "OK" : "NOT RUNNING");
    
    // Vehicle data
    FoxVehicleData data = foxVehicleGetData();
    Serial.print("Vehicle Mode: ");
    Serial.println(foxVehicleModeToString(data.mode));
    
    Serial.print("Display Page: ");
    Serial.println(currentPage);
    
    Serial.print("Uptime: ");
    Serial.print(millis() / 1000);
    Serial.println(" seconds");
    
    Serial.println("====================\n");
}

void loop() {
    static unsigned long lastUpdate = 0;
    static unsigned long lastDebug = 0;
    static bool blinkState = false;
    static unsigned long lastHealthCheck = 0;
    unsigned long now = millis();
    
    // Process serial commands
    processSerialCommands();
    
    // Get vehicle data
    FoxVehicleData vehicleData = foxVehicleGetData();
    
    // ========== SYSTEM HEALTH CHECK ==========
    if(now - lastHealthCheck > 30000) { // Setiap 30 detik
        if(!foxDisplayIsInitialized()) {
            static unsigned long lastDisplayError = 0;
            if(now - lastDisplayError > 60000) {
                Serial.println("[SYSTEM] Display not initialized");
                lastDisplayError = now;
            }
        }
        lastHealthCheck = now;
    }
    
    // ========== MODE_UNKNOWN PROTECTION ==========
    if(vehicleData.mode == MODE_UNKNOWN) {
        // Mode unknown, skip semua display logic tapi system tetap jalan
        
        static unsigned long lastUnknownModeLog = 0;
        if(now - lastUnknownModeLog > 30000) {
            Serial.println("[SYSTEM] MODE_UNKNOWN - Display updates suspended");
            lastUnknownModeLog = now;
        }
        
        // CAN update dengan throttling tinggi
        if(foxDisplayIsInitialized() && (now - lastCANUpdate > 200)) { // 5Hz max
            foxCANUpdate();
            lastCANUpdate = now;
        }
        
        delay(10);
        return; // SKIP SEMUA DISPLAY LOGIC
    }
    
    // ========== NORMAL OPERATION ==========
    
    // CAN update dengan throttling berdasarkan mode
    if(foxDisplayIsInitialized()) {
        unsigned long canInterval = (vehicleData.mode == MODE_CHARGING) ? 200 : 50;
        
        if(now - lastCANUpdate > canInterval) {
            foxCANUpdate();
            lastCANUpdate = now;
        }
    }
    
    // Deteksi mode change
    if(vehicleData.mode != lastMode) {
        lastModeChangeTime = now;
        
        // Handle charging mode transition
        if(vehicleData.mode == MODE_CHARGING && !wasCharging) {
            Serial.println("=== CHARGING MODE ===");
            Serial.println("Button disabled, simple display enabled");
            wasCharging = true;
            lastChargingDisplayUpdate = now;
            lastChargingLog = now;
        } else if(vehicleData.mode != MODE_CHARGING && wasCharging) {
            Serial.println("=== NORMAL MODE ===");
            Serial.println("Button enabled");
            wasCharging = false;
        }
        
        lastMode = vehicleData.mode;
    }
    
    // ========== BUTTON HANDLING ==========
    bool buttonEnabled = (vehicleData.mode != MODE_CHARGING);
    
    // Handle button press - HANYA jika tidak charging
    if (buttonEnabled && !setupMode && currentPage != PAGE_SPORT && MAX_USER_PAGES > 0) {
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
                
                if(foxDisplayIsInitialized()) {
                    foxDisplayUpdate(currentPage);
                }
                lastUpdate = now;
            }
        }
        lastButton = btn;
    } else {
        // Reset button state saat disabled
        lastButton = HIGH;
    }
    
    // ========== PAGE SWITCHING (SPORT PAGE AUTO-TRIGGER) ==========
    if(buttonEnabled) {
        bool shouldBeOnSportPage = (vehicleData.sportActive || 
                                   vehicleData.mode == MODE_SPORT_CRUISE ||
                                   vehicleData.mode == MODE_CRUISE);
        
        if (shouldBeOnSportPage) {
            if (currentPage != PAGE_SPORT) {
                lastNormalPage = currentPage;
                currentPage = PAGE_SPORT;
                Serial.println("Auto-switched to SPORT page (9)");
                foxDisplayForceSportUpdate();
                
                // Immediate update untuk sport page
                if(foxDisplayIsInitialized() && (now - lastModeChangeTime > 100)) {
                    foxDisplayUpdate(currentPage);
                    lastUpdate = now;
                }
            }
        } else {
            if (currentPage == PAGE_SPORT) {
                currentPage = lastNormalPage;
                Serial.println("Returned to normal page");
                
                // Update display setelah kembali dari sport
                if(foxDisplayIsInitialized() && (now - lastModeChangeTime > 100)) {
                    foxDisplayUpdate(currentPage);
                    lastUpdate = now;
                }
            }
        }
    }
    
    // ========== UPDATE LOGIC ==========
    if (setupMode) {
        // Setup mode
        if(now - lastUpdate > UPDATE_INTERVAL_SETUP_MS) {
            blinkState = !blinkState;
            if(foxDisplayIsInitialized()) {
                foxDisplayShowSetupMode(blinkState);
            }
            lastUpdate = now;
        }
        
        if (now - setupModeStart > SETUP_TIMEOUT_MS) {
            setupMode = false;
            Serial.println("Setup mode timeout - auto exit");
            if(foxDisplayIsInitialized()) {
                foxDisplayUpdate(currentPage);
            }
        }
    } 
    else if (vehicleData.mode == MODE_CHARGING) {
        // ========== CHARGING MODE ==========
        if(now - lastChargingDisplayUpdate > UPDATE_INTERVAL_CHARGING_MS) {
            if(foxDisplayIsInitialized()) {
                foxDisplayUpdate(99); // Charging display page
            }
            lastChargingDisplayUpdate = now;
            lastUpdate = now;
            
            // Minimal logging (60 detik sekali)
            if(now - lastChargingLog > 60000) {
                RTCDateTime dt = foxRTCGetDateTime();
                Serial.print("[CHARGING] ");
                Serial.print(dt.hour);
                Serial.print(":");
                if(dt.minute < 10) Serial.print("0");
                Serial.print(dt.minute);
                Serial.print(" - ");
                Serial.print(vehicleData.soc);
                Serial.print("% ");
                Serial.print(vehicleData.voltage, 1);
                Serial.println("V");
                lastChargingLog = now;
            }
        }
    }
    else if (currentPage == PAGE_SPORT) {
        // Sport page update dengan throttle
        bool isCruiseMode = (vehicleData.mode == MODE_CRUISE || 
                            vehicleData.mode == MODE_SPORT_CRUISE);
        
        unsigned long interval = isCruiseMode ? 200 : UPDATE_INTERVAL_SPORT_MS;
        
        if(foxDisplayIsInitialized() && (now - lastModeChangeTime > 100) && (now - lastUpdate > interval)) {
            foxDisplayUpdate(currentPage);
            lastUpdate = now;
        }
    }
    else {
        // Normal pages
        if(foxDisplayIsInitialized() && (now - lastModeChangeTime > 500) && (now - lastUpdate > UPDATE_INTERVAL_NORMAL_MS)) {
            foxDisplayUpdate(currentPage);
            lastUpdate = now;
        }
    }
    
    // ========== DEBUG INFO ==========
    if(showDebugInfo && (now - lastDebug > DEBUG_INTERVAL_MS) && vehicleData.mode != MODE_CHARGING) {
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
            Serial.print(" SOC:");
            Serial.print(vehicleData.soc);
            Serial.print("% Volt:");
            Serial.print(vehicleData.voltage, 1);
            Serial.print("V TimeSinceModeChange:");
            Serial.print(now - lastModeChangeTime);
            Serial.println("ms");
        }
        lastDebug = now;
    }
    
    // Small delay untuk stability
    delay(10);
}
