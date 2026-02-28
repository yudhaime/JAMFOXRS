#include "fox_ble.h"
#include "fox_config.h"
#include "fox_vehicle.h"
#include "fox_canbus.h"
#include "fox_serial.h"
#include "fox_display.h"
#include "fox_page.h"
#include "fox_rtc.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// =============================================
// BLE GLOBAL VARIABLES
// =============================================
static BLEServer* pServer = nullptr;
static BLECharacteristic* pCharacteristic = nullptr;
static volatile bool deviceConnected = false;
static bool oldDeviceConnected = false;

// BLE TX State
static char bleTxBuf[2200];
static uint16_t bleTxLen = 0;
static uint16_t bleTxOffset = 0;
static bool bleTxInProgress = false;
static uint32_t lastFastSend = 0;
static uint32_t lastSlowSend = 0;
static bool sendRequested = false;

// Heartbeat counter
static unsigned long heartbeatCounter = 0;

// BLE Activation State
static bool bleActive = false;
static bool bleInitialized = false;
static unsigned long bleActivationStartTime = 0;
static bool activationPending = false;
static unsigned long bleLastConnectionTime = 0;
static bool appModeDisplayed = false;
static bool waitingForConnection = false;
static int previousPage = 1;

// CAN statistics
extern uint32_t getCANMessagesPerSecond();
extern bool displayReady;
extern int currentPage;
extern VehicleData vehicle;

// Display object
extern Adafruit_SSD1306 display;

// =============================================
// FORWARD DECLARATIONS
// =============================================
static void showAppModeDisplay();
static void showBleOffDisplay();
static void hideAppModeDisplay();

// =============================================
// HELPER FUNCTIONS FOR JSON PARSING
// =============================================

// Ekstrak nilai string dari JSON: "key":"value"
static String extractJsonString(const String& json, const String& key) {
    String searchKey = "\"" + key + "\":\"";
    int keyPos = json.indexOf(searchKey);
    if (keyPos == -1) return "";
    
    int startPos = keyPos + searchKey.length();
    int endPos = json.indexOf("\"", startPos);
    if (endPos == -1) return "";
    
    return json.substring(startPos, endPos);
}

// Ekstrak nilai integer dari JSON: "key":123
static int extractJsonInt(const String& json, const String& key, int defaultValue = 0) {
    String searchKey = "\"" + key + "\":";
    int keyPos = json.indexOf(searchKey);
    if (keyPos == -1) return defaultValue;
    
    int startPos = keyPos + searchKey.length();
    int endPos = startPos;
    
    // Cari akhir angka (koma atau kurung tutup)
    while (endPos < json.length()) {
        char c = json.charAt(endPos);
        if (c == ',' || c == '}' || c == ']') break;
        endPos++;
    }
    
    String numStr = json.substring(startPos, endPos);
    numStr.trim();
    return numStr.toInt();
}

// Ekstrak nilai boolean dari JSON: "key":true
static bool extractJsonBool(const String& json, const String& key, bool defaultValue = false) {
    String searchKey = "\"" + key + "\":";
    int keyPos = json.indexOf(searchKey);
    if (keyPos == -1) return defaultValue;
    
    int startPos = keyPos + searchKey.length();
    if (json.substring(startPos, startPos + 4) == "true") return true;
    if (json.substring(startPos, startPos + 5) == "false") return false;
    
    return defaultValue;
}

// Parse string waktu "HH:MM:SS"
static bool parseTimeString(const String& timeStr, int& hour, int& minute, int& second) {
    if (timeStr.length() < 8) return false;
    
    hour = timeStr.substring(0, 2).toInt();
    minute = timeStr.substring(3, 5).toInt();
    second = timeStr.substring(6, 8).toInt();
    
    return (hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60);
}

// Parse string tanggal "DD/MM/YYYY"
static bool parseDateString(const String& dateStr, int& day, int& month, int& year) {
    if (dateStr.length() < 10) return false;
    
    day = dateStr.substring(0, 2).toInt();
    month = dateStr.substring(3, 5).toInt();
    year = dateStr.substring(6, 10).toInt();
    
    return (day >= 1 && day <= 31 && month >= 1 && month <= 12 && year >= 2000 && year <= 2099);
}

