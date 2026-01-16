#include "fox_display.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <math.h>
#include "fox_config.h"
#include "fox_rtc.h"
#include "fox_vehicle.h"
#include <Fonts/FreeSansBold18pt7b.h>

// Deklarasi global
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool displayInitialized = false;

// Variabel untuk tracking perubahan
uint8_t lastDisplayedSpeed = 0;
uint16_t lastDisplayedRPM = 0;
bool sportNeedsUpdate = false;
unsigned long lastBlinkTime = 0;
bool blinkState = true;

// I2C error tracking
int i2cErrorCount = 0;
unsigned long lastI2CErrorTime = 0;
unsigned long lastI2CCheck = 0;

// Helper function
void showPageDisabled(int page) {
    display.setTextSize(FONT_SIZE_MEDIUM);
    display.setCursor(10, 12);
    display.print("PAGE ");
    display.print(page);
    display.print(" DISABLED");
}

// Fungsi I2C recovery
void recoverI2C() {
    Serial.println("=== I2C RECOVERY START ===");
    
    // Stop I2C
    Wire.end();
    
    // Reset pins ke high-impedance
    pinMode(SDA_PIN, INPUT);
    pinMode(SCL_PIN, INPUT);
    delay(100); // Delay lebih lama untuk stabilisasi
    
    // Generate clock pulses untuk clear stuck devices
    pinMode(SCL_PIN, OUTPUT);
    Serial.print("Sending clock pulses...");
    for(int i = 0; i < 20; i++) {
        digitalWrite(SCL_PIN, LOW);
        delayMicroseconds(10);
        digitalWrite(SCL_PIN, HIGH);
        delayMicroseconds(10);
    }
    Serial.println("Done");
    
    // Reinitialize dengan pull-up
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, INPUT_PULLUP);
    delay(100);
    
    Wire.begin(SDA_PIN, SCL_PIN);
    
    // Gunakan speed lebih rendah jika charging
    FoxVehicleData data = foxVehicleGetData();
    if(data.mode == MODE_CHARGING) {
        Wire.setClock(50000); // 50kHz saat charging
    } else {
        Wire.setClock(100000); // 100kHz normal
    }
    
    // Test koneksi
    byte error;
    bool oledFound = false;
    uint8_t oledAddresses[] = {0x3C, 0x3D};
    
    for(int i = 0; i < 2; i++) {
        Wire.beginTransmission(oledAddresses[i]);
        error = Wire.endTransmission();
        
        if(error == 0) {
            Serial.print("OLED found at 0x");
            Serial.println(oledAddresses[i], HEX);
            
            // Reinitialize display
            if(display.begin(SSD1306_SWITCHCAPVCC, oledAddresses[i])) {
                displayInitialized = true;
                oledFound = true;
                Serial.println("OLED reinitialized successfully");
                break;
            }
        }
    }
    
    if(!oledFound) {
        Serial.println("OLED NOT FOUND after recovery");
        displayInitialized = false;
    }
    
    Serial.println("=== I2C RECOVERY END ===");
}

// Fungsi check I2C health
bool checkI2CHealth() {
    FoxVehicleData data = foxVehicleGetData();
    
    // Saat charging, cek lebih jarang
    unsigned long checkInterval = (data.mode == MODE_CHARGING) ? 10000 : 1000;
    
    if(millis() - lastI2CCheck < checkInterval) {
        return displayInitialized;
    }
    
    lastI2CCheck = millis();
    
    if(!displayInitialized) {
        return false;
    }
    
    // Test koneksi ke OLED
    Wire.beginTransmission(OLED_ADDRESS);
    byte error = Wire.endTransmission();
    
    if(error != 0) {
        i2cErrorCount++;
        lastI2CErrorTime = millis();
        
        // Log error dengan throttling
        static unsigned long lastErrorLog = 0;
        if(millis() - lastErrorLog > 5000) {
            Serial.print("[I2C] Error #");
            Serial.print(i2cErrorCount);
            Serial.print(" Code: ");
            Serial.println(error);
            lastErrorLog = millis();
        }
        
        // Auto-recovery jika banyak error
        if(i2cErrorCount >= 5) {
            Serial.println("[I2C] Multiple errors, attempting recovery...");
            recoverI2C();
            i2cErrorCount = 0;
        }
        
        return false;
    }
    
    // Reset error count jika berhasil
    if(i2cErrorCount > 0) {
        i2cErrorCount = 0;
    }
    
    return true;
}

