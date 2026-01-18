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

// Performance tracking
unsigned long lastCurrentUpdate = 0;
unsigned long lastSpeedUpdate = 0;
unsigned long lastVoltageUpdate = 0;
unsigned long lastTempUpdate = 0;
unsigned long lastSOCUpdate = 0;

// Sport mode speed tracking
uint16_t lastSportSpeed = 0;
bool speedCrossedThreshold = false;

// Cruise mode blink tracking
bool cruiseBlinkState = false;
unsigned long lastCruiseBlink = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== JAMFOXRS v2.0 (EVENT-DRIVEN HYBRID) ===");
    Serial.println("Optimized for responsiveness & stability");
    Serial.println("Type HELP for command list");
    Serial.println("============================================");
    
    // Inisialisasi array enabled pages
    initEnabledPages();
    
    // Initialize display dengan enhanced recovery
    foxDisplayInitEnhanced();
    
    // Initialize RTC
    if(foxRTCInit()) {
        Serial.println("RTC: OK");
        foxRTCDebugPrint();
    } else {
        Serial.println("RTC: Not found");
    }
    
    // Initialize CAN bus dengan event-driven mode
    if(foxCANInitEnhanced()) {
        Serial.println("CAN: OK (Enhanced Mode)");
    } else {
        Serial.println("CAN: Failed");
    }
    
    // Initialize vehicle module dengan priority system
    foxVehicleInitEnhanced();
    Serial.println("Vehicle module initialized (Priority System)");
    
    // Configure button
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("Button configured");
    
    // Tampilkan konfigurasi page
    printPageConfiguration();
    
    // Performance info
    Serial.println("\n=== PERFORMANCE TARGETS ===");
    Serial.println("Current Display: < 100ms");
    Serial.println("Speed Display: < 100ms");
    Serial.println("Voltage Display: < 250ms");
    Serial.println("Temp Display: < 500ms");
    Serial.println("No Freeze > 500ms");
    Serial.println("===========================");
    
    // Initial display update
    if(foxDisplayIsInitialized()) {
        foxDisplayUpdateSmart(currentPage, true); // Force full initial draw
        Serial.print("Showing page ");
        Serial.println(currentPage);
    } else {
        Serial.println("WARNING: Display not initialized");
    }
    
    Serial.println("\n=== SETUP COMPLETE ===");
    Serial.println("Ready for testing!");
    Serial.println("====================\n");
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
    Serial.println("PERFSTATUS    - Show performance metrics");
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
            foxDisplayUpdateSmart(currentPage, true); // Force full update
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
        foxVehicleClearUnknownList();
        Serial.println("Unknown bytes list cleared");
    }
    else if (command == "SYSTEMSTATUS") {
        displaySystemStatus();
    }
    else if (command == "PERFSTATUS") {
        displayPerformanceStatus();
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
                foxDisplayForceUpdate(); // Force display update
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
            foxDisplayForceUpdate(); // Force display update
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
            foxDisplayForceUpdate(); // Force display update
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
            lastNormalPage = currentPage;
            
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
            foxDisplayUpdateSmart(currentPage, true); // Force full update
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
    Serial.print("Speed: ");
    Serial.print(data.speedKmh);
    Serial.println(" km/h");
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
        Serial.print("Smart Updates: ");
        Serial.println(foxDisplayGetSmartUpdateCount());
        Serial.print("Fallback Updates: ");
        Serial.println(foxDisplayGetFallbackUpdateCount());
    }
    
    // CAN status
    Serial.print("CAN: ");
    Serial.println(foxCANIsInitialized() ? "OK" : "NOT INITIALIZED");
    Serial.print("CAN Messages/sec: ");
    Serial.println(foxCANGetMessageRate());
    
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