// =============================================
// VEHICLE MODE STRING CONVERSION
// =============================================
static const char* getModeString(VehicleMode mode) {
    switch(mode) {
        case MODE_PARK: return "PARK";
        case MODE_STAND: return "STAND";
        case MODE_CHARGING: return "CHARGING";
        case MODE_DRIVE: return "DRIVE";
        case MODE_SPORT: return "SPORT";
        case MODE_REVERSE: return "REVERSE";
        case MODE_BRAKE: return "BRAKE";
        default: return "UNKNOWN";
    }
}

// =============================================
// ADAPTIVE TIMING HELPERS
// =============================================
static VehicleMode getCurrentVehicleMode() {
    uint8_t modeByte = vehicle.lastModeByte;
    
    if (modeByte == 0x00) return MODE_PARK;
    else if (modeByte == 0x61) return MODE_CHARGING;
    else if (modeByte == 0x70) return MODE_DRIVE;
    else if (modeByte == 0x50 || modeByte == 0xF0 || modeByte == 0x30 || modeByte == 0xF8) return MODE_REVERSE;
    else if (modeByte == 0x72 || modeByte == 0xB2) return MODE_BRAKE;
    else if (modeByte == 0xB0) return MODE_SPORT;
    else if (modeByte == 0x78 || modeByte == 0x08) return MODE_STAND;
    
    return MODE_PARK;
}

static uint32_t getFastUpdateInterval() {
    VehicleMode mode = getCurrentVehicleMode();
    switch (mode) {
        case MODE_PARK:
        case MODE_STAND:
            return 500;
        case MODE_CHARGING:
            return 500;
        case MODE_BRAKE:
            return 100;
        default:
            return 150;
    }
}

static uint32_t getSlowUpdateInterval() {
    VehicleMode mode = getCurrentVehicleMode();
    switch (mode) {
        case MODE_PARK:
        case MODE_STAND:
            return 2000;
        case MODE_CHARGING:
            return 2000;
        default:
            return 1000;
    }
}

// =============================================
// DISPLAY APP MODE
// =============================================
static void showAppModeDisplay() {
    if (!displayReady) return;
    
    if (!appModeDisplayed) {
        previousPage = currentPage;
    }
    
    if (safeI2COperation(I2C_MUTEX_TIMEOUT_MS)) {
        display.setFont();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setTextWrap(false);
        
        display.clearDisplay();
        
        display.setFont(&FreeSansBold9pt7b);
        display.setTextSize(1);
        
        int textWidth = strlen(APP_MODE_TEXT) * 10;
        int xPos = (SCREEN_WIDTH - textWidth) / 2;
        
        display.setCursor(xPos, APP_MODE_POS_Y);
        display.print(APP_MODE_TEXT);
        
        display.setFont();
        display.setTextSize(1);
        
        if (deviceConnected) {
            display.setCursor((SCREEN_WIDTH - 50) / 2, 22);
            display.print("connected");
        } else {
            display.setCursor(xPos, 22);
            display.print("ready to connect");
        }
        
        display.display();
        releaseI2C();
        
        appModeDisplayed = true;
        waitingForConnection = !deviceConnected;
        serialPrintfln("[DISPLAY] APP MODE shown");
    }
}

// =============================================
// DISPLAY BLE OFF
// =============================================
static void showBleOffDisplay() {
    if (!displayReady) return;
    
    if (safeI2COperation(I2C_MUTEX_TIMEOUT_MS)) {
        display.setFont();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setTextWrap(false);
        
        display.clearDisplay();
        
        display.setFont(&FreeSansBold9pt7b);
        display.setTextSize(1);
        
        int textWidth = strlen("BLE OFF") * 10;
        int xPos = (SCREEN_WIDTH - textWidth) / 2;
        
        display.setCursor(xPos, APP_MODE_POS_Y);
        display.print("BLE OFF");
        
        display.setFont();
        display.setTextSize(1);
        display.setCursor((SCREEN_WIDTH - 60) / 2, 22);
        display.print("Disconnected");
        
        display.display();
        releaseI2C();
        
        serialPrintfln("[DISPLAY] BLE OFF shown");
        delay(1500);
    }
}

// =============================================
// HIDE APP MODE
// =============================================
static void hideAppModeDisplay() {
    if (!displayReady || !appModeDisplayed) return;
    
    serialPrintfln("[BLE] Switching from APP MODE to Clock Page");
    
    delay(50);
    
    appModeDisplayed = false;
    waitingForConnection = false;
    currentPage = 1;
    
    transitionFromAppModeToClock();
    
    serialPrintfln("[BLE] Switched to Clock Page");
}