// Fungsi untuk akses dari JAMFOXRSBETA.ino
int getI2CErrorCount() {
    return i2cErrorCount;
}

unsigned long getLastI2CErrorTime() {
    return lastI2CErrorTime;
}

void foxDisplayInit() {
    Serial.println("Initializing OLED...");
    
    // Reset I2C bus dulu untuk hindari NACK error
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000); // 100kHz
    delay(100);
    
    // Coba multiple address untuk OLED
    uint8_t oledAddresses[] = {0x3C, 0x3D}; // Common OLED addresses
    bool oledFound = false;
    byte error;
    
    for(int i = 0; i < 2; i++) {
        Wire.beginTransmission(oledAddresses[i]);
        error = Wire.endTransmission();
        
        if(error == 0) {
            Serial.print("OLED found at 0x");
            Serial.println(oledAddresses[i], HEX);
            
            if(!display.begin(SSD1306_SWITCHCAPVCC, oledAddresses[i])) {
                Serial.println("OLED init failed at this address");
            } else {
                Serial.println("OLED initialized successfully!");
                displayInitialized = true;
                oledFound = true;
                break;
            }
        }
        delay(50);
    }
    
    if(!oledFound) {
        Serial.println("OLED NOT FOUND at any address!");
        displayInitialized = false;
        return;
    }
    
    // ========== SPLASH SCREEN ==========
    display.clearDisplay();
    display.setTextSize(FONT_SIZE_MEDIUM);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 10);
    display.print(SPLASH_TEXT);
    display.display();
    
    delay(SPLASH_DURATION_MS);
    
    // ========== LANGSUNG TAMPILKAN DEFAULT PAGE ==========
    // Reset ke font default
    display.setFont();
    display.setTextSize(FONT_SIZE_SMALL);
    
    // Tampilkan page clock langsung setelah splash
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    // Ambil waktu dari RTC
    RTCDateTime dt = foxRTCGetDateTime();
    
    // Jam besar
    display.setFont(&FreeSansBold18pt7b);
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), CLOCK_TIME_FORMAT, dt.hour, dt.minute);
    display.setCursor(0, 28);
    display.print(timeStr);
    
    // Info tanggal
    display.setFont();
    display.setTextSize(FONT_SIZE_SMALL);
    
    const char* hari[] = {"MINGGU", "SENIN", "SELASA", "RABU", "KAMIS", "JUMAT", "SABTU"};
    const char* bulan[] = {"JAN", "FEB", "MAR", "APR", "MEI", "JUN", 
                          "JUL", "AGU", "SEP", "OKT", "NOV", "DES"};
    
    int hariIndex = dt.dayOfWeek - 1;
    if(hariIndex < 0) hariIndex = 0;
    if(hariIndex > 6) hariIndex = 6;
    
    int bulanIndex = dt.month - 1;
    if(bulanIndex < 0) bulanIndex = 0;
    if(bulanIndex > 11) bulanIndex = 11;
    
    int rightCol = 88;
    
    display.setCursor(rightCol, 5);
    display.print(hari[hariIndex]);
    
    char tanggalBulan[10];
    snprintf(tanggalBulan, sizeof(tanggalBulan), CLOCK_DATE_FORMAT, dt.day, bulan[bulanIndex]);
    display.setCursor(rightCol, 15);
    display.print(tanggalBulan);
    
    char yearStr[5];
    snprintf(yearStr, sizeof(yearStr), CLOCK_YEAR_FORMAT, dt.year);
    display.setCursor(rightCol, 25);
    display.print(yearStr);
    
    display.display();
    
    Serial.println("Display showing default clock page");
}

bool foxDisplayIsInitialized() {
    return displayInitialized;
}

