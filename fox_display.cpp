#include "fox_display.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "fox_config.h"
#include "fox_rtc.h"
#include "fox_canbus.h"
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

// FreeRTOS untuk ESP32
#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

// =============================================
// GLOBAL VARIABLES
// =============================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool displayInitialized = false;
bool displayReady = false;

// I2C Mutex
#ifdef ESP32
extern SemaphoreHandle_t i2cMutex;
#endif

// =============================================
// HELPER FUNCTION: GET SPLASH FONT
// =============================================
const GFXfont* getSplashFont() {
    switch(SPLASH_FONT_SIZE) {
        case 0:
            return NULL;  // Default font
        case 1:
            return &FreeSansBold9pt7b;  // Font sedang
        case 2:
            return &FreeSansBold12pt7b; // Font besar
        default:
            return &FreeSansBold9pt7b;  // Default ke sedang
    }
}

int getSplashFontWidth() {
    switch(SPLASH_FONT_SIZE) {
        case 0:
            return FONT_WIDTH_DEFAULT;
        case 1:
            return FONT_WIDTH_9PT;
        case 2:
            return FONT_WIDTH_12PT;
        default:
            return FONT_WIDTH_9PT;
    }
}

// =============================================
// DISPLAY INITIALIZATION - SPLASH WITH CONFIGURABLE FONT
// =============================================
void initDisplay() {
    // Coba beberapa kali
    for(int attempt = 1; attempt <= 5; attempt++) {
        // Re-init I2C setiap attempt
        Wire.end();
        delay(10);
        Wire.begin(SDA_PIN, SCL_PIN);
        Wire.setClock(100000);
        delay(50);
        
        // Coba init display
        if(display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
            displayInitialized = true;
            displayReady = true;
            
            // Reset display state ke default
            display.setFont();
            display.setTextSize(1);
            
            // =========================================
            // SPLASH SCREEN DENGAN FONT CONFIGURABLE
            // =========================================
            display.clearDisplay();
            display.setTextColor(SSD1306_WHITE);
            
            // Dapatkan font yang dikonfigurasi
            const GFXfont* splashFont = getSplashFont();
            if(splashFont != NULL) {
                display.setFont(splashFont);
            }
            
            // Hitung posisi tengah jika X = 0 (auto-center)
            int xPos = SPLASH_POS_X;
            int yPos = SPLASH_POS_Y;
            
            if(SPLASH_POS_X == 0) {
                // Hitung width berdasarkan font yang dipilih
                int textWidth = strlen(SPLASH_TEXT) * getSplashFontWidth();
                xPos = (SCREEN_WIDTH - textWidth) / 2;
                
                // Adjust Y position berdasarkan font size
                if(SPLASH_FONT_SIZE == 2) { // Font besar (12pt)
                    yPos = 24; // Turunkan sedikit
                } else if(SPLASH_FONT_SIZE == 1) { // Font sedang (9pt)
                    yPos = 22; // Posisi optimal untuk 9pt
                } else { // Default font
                    yPos = 16;
                }
            }
            
            display.setCursor(xPos, yPos);
            display.print(SPLASH_TEXT);
            display.display();
            
            delay(SPLASH_DURATION_MS);
            
            // Reset ke default font sebelum operasi normal
            display.setFont();
            display.setTextSize(1);
            display.clearDisplay();
            display.display();
            
            return;
        }
        
        delay(200);
    }
    
    displayInitialized = false;
    displayReady = false;
}