// =============================================
// BLE CALLBACKS - DENGAN HANDLER SET TIME
// =============================================
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        deviceConnected = true;
        bleLastConnectionTime = millis();
        waitingForConnection = false;
        serialPrintflnAlways("[BLE] Client connected");
        
        if (displayReady && appModeDisplayed) {
            hideAppModeDisplay();
        }
    }
    
    void onDisconnect(BLEServer* pServer) override {
        deviceConnected = false;
        serialPrintflnAlways("[BLE] Client disconnected");
        
        if (bleActive && displayReady) {
            waitingForConnection = true;
            showAppModeDisplay();
        }
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        String value = pCharacteristic->getValue().c_str();
        if (value.length() > 0) {
            serialPrintfln("[BLE] Received: %s", value.c_str());
            
            // ========== HANDLER SET TIME ==========
            if (value.indexOf("\"cmd\":\"set_time\"") >= 0) {
                
                // Ekstrak data dari JSON
                String timeStr = extractJsonString(value, "time");
                String dateStr = extractJsonString(value, "date");
                int dayOfWeek = extractJsonInt(value, "dayofweek", 1);
                
                serialPrintfln("[BLE] Extracted - time:%s, date:%s, dow:%d", 
                              timeStr.c_str(), dateStr.c_str(), dayOfWeek);
                
                // Parse time
                int hour = 0, minute = 0, second = 0;
                bool timeValid = parseTimeString(timeStr, hour, minute, second);
                
                // Parse date
                int day = 0, month = 0, year = 0;
                bool dateValid = parseDateString(dateStr, day, month, year);
                
                // Validasi dayOfWeek (1-7)
                bool dowValid = (dayOfWeek >= 1 && dayOfWeek <= 7);
                
                String response;
                
                if (timeValid && dateValid && dowValid) {
                    // Set RTC
                    setRTCTime(year, month, day, hour, minute, second, dayOfWeek);
                    serialPrintflnAlways("[BLE] RTC updated: %02d:%02d:%02d %02d/%02d/%04d DOW:%d", 
                                        hour, minute, second, day, month, year, dayOfWeek);
                    
                    // Update display jika di page jam
                    if (currentPage == 1) {
                        safeDisplayUpdate(currentPage);
                    }
                    
                    // Response sukses
                    response = "{\"status\":\"ok\",\"cmd\":\"set_time\"}";
                    
                } else {
                    // Response error
                    response = "{\"status\":\"error\",\"cmd\":\"set_time\",\"message\":\"Invalid data\"}";
                    serialPrintfln("[BLE] Invalid time/date data");
                }
                
                // Kirim response
                pCharacteristic->setValue((uint8_t*)response.c_str(), response.length());
                pCharacteristic->notify();
                serialPrintfln("[BLE] Response sent: %s", response.c_str());
            }
            
            // ========== HANDLER LAINNYA BISA DITAMBAHKAN DI SINI ==========
        }
    }
};

// =============================================
// INIT BLE HARDWARE
// =============================================
static void initBLEHardware() {
    if (bleInitialized) return;
    
    serialPrintflnAlways("[BLE] Initializing hardware...");
    
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEDevice::setMTU(512);
    
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | 
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_WRITE
    );
    
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setCallbacks(new MyCallbacks());
    pCharacteristic->setValue("JAMFOXRS BLE Ready");
    
    pService->start();
    
    bleInitialized = true;
    serialPrintflnAlways("[BLE] Hardware initialized");
}

static void startBLEAdvertising() {
    if (!bleInitialized) return;
    
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    
    BLEDevice::startAdvertising();
    serialPrintflnAlways("[BLE] Advertising started");
    
    waitingForConnection = true;
    showAppModeDisplay();
}

static void stopBLE() {
    if (!bleActive) return;
    
    serialPrintflnAlways("[BLE] Stopping...");
    
    if (deviceConnected) {
        pServer->disconnect(0);
        deviceConnected = false;
    }
    
    BLEDevice::stopAdvertising();
    
    bleActive = false;
    waitingForConnection = false;
    appModeDisplayed = false;
    
    serialPrintflnAlways("[BLE] Stopped");
}

// =============================================
// PUBLIC FUNCTIONS
// =============================================
void activateBLE() {
    if (bleActive) return;
    
    serialPrintflnAlways("[BLE] Activating...");
    
    if (!bleInitialized) {
        initBLEHardware();
    }
    
    startBLEAdvertising();
    
    bleActive = true;
    bleLastConnectionTime = millis();
    activationPending = false;
}