// ========== MODIFIKASI UTAMA: foxDisplayUpdate() ==========
void foxDisplayUpdate(int page) {
    // Skip update jika display tidak initialized
    if(!displayInitialized) {
        return;
    }
    
    // Cek I2C health sebelum update (kecuali untuk charging)
    FoxVehicleData vehicleData = foxVehicleGetData();
    
    // ========== HANDLE CHARGING MODE ==========
    if(vehicleData.mode == MODE_CHARGING) {
        // Gunakan page khusus untuk charging (99)
        if(page == 99) {
            static uint8_t lastDisplayedSOC = 0;
            static float lastDisplayedVoltage = 0;
            static unsigned long lastDisplayUpdate = 0;
            static String lastTimeStr = "";
            
            // Ambil waktu sekarang
            RTCDateTime dt = foxRTCGetDateTime();
            String currentTimeStr = String(dt.hour) + ":" + (dt.minute < 10 ? "0" : "") + String(dt.minute);
            
            // Update hanya jika ada perubahan signifikan atau setiap 30 detik
            bool socChanged = (abs((int)vehicleData.soc - (int)lastDisplayedSOC) > 1);
            bool voltChanged = (fabs(vehicleData.voltage - lastDisplayedVoltage) > 0.5);
            bool timeChanged = (currentTimeStr != lastTimeStr);
            bool timeout = (millis() - lastDisplayUpdate > 30000);
            
            if(!socChanged && !voltChanged && !timeChanged && !timeout) {
                return; // Skip update
            }
            
            // SIMPLE DISPLAY - minimal I2C traffic
            display.clearDisplay();
            display.setTextSize(2); // Font besar
            
            // CHARGING di tengah atas
            display.setCursor(15, 0);
            display.print("CHARGING");
            
            // JAM di kiri bawah (persentase)
            display.setTextSize(1);
            display.setCursor(48, 20);
            display.print(currentTimeStr);
            
            display.display();
            
            // Update trackers
            lastDisplayedSOC = vehicleData.soc;
            lastDisplayedVoltage = vehicleData.voltage;
            lastTimeStr = currentTimeStr;
            lastDisplayUpdate = millis();
            return;
        }
        // Untuk page lain, skip saat charging
        return;
    }
    
    // Cek I2C health untuk mode normal
    if(!checkI2CHealth()) {
        return;
    }
    
    // Untuk SPORT page: cek apakah data berubah
    if(page == PAGE_SPORT) {
        bool speedChanged = (vehicleData.speedKmh != lastDisplayedSpeed);
        bool rpmChanged = (vehicleData.rpm != lastDisplayedRPM);
        
        // Skip update jika tidak ada perubahan
        if(!speedChanged && !rpmChanged && !sportNeedsUpdate) {
            return;
        }
        
        // Update nilai terakhir
        lastDisplayedSpeed = vehicleData.speedKmh;
        lastDisplayedRPM = vehicleData.rpm;
        sportNeedsUpdate = false;
    }
    
    // Attempt display update dengan retry mechanism
    for(int retry = 0; retry < 2; retry++) {
        if(retry > 0) {
            delay(50);
        }
        
        try {
            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);
            display.setFont();
            display.setTextSize(FONT_SIZE_SMALL);
            
            // Switch berdasarkan page
            if(page == PAGE_CLOCK) {
                #if PAGE_CLOCK_ENABLED
                    displayPageClock();
                #else
                    showPageDisabled(1);
                #endif
            }
            else if(page == PAGE_TEMP) {
                #if PAGE_TEMP_ENABLED
                    displayPageTemperature(vehicleData);
                #else
                    showPageDisabled(2);
                #endif
            }
            else if(page == PAGE_ELECTRICAL) {
                displayPageElectrical(vehicleData);
            }
            else if(page == PAGE_SPORT) {
                displayPageSport(vehicleData);
            }
            else {
                showPageDisabled(page);
            }
            
            display.display();
            return; // Success
            
        } catch(...) {
            if(retry == 0) {
                recoverI2C();
            }
        }
    }
}