// =============================================
// I2C SAFETY FUNCTIONS
// =============================================
bool safeI2COperation(uint32_t timeoutMs) {
    if(!displayInitialized) return false;
    
#ifdef ESP32
    if(i2cMutex != NULL) {
        return (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
    }
#endif
    
    delay(1);
    return true;
}

void releaseI2C() {
#ifdef ESP32
    if(i2cMutex != NULL) {
        xSemaphoreGive(i2cMutex);
    }
#endif
}

// =============================================
// DISPLAY STATE RESET FUNCTION
// =============================================
void resetDisplayState() {
    if(!displayInitialized) return;
    
    // Reset semua setting display ke default state
    display.setFont();                     // Default font (6x8)
    display.setTextSize(1);                // Text size 1 (default)
    display.setTextColor(SSD1306_WHITE);   // White text
    display.setTextWrap(false);            // No text wrapping
    display.setCursor(0, 0);               // Reset cursor position
}

// =============================================
// SPECIAL MODE DISPLAY FUNCTION - HANYA SPORT & CRUISE
// =============================================
void updateSpecialModeDisplay(uint8_t modeType, bool blinkState) {
    if(!displayInitialized) return;
    
    // Reset state display sebelum menggambar
    resetDisplayState();
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    const char* displayText = "";
    
    // Tentukan teks berdasarkan mode (HANYA SPORT & CRUISE)
    switch(modeType) {
        case 1: // Sport Mode
            displayText = SPORT_TEXT;
            break;
        case 2: // Cruise Mode
            if(!blinkState) {
                // Mode blink, jika blinkState false, kosongkan display
                display.display();
                return;
            }
            displayText = CRUISE_TEXT;
            break;
        // TIDAK ADA CASE UNTUK CHARGING MODE LAGI
        default:
            // Untuk mode lain (termasuk charging), tampilkan kosong dan return
            display.display();
            return;
    }
    
    // GUNAKAN FONT BESAR UNTUK SPECIAL MODE (12pt)
    display.setFont(&FreeSansBold12pt7b);
    
    // Hitung posisi tengah jika X = 0 (auto-center)
    int xPos = SPECIAL_MODE_POS_X;
    int yPos = SPECIAL_MODE_POS_Y;
    
    if(SPECIAL_MODE_POS_X == 0) {
        // Auto-center calculation dengan font 12pt
        int textWidth = strlen(displayText) * FONT_WIDTH_12PT;
        xPos = (SCREEN_WIDTH - textWidth) / 2;
    }
    
    display.setCursor(xPos, yPos);
    display.print(displayText);
    
    // TIDAK ADA INDIKATOR KECIL DI POJOK KANAN
    display.display();
}

// =============================================
// THREAD-SAFE SPECIAL MODE DISPLAY
// =============================================
bool safeShowSpecialMode(uint8_t modeType, bool blinkState) {
    if(!displayInitialized || !displayReady) return false;
    
    // Jika modeType bukan sport atau cruise, langsung return false
    if(modeType != 1 && modeType != 2) return false;
    
    if(!safeI2COperation(I2C_MUTEX_TIMEOUT_MS)) {
        return false;
    }
    
    updateSpecialModeDisplay(modeType, blinkState);
    releaseI2C();
    return true;
}

// =============================================
// DISPLAY FUNCTIONS
// =============================================
bool safeDisplayUpdate(int page) {
    if(!displayInitialized || !displayReady) {
        return false;
    }
    
    if(!safeI2COperation(I2C_MUTEX_TIMEOUT_MS)) {
        return false;
    }
    
    updateDisplay(page);
    releaseI2C();
    return true;
}

void resetDisplay() {
    if(!displayInitialized) return;
    
    if(safeI2COperation(I2C_MUTEX_TIMEOUT_MS)) {
        resetDisplayState(); // Reset state sebelum clear
        display.clearDisplay();
        display.display();
        releaseI2C();
    }
}

// =============================================
// UPDATE DISPLAY FUNCTION - DENGAN PAGE 3 RESPONSIF
// =============================================
void updateDisplay(int page) {
    if(!displayInitialized) {
        return;
    }
    
    // RESET DISPLAY STATE SETIAP KALI UPDATE
    resetDisplayState();
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    if(page == 1) {
        // Page 1: Clock
        RTCDateTime dt = getRTC();
        
        // Jam besar (kiri) - font besar untuk jam
        display.setFont(&FreeSansBold18pt7b);
        display.setCursor(CLOCK_TIME_POS_X, CLOCK_TIME_POS_Y);
        display.printf("%02d:%02d", dt.hour, dt.minute);
        
        // Reset font untuk teks kecil
        display.setFont();
        display.setTextSize(1);
        
        // Validasi index array
        int hariIndex = dt.dayOfWeek - 1;
        if(hariIndex < 0) hariIndex = 0;
        if(hariIndex > 6) hariIndex = 6;
        
        int bulanIndex = dt.month - 1;
        if(bulanIndex < 0) bulanIndex = 0;
        if(bulanIndex > 11) bulanIndex = 11;
        
        // Gunakan array dari fox_config.h
        // Hari
        display.setCursor(CLOCK_DAY_POS_X, CLOCK_DAY_POS_Y);
        display.print(DAY_NAMES[hariIndex]);
        
        // Tanggal & bulan
        display.setCursor(CLOCK_DATE_POS_X, CLOCK_DATE_POS_Y);
        display.printf("%d %s", dt.day, MONTH_NAMES[bulanIndex]);
        
        // Tahun
        display.setCursor(CLOCK_YEAR_POS_X, CLOCK_YEAR_POS_Y);
        display.printf("%04d", dt.year);
        
    } else if(page == 2) {
        // Page 2: Temperature
        display.setTextSize(1);
        
        // Labels
        display.setCursor(TEMP_LABEL_ECU_POS_X, TEMP_LABEL_ECU_POS_Y);
        display.print(TEMP_LABEL_ECU);
        display.setCursor(TEMP_LABEL_MOTOR_POS_X, TEMP_LABEL_MOTOR_POS_Y);
        display.print(TEMP_LABEL_MOTOR);
        display.setCursor(TEMP_LABEL_BATT_POS_X, TEMP_LABEL_BATT_POS_Y);
        display.print(TEMP_LABEL_BATT);
        
        // Values (besar)
        display.setTextSize(2);
        display.setCursor(TEMP_VALUE_ECU_POS_X, TEMP_VALUE_ECU_POS_Y);
        display.print(getTempCtrl());
        display.setCursor(TEMP_VALUE_MOTOR_POS_X, TEMP_VALUE_MOTOR_POS_Y);
        display.print(getTempMotor());
        display.setCursor(TEMP_VALUE_BATT_POS_X, TEMP_VALUE_BATT_POS_Y);
        display.print(getTempBatt());
        
    } else if(page == 3) {
        // ===== PAGE 3: VOLTAGE & CURRENT - RESPONSIF VERSION =====
        display.setTextSize(1);
        
        // Ambil data BMS
        float voltage = getBatteryVoltage();
        float current = getBatteryCurrent();
        
        // Labels (tetap pakai label kecil)
        display.setCursor(BMS_LABEL_VOLTAGE_POS_X, BMS_LABEL_VOLTAGE_POS_Y);
        display.print(BMS_LABEL_VOLTAGE);
        display.setCursor(BMS_LABEL_CURRENT_POS_X, BMS_LABEL_CURRENT_POS_Y);
        display.print(BMS_LABEL_CURRENT);
        
        // Values (besar) - TANPA SATUAN
        display.setTextSize(2);
        
        // Voltage (kiri)
        display.setCursor(BMS_VALUE_VOLTAGE_POS_X, BMS_VALUE_VOLTAGE_POS_Y);
        
        // DEADZONE HANDLING DI DISPLAY LEVEL (0.1V)
        if(fabs(voltage) < 0.1f) {
            display.print("0");  // Tampilkan "0" bukan "0.0"
        } else {
            display.printf("%.1f", voltage);  // Format 1 decimal
        }
        
        // Current (kanan) - dengan penanganan tanda
        display.setCursor(BMS_VALUE_CURRENT_POS_X, BMS_VALUE_CURRENT_POS_Y);
        
        // DEADZONE HANDLING DI DISPLAY LEVEL (0.1A)
        if(fabs(current) < 0.1f) {
            // Current < 0.1A, tampilkan "0"
            display.setCursor(BMS_VALUE_CURRENT_POS_X + 20, BMS_VALUE_CURRENT_POS_Y);
            display.print("0");
        } else if(current > 0) {
            // Positive (charging)
            display.printf("%.1f", current);
        } else {
            // Negative (discharging) - tampilkan nilai absolut
            display.printf("%.1f", -current);
            
            // Tambahkan tanda minus kecil
            display.setTextSize(1);
            display.setCursor(BMS_VALUE_CURRENT_POS_X - 7, BMS_VALUE_CURRENT_POS_Y + 5);
            display.print("-");
            display.setTextSize(2);
        }
        
        // TIDAK ADA SATUAN V/A, TIDAK ADA SOC, TIDAK ADA CHARGING INDICATOR
        
    } else if(page == PAGE_SPECIAL_ID) {
        // Page khusus akan ditangani di main loop
        // Untuk charging, sekarang tetap tampilkan page normal
        resetDisplayState(); // Reset state
        display.display();
    } else {
        // Fallback ke page 1
        updateDisplay(1);
        return;
    }
    
    display.display();
}

// =============================================
// SETUP MODE DISPLAY
// =============================================
void showSetupMode(bool blinkState) {
    if(!displayInitialized) return;
    
    // Reset state display
    resetDisplayState();
    
    display.clearDisplay();
    
    // Gunakan font 9pt untuk setup mode (lebih pas)
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(SSD1306_WHITE);
    
    // Hitung posisi tengah jika X = 0 (auto-center)
    int xPos = SETUP_MODE_POS_X;
    int yPos = SETUP_MODE_POS_Y;
    
    if(SETUP_MODE_POS_X == 0) {
        int textWidth = strlen(SETUP_TEXT) * FONT_WIDTH_9PT;
        xPos = (SCREEN_WIDTH - textWidth) / 2;
        yPos = 22; // Optimal untuk font 9pt
    }
    
    if(blinkState) {
        display.setCursor(xPos, yPos);
        display.print(SETUP_TEXT);
    }
    
    display.display();
}

// =============================================
// UTILITY FUNCTIONS
// =============================================
bool isDisplayInitialized() {
    return displayInitialized;
}