void deactivateBLE() {
    if (!bleActive) return;
    
    serialPrintflnAlways("[BLE] Deactivating...");
    
    if (displayReady) {
        showBleOffDisplay();
    }
    
    stopBLE();
    bleActive = false;
    activationPending = false;
    waitingForConnection = false;
    appModeDisplayed = false;
    
    currentPage = previousPage;
    safeDisplayUpdate(currentPage);
}

bool isBLEActive() {
    return bleActive;
}

bool isInAppMode() {
    return bleActive && !deviceConnected;
}

bool isBLEActivationPending() {
    return activationPending;
}

void setBLEActivationPending(bool pending, unsigned long startTime) {
    activationPending = pending;
    if (pending) {
        bleActivationStartTime = startTime;
    }
}

void resetBLEActivation() {
    activationPending = false;
}

bool isBLEConnected() {
    return deviceConnected && bleActive;
}

// =============================================
// BUILD FAST JSON
// =============================================
static bool buildFastJson() {
    int len = snprintf(bleTxBuf, sizeof(bleTxBuf),
        "{\"r\":%d,\"s\":%d,\"m\":\"%s\",\"v\":%.1f,\"a\":%.1f,\"p\":%.0f,\"sc\":%d,"
        "\"t\":{\"c\":%d,\"m\":%d,\"b\":%d},\"cr\":%lu,\"hb\":%lu,\"type\":\"fast\"}\n",
        vehicle.rpm, vehicle.speed, getModeString(getCurrentVehicleMode()),
        vehicle.batteryVoltage, vehicle.batteryCurrent, 
        vehicle.batteryVoltage * vehicle.batteryCurrent,
        vehicle.batterySOC,
        vehicle.tempCtrl, vehicle.tempMotor, vehicle.tempBatt,
        (unsigned long)getCANMessagesPerSecond(),
        (unsigned long)heartbeatCounter++
    );
    
    if (len < 0 || len >= (int)sizeof(bleTxBuf)) return false;
    bleTxLen = (uint16_t)len;
    return true;
}

// =============================================
// BUILD FULL JSON
// =============================================
static bool buildFullJson() {
    uint16_t minCell = 9999, maxCell = 0;
    for (int i = 0; i < MAX_CELLS; i++) {
        if (vehicle.cellVoltages[i] > 0 && vehicle.cellVoltages[i] < minCell) 
            minCell = vehicle.cellVoltages[i];
        if (vehicle.cellVoltages[i] > maxCell) 
            maxCell = vehicle.cellVoltages[i];
    }
    int cellDelta = (minCell > maxCell) ? 0 : ((int)maxCell - (int)minCell);

    char balanceCells[64];
    int bpos = 0;
    for (int i = 0; i < MAX_CELLS; i++) {
        int byteIndex = i / 8;
        int bitIndex = i % 8;
        bool isBalancing = (vehicle.balanceBits[byteIndex] & (1 << bitIndex)) != 0;
        bpos += snprintf(balanceCells + bpos, sizeof(balanceCells) - bpos, 
                         "%d%s", isBalancing ? 1 : 0, (i < MAX_CELLS-1) ? "," : "");
    }

    char cellsStr[200];
    int cpos = 0;
    for (int i = 0; i < MAX_CELLS; i++) {
        cpos += snprintf(cellsStr + cpos, sizeof(cellsStr) - cpos, 
                         "%u%s", vehicle.cellVoltages[i], (i < MAX_CELLS-1) ? "," : "");
    }

    int len = snprintf(bleTxBuf, sizeof(bleTxBuf),
        "{\"r\":%d,\"s\":%d,\"m\":\"%s\",\"v\":%.1f,\"a\":%.1f,\"p\":%.0f,\"sc\":%d,"
        "\"t\":{\"c\":%d,\"m\":%d,\"b\":%d},\"cells\":[%s],\"cd\":%d,\"cr\":%lu,"
        "\"h\":{\"soh\":%d,\"cyc\":%u,\"rc\":%.1f,\"fc\":%.1f},"
        "\"cvs\":{\"hi\":%u,\"hiC\":%u,\"lo\":%u,\"loC\":%u,\"av\":%u},"
        "\"ts\":{\"max\":%u,\"maxC\":%u,\"min\":%u,\"minC\":%u},"
        "\"b\":{\"md\":%u,\"st\":%u,\"cells\":[%s]},"
        "\"chr\":{\"v\":%.1f,\"a\":%.1f},\"hb\":%lu,\"type\":\"full\"}\n",
        vehicle.rpm, vehicle.speed, getModeString(getCurrentVehicleMode()),
        vehicle.batteryVoltage, vehicle.batteryCurrent, 
        vehicle.batteryVoltage * vehicle.batteryCurrent,
        vehicle.batterySOC,
        vehicle.tempCtrl, vehicle.tempMotor, vehicle.tempBatt,
        cellsStr, cellDelta,
        (unsigned long)getCANMessagesPerSecond(),
        vehicle.batterySOH, vehicle.batteryCycleCount, 
        vehicle.remainingCapacity, vehicle.fullCapacity,
        vehicle.cellHighestVolt, vehicle.cellHighestNum, 
        vehicle.cellLowestVolt, vehicle.cellLowestNum, 
        vehicle.cellAvgVolt,
        vehicle.tempMax, vehicle.tempMaxCell,
        vehicle.tempMin, vehicle.tempMinCell,
        vehicle.balanceMode, vehicle.balanceStatus, balanceCells,
        vehicle.chargerVoltage, vehicle.chargerCurrent,
        (unsigned long)heartbeatCounter++
    );

    if (len < 0 || len >= (int)sizeof(bleTxBuf)) return false;
    bleTxLen = (uint16_t)len;
    return true;
}