// Fungsi display page (sama seperti sebelumnya)
void displayPageClock() {
    RTCDateTime dt = foxRTCGetDateTime();
    
    // Jam besar menggunakan konfigurasi format
    display.setFont(&FreeSansBold18pt7b);
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), CLOCK_TIME_FORMAT, dt.hour, dt.minute);
    display.setCursor(0, 28);
    display.print(timeStr);
    
    // Info tanggal menggunakan konfigurasi
    display.setFont();
    display.setTextSize(FONT_SIZE_SMALL);
    
    const char* hari[] = {"MINGGU", "SENIN", "SELASA", "RABU", "KAMIS", "JUMAT", "SABTU"};
    const char* bulan[] = {"JAN", "FEB", "MAR", "APR", "MEI", "JUN", 
                          "JUL", "AGU", "SEP", "OKT", "NOV", "DES"};
    
    int hariIndex = dt.dayOfWeek - 1;
    if(hariIndex < 0) hariIndex = 0;
    if(hariIndex > 6) hariIndex = 6;
    
    int bulanIndex = dt.month - 1;
    if(bulanIndex < 0) bulanIndex = 0;
    if(bulanIndex > 11) bulanIndex = 11;
    
    int rightCol = 88;
    
    display.setCursor(rightCol, 5);
    display.print(hari[hariIndex]);
    
    char tanggalBulan[10];
    snprintf(tanggalBulan, sizeof(tanggalBulan), CLOCK_DATE_FORMAT, dt.day, bulan[bulanIndex]);
    display.setCursor(rightCol, 15);
    display.print(tanggalBulan);
    
    char yearStr[5];
    snprintf(yearStr, sizeof(yearStr), CLOCK_YEAR_FORMAT, dt.year);
    display.setCursor(rightCol, 25);
    display.print(yearStr);
}

void displayPageTemperature(const FoxVehicleData& vehicleData) {
    // Label menggunakan konfigurasi
    display.setTextSize(FONT_SIZE_SMALL);
    display.setCursor(0, 4);
    display.print(TEMP_LABEL_ECU);
    display.setCursor(43, 4);
    display.print(TEMP_LABEL_MOTOR);
    display.setCursor(86, 4);
    display.print(TEMP_LABEL_BATT);
    
    // Angka suhu
    display.setTextSize(FONT_SIZE_MEDIUM);
    display.setCursor(0, 16);
    display.print(vehicleData.tempController);
    display.setCursor(43, 16);
    display.print(vehicleData.tempMotor);
    display.setCursor(86, 16);
    display.print(vehicleData.tempBattery);
}

void displayPageElectrical(const FoxVehicleData& vehicleData) {
    #if PAGE_ELECTRICAL_ENABLED
        // Layout 3 kolom sama seperti page suhu
        
        // Baris 1: Labels
        display.setTextSize(FONT_SIZE_SMALL);
        display.setCursor(0, 4);
        display.print(ELECTRICAL_LABEL_VOLT);
        display.setCursor(86, 4);
        display.print(ELECTRICAL_LABEL_CURR);
        
        // Baris 2: Values
        display.setTextSize(FONT_SIZE_MEDIUM);
        
        // =============================================
        // VOLTAGE (KIRI)
        // =============================================
        float voltage = vehicleData.voltage;
        
        if(voltage < 0.1) {
            // Jika voltage 0, tampilkan "0" bukan "0.0"
            display.setCursor(0, 16);
            display.print("  0");
        } else if(voltage < 10.0) {
            // Voltage 1 digit: " 7.5"
            char voltStr[6];
            snprintf(voltStr, sizeof(voltStr), "%4.1f", voltage);
            display.setCursor(0, 16);
            display.print(voltStr);
        } else if(voltage < 100.0) {
            // Voltage 2 digit: "75.5"
            char voltStr[6];
            snprintf(voltStr, sizeof(voltStr), "%4.1f", voltage);
            display.setCursor(0, 16);
            display.print(voltStr);
        } else {
            // Voltage 3 digit: "100"
            display.setCursor(0, 16);
            display.print((int)voltage);
        }
        
        // =============================================
        // CURRENT (KANAN)
        // =============================================
        float current = vehicleData.current;
        float absCurrent = fabs(current);
        
        if(absCurrent < 0.1) {
            // Current ~0, tampilkan "0" bukan "0.0"
            display.setCursor(86, 16);
            display.print("  0");
        } else {
            if(current >= 0) {
                // POSITIVE CURRENT
                if(absCurrent < 10.0) {
                    // 1 digit positif: "+2.5"
                    char currStr[6];
                    snprintf(currStr, sizeof(currStr), "+%3.1f", current);
                    display.setCursor(76, 16);
                    display.print(currStr);
                } else if(absCurrent < 100.0) {
                    // 2 digit positif: "+25"
                    display.setCursor(71, 16);
                    display.print("+");
                    display.print((int)current);
                } else {
                    // 3 digit positif: "+100"
                    display.setCursor(66, 16);
                    display.print("+");
                    display.print((int)current);
                }
            } else {
                // NEGATIVE CURRENT
                if(absCurrent < 10.0) {
                    // 1 digit negatif: "-2.5"
                    char currStr[6];
                    snprintf(currStr, sizeof(currStr), "%4.1f", current);
                    display.setCursor(76, 16);
                    display.print(currStr);
                } else if(absCurrent < 100.0) {
                    // 2 digit negatif: "-25"
                    display.setCursor(71, 16);
                    display.print((int)current);
                } else {
                    // 3 digit negatif: "-100"
                    display.setCursor(66, 16);
                    display.print((int)current);
                }
            }
        }
    #else
        // Jika page disabled, tampilkan pesan
        display.setTextSize(FONT_SIZE_MEDIUM);
        display.setCursor(10, 12);
        display.print("PAGE 3 DISABLED");
    #endif
}