void displayPerformanceStatus() {
    Serial.println("\n=== PERFORMANCE METRICS ===");
    
    FoxVehicleData data = foxVehicleGetData();
    unsigned long now = millis();
    
    Serial.println("Update Latencies:");
    Serial.print("  Current: ");
    Serial.print(now - lastCurrentUpdate);
    Serial.println("ms");
    
    Serial.print("  Speed: ");
    Serial.print(now - lastSpeedUpdate);
    Serial.println("ms");
    
    Serial.print("  Voltage: ");
    Serial.print(now - lastVoltageUpdate);
    Serial.println("ms");
    
    Serial.print("  Temperature: ");
    Serial.print(now - lastTempUpdate);
    Serial.println("ms");
    
    Serial.print("  SOC: ");
    Serial.print(now - lastSOCUpdate);
    Serial.println("ms");
    
    Serial.println("\nData Freshness:");
    Serial.print("  Vehicle Data: ");
    Serial.print(now - data.lastUpdate);
    Serial.println("ms");
    
    Serial.print("  Last CAN: ");
    Serial.print(now - lastCANUpdate);
    Serial.println("ms ago");
    
    // Display performance
    if(foxDisplayIsInitialized()) {
        Serial.print("\nDisplay - Smart Update Success: ");
        Serial.print(foxDisplayGetSmartUpdateSuccessRate());
        Serial.println("%");
    }
    
    Serial.println("=============================\n");
}

// =============================================
// FUNGSI-FUNGSI YANG PERLU ADA
// =============================================

// Fungsi untuk force display update
void foxDisplayForceUpdate() {
    if(foxDisplayIsInitialized()) {
        foxDisplayUpdateSmart(currentPage, true);
    }
}

void foxDisplayForceSportUpdate() {
    if(foxDisplayIsInitialized()) {
        foxDisplayUpdateSmart(PAGE_SPORT, true);
    }
}

// Fungsi untuk update performance tracking dari CAN data
void updatePerformanceTracking(uint32_t canId) {
    unsigned long now = millis();
    
    switch(canId) {
        case FOX_CAN_VOLTAGE_CURRENT:
            lastCurrentUpdate = now;
            lastVoltageUpdate = now;
            break;
        case FOX_CAN_MODE_STATUS:
            lastSpeedUpdate = now;
            break;
        case FOX_CAN_TEMP_CTRL_MOT:
        case FOX_CAN_TEMP_BATT_5S:
        case FOX_CAN_TEMP_BATT_SGL:
            lastTempUpdate = now;
            break;
        case FOX_CAN_SOC:
            lastSOCUpdate = now;
            break;
    }
}

// =============================================
// MAIN LOOP - DENGAN PERBAIKAN ELECTRICAL PAGE
// =============================================