// =============================================
// BLE TRANSMISSION
// =============================================
static void startBleTxIfIdle(bool useFast) {
    if (!deviceConnected) return;
    if (bleTxInProgress) return;

    bool ok;
    if (useFast) {
        ok = buildFastJson();
    } else {
        ok = buildFullJson();
    }
    
    if (!ok) return;
    
    bleTxOffset = 0;
    bleTxInProgress = true;
}

static void pumpBleTx() {
    if (!deviceConnected) {
        bleTxInProgress = false;
        bleTxOffset = 0;
        return;
    }
    
    if (!bleTxInProgress) return;
    
    if (bleTxOffset >= bleTxLen) {
        bleTxInProgress = false;
        return;
    }

    uint32_t startUs = micros();
    int sent = 0;

    while (bleTxInProgress &&
           sent < BLE_PUMP_MAX_CHUNKS &&
           (micros() - startUs) < BLE_PUMP_BUDGET_US) {

        int remain = bleTxLen - (int)bleTxOffset;
        if (remain <= 0) {
            bleTxInProgress = false;
            break;
        }

        int chunkLen = min((int)BLE_SAFE_CHUNK, remain);
        const char *p = bleTxBuf + bleTxOffset;

        pCharacteristic->setValue((uint8_t*)p, chunkLen);
        pCharacteristic->notify();

        bleTxOffset += chunkLen;
        sent++;
    }

    if (bleTxOffset >= bleTxLen) bleTxInProgress = false;
}

// =============================================
// MAIN PROCESS FUNCTION
// =============================================
void processBLE() {
    if (!bleActive) return;
    
    if (bleActive && !deviceConnected) {
        unsigned long now = millis();
        if (now - bleLastConnectionTime > (BLE_AUTO_OFF_MINUTES * 60 * 1000UL)) {
            serialPrintflnAlways("[BLE] Auto off after %d minutes idle", BLE_AUTO_OFF_MINUTES);
            deactivateBLE();
            return;
        }
    }
    
    if (!deviceConnected && oldDeviceConnected) {
        delay(200);
        BLEDevice::startAdvertising();
        serialPrintflnAlways("[BLE] Restarting advertising");
        oldDeviceConnected = deviceConnected;
    }
    
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    uint32_t now = millis();
    
    if (!bleTxInProgress) {
        uint32_t fastInterval = getFastUpdateInterval();
        uint32_t slowInterval = getSlowUpdateInterval();
        
        if (now - lastSlowSend >= slowInterval) {
            lastSlowSend = now;
            lastFastSend = now;
            startBleTxIfIdle(false);
        } else if (now - lastFastSend >= fastInterval) {
            lastFastSend = now;
            startBleTxIfIdle(true);
        }
    }

    pumpBleTx();
}

void updateBLEData() {
    if (bleActive && deviceConnected) {
        lastFastSend = 0;
        lastSlowSend = 0;
    }
}