void displayPageSport(const FoxVehicleData& vehicleData) {
    // Deteksi mode
    bool isCruiseMode = (vehicleData.mode == MODE_CRUISE || 
                        vehicleData.mode == MODE_SPORT_CRUISE);
    bool isSportMode = (vehicleData.mode == MODE_SPORT || 
                       vehicleData.mode == MODE_SPORT_CRUISE);
    
    // PRIORITAS 1: CRUISE MODE
    if(isCruiseMode) {
        updateBlinkState();
        displayCruiseMode();
    }
    // PRIORITAS 2: SPORT MODE
    else if(isSportMode) {
        if(vehicleData.speedKmh < SPEED_TRIGGER_SPORT_PAGE) {
            displaySportModeLowSpeed();
        } else {
            displaySportModeHighSpeed(vehicleData.speedKmh);
        }
    }
}

void updateBlinkState() {
    if(millis() - lastBlinkTime > BLINK_INTERVAL_MS) {
        blinkState = !blinkState;
        lastBlinkTime = millis();
    }
}

void displayCruiseMode() {
    // "CRUISE" besar di tengah (blinking) menggunakan konfigurasi
    display.setTextSize(FONT_SIZE_LARGE);
    int textWidth = strlen(CRUISE_TEXT) * 18;
    int xPos = (SCREEN_WIDTH - textWidth) / 2;
    int yPos = 4;
    
    display.setCursor(xPos, yPos);
    if(blinkState) {
        display.print(CRUISE_TEXT);
    }
}

void displaySportModeLowSpeed() {
    // "SPORT" besar di tengah menggunakan konfigurasi
    display.setTextSize(FONT_SIZE_LARGE);
    int textWidth = strlen(SPORT_TEXT) * 18;
    int xPos = (SCREEN_WIDTH - textWidth) / 2;
    int yPos = 4;
    
    display.setCursor(xPos, yPos);
    display.print(SPORT_TEXT);
}

void displaySportModeHighSpeed(uint8_t speedKmh) {
    // "SPORT MODE" kecil di tengah atas menggunakan konfigurasi
    display.setTextSize(FONT_SIZE_SMALL);
    int sportWidth = strlen(SPORT_MODE_LABEL) * 6;
    int sportX = (SCREEN_WIDTH - sportWidth) / 2;
    display.setCursor(sportX, POS_TOP);
    display.print(SPORT_MODE_LABEL);
    
    // Angka speed besar
    display.setTextSize(FONT_SIZE_LARGE);
    char speedStr[4];
    snprintf(speedStr, sizeof(speedStr), "%3d", speedKmh);
    display.setCursor(0, 10);
    display.print(speedStr);
    
    // "km/h" di kanan angka menggunakan konfigurasi
    display.setTextSize(FONT_SIZE_SMALL);
    int speedWidth = 54;
    int kmhX = speedWidth + 4;
    int kmhY = 18;
    display.setCursor(kmhX, kmhY);
    display.print(KMH_TEXT);
}

void foxDisplayForceSportUpdate() {
    sportNeedsUpdate = true;
}

void foxDisplayShowSetupMode(bool blinkState) {
    if(!displayInitialized) return;
    
    display.clearDisplay();
    display.setTextSize(FONT_SIZE_MEDIUM);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 10);
    if (blinkState) {
        display.print(SETUP_TEXT);
    }
    display.display();
}