void loop() {
    static unsigned long lastUpdate = 0;
    static unsigned long lastDebug = 0;
    static unsigned long lastHealthCheck = 0;
    static unsigned long lastDataDebug = 0;
    static unsigned long lastModeDebug = 0;
    static unsigned long lastCANDebug = 0;
    unsigned long now = millis();
    
    // ========== WATCHDOG TIMER ==========
    static unsigned long lastWatchdogCheck = 0;
    if(now - lastWatchdogCheck > 1000) {
        foxDisplayWatchdogCheck();
        lastWatchdogCheck = now;
    }
    
    // ========== DEBUG CAN MESSAGE RATE ==========
    if(now - lastCANDebug > 2000) {
        FoxVehicleData data = foxVehicleGetData();
        
        Serial.print("[DATA] Current: ");
        Serial.print(data.current, 2);
        Serial.print("A | Voltage: ");
        Serial.print(data.voltage, 1);
        Serial.print("V | Fresh: ");
        Serial.print(now - data.lastUpdate);
        Serial.print("ms | CAN rate: ");
        Serial.print(foxCANGetMessageRate());
        Serial.println(" msg/s");
        
        // Debug untuk electrical page
        if(currentPage == PAGE_ELECTRICAL) {
            Serial.print("[ELECTRICAL] Page active | Last current: ");
            Serial.print(data.current, 2);
            Serial.println("A");
        }
        
        lastCANDebug = now;
    }
    
    // ========== DEBUG MODE INFO ==========
    if(now - lastModeDebug > 3000) {
        FoxVehicleData vehicleData = foxVehicleGetData();
        Serial.print("[DEBUG] Mode: ");
        Serial.print(foxVehicleModeToString(vehicleData.mode));
        Serial.print(" | Page: ");
        Serial.print(currentPage);
        Serial.print(" | Speed: ");
        Serial.print(vehicleData.speedKmh);
        Serial.print(" km/h | Sport: ");
        Serial.println(vehicleData.sportActive ? "YES" : "NO");
        lastModeDebug = now;
    }
    
    // ========== SYSTEM HEALTH CHECK ==========
    if(now - lastHealthCheck > 30000) {
        if(!foxDisplayIsInitialized()) {
            static unsigned long lastDisplayError = 0;
            if(now - lastDisplayError > 60000) {
                Serial.println("[SYSTEM] Display not initialized");
                lastDisplayError = now;
            }
        }
        lastHealthCheck = now;
    }
    
    // Process serial commands
    processSerialCommands();
    
    // Get vehicle data
    FoxVehicleData vehicleData = foxVehicleGetData();
    
    // ========== MODE_UNKNOWN PROTECTION ==========
    if(vehicleData.mode == MODE_UNKNOWN) {
        static unsigned long lastUnknownModeLog = 0;
        if(now - lastUnknownModeLog > 30000) {
            Serial.println("[SYSTEM] MODE_UNKNOWN - Display updates suspended");
            lastUnknownModeLog = now;
        }
        
        // Minimal CAN update
        if(foxDisplayIsInitialized() && (now - lastCANUpdate > 200)) {
            foxCANUpdate();
            lastCANUpdate = now;
        }
        
        delay(10);
        return;
    }
    
    // ========== NORMAL OPERATION ==========
    
    // CAN update dengan fixed interval untuk SEMUA MODE (termasuk charging)
    if(foxDisplayIsInitialized()) {
        // Update CAN dengan fixed interval (50ms = 20Hz)
        if(now - lastCANUpdate > 50) {
            foxCANUpdate();
            lastCANUpdate = now;
        }
    }
    
    // Deteksi mode change
    if(vehicleData.mode != lastMode) {
        lastModeChangeTime = now;
        
        // Log mode change
        static unsigned long lastModeLog = 0;
        if(now - lastModeLog > 1000) {
            Serial.print("Mode changed: ");
            Serial.print(foxVehicleModeToString(lastMode));
            Serial.print(" -> ");
            Serial.println(foxVehicleModeToString(vehicleData.mode));
            lastModeLog = now;
        }
        
        lastMode = vehicleData.mode;
    }
    
    // ========== BUTTON HANDLING ==========
    // Button selalu enabled, termasuk saat charging
    bool buttonEnabled = true;
    
    // Handle button press - SEMUA MODE (termasuk charging)
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
                    foxDisplayUpdateSmart(currentPage, true);
                }
                lastUpdate = now;
            }
        }
        lastButton = btn;
    } else {
        // Reset button state saat disabled
        lastButton = HIGH;
    }
    
    // ========== ELECTRICAL PAGE AUTO-REFRESH ==========
    // Force update electrical page jika ada data BMS baru
    if(currentPage == PAGE_ELECTRICAL && foxDisplayIsInitialized()) {
        FoxVehicleData data = foxVehicleGetData();
        static unsigned long lastElectricalAutoUpdate = 0;
        
        // Check jika data BMS fresh dan valid
        bool bmsDataFresh = (now - data.lastUpdate < 300);
        bool hasBMSData = (data.voltageValid || data.currentValid);
        
        if(bmsDataFresh && hasBMSData) {
            // Auto update setiap 250ms maksimal
            if(now - lastElectricalAutoUpdate > 250) {
                // Trigger smart update dengan threshold kecil
                bool needsUpdate = foxDisplayCheckUpdateNeeded(data);
                if(needsUpdate) {
                    foxDisplayUpdateSmart(PAGE_ELECTRICAL, false);
                    
                    // Debug log
                    static unsigned long lastBMSDisplayLog = 0;
                    if(now - lastBMSDisplayLog > 2000) {
                        Serial.print("[LOOP] Electrical auto-refresh - V:");
                        Serial.print(data.voltage, 1);
                        Serial.print("V I:");
                        Serial.print(data.current, 2);
                        Serial.println("A");
                        lastBMSDisplayLog = now;
                    }
                }
                lastElectricalAutoUpdate = now;
            }
        }
    }
    
    // ========== PAGE SWITCHING (SPORT PAGE AUTO-TRIGGER) ==========
    // Sport page auto-trigger TETAP AKTIF di semua mode
    if(buttonEnabled) {
        bool shouldBeOnSportPage = (vehicleData.sportActive || 
                                   vehicleData.mode == MODE_SPORT_CRUISE ||
                                   vehicleData.mode == MODE_CRUISE);
        
        if (shouldBeOnSportPage) {
            if (currentPage != PAGE_SPORT) {
                lastNormalPage = currentPage;
                currentPage = PAGE_SPORT;
                Serial.println("Auto-switched to SPORT page (9)");
                
                // Force sport page update
                foxDisplayForceSportUpdate();
                
                // Reset sport speed tracking
                lastSportSpeed = vehicleData.speedKmh;
                
                // Immediate smart update
                if(foxDisplayIsInitialized()) {
                    foxDisplayUpdateSmart(currentPage, true);
                    lastUpdate = now;
                }
            }
        } else {
            if (currentPage == PAGE_SPORT) {
                currentPage = lastNormalPage;
                Serial.println("Returned to normal page");
                
                // Force update setelah kembali dari sport
                if(foxDisplayIsInitialized()) {
                    foxDisplayUpdateSmart(currentPage, true);
                    lastUpdate = now;
                }
            }
        }
    }
    
    // ========== SMART DISPLAY UPDATES ==========
    if (setupMode) {
        // Setup mode - simple blinking
        if(now - lastUpdate > 500) {
            bool blinkState = (millis() / 500) % 2 == 0;
            if(foxDisplayIsInitialized()) {
                foxDisplayShowSetupMode(blinkState);
            }
            lastUpdate = now;
        }
        
        if (now - setupModeStart > SETUP_TIMEOUT_MS) {
            setupMode = false;
            Serial.println("Setup mode timeout - auto exit");
            if(foxDisplayIsInitialized()) {
                foxDisplayUpdateSmart(currentPage, true);
            }
        }
    } 
    else {
        // SEMUA MODE (termasuk charging) pakai update normal
        // Normal pages dengan smart updates
        if(foxDisplayIsInitialized() && (now - lastModeChangeTime > 500)) {
            // Check jika data berubah cukup untuk update
            bool needsUpdate = foxDisplayCheckUpdateNeeded(vehicleData);
            
            if(needsUpdate || (now - lastUpdate > 1000)) {
                foxDisplayUpdateSmart(currentPage, false);
                lastUpdate = now;
            }
        }
    }
    
    // ========== PERFORMANCE TRACKING ==========
    // Update timestamps untuk performance monitoring
    if(vehicleData.currentValid && now - lastCurrentUpdate > 1000) {
        lastCurrentUpdate = now;
    }
    if(vehicleData.speedValid && now - lastSpeedUpdate > 1000) {
        lastSpeedUpdate = now;
    }
    if(vehicleData.voltageValid && now - lastVoltageUpdate > 1000) {
        lastVoltageUpdate = now;
    }
    if(vehicleData.tempValid && now - lastTempUpdate > 1000) {
        lastTempUpdate = now;
    }
    if(vehicleData.socValid && now - lastSOCUpdate > 1000) {
        lastSOCUpdate = now;
    }
    
    // ========== DEBUG INFO ==========
    if(showDebugInfo && (now - lastDebug > 10000)) {
        if (!setupMode) {
            Serial.print("DEBUG - Page:");
            Serial.print(currentPage);
            Serial.print(" Mode:");
            Serial.print(foxVehicleModeToString(vehicleData.mode));
            Serial.print(" SOC:");
            Serial.print(vehicleData.soc);
            Serial.print("% Volt:");
            Serial.print(vehicleData.voltage, 1);
            Serial.print("V Curr:");
            Serial.print(vehicleData.current, 1);
            Serial.print("A UpdateLag:");
            Serial.print(now - vehicleData.lastUpdate);
            Serial.println("ms");
        }
        lastDebug = now;
    }
    
    // Small delay untuk stability
    delay(5);
}
