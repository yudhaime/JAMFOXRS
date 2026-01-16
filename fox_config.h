#ifndef FOX_CONFIG_H
#define FOX_CONFIG_H

// =============================================
// KONFIGURASI UMUM
// =============================================

// Hardware Pin Configuration
#define SDA_PIN 16
#define SCL_PIN 17
#define BUTTON_PIN 25
#define CAN_TX_PIN 22
#define CAN_RX_PIN 21
#define DEBOUNCE_DELAY 50

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_ADDRESS 0x3C

// CAN Bus Configuration
#define CAN_BAUDRATE 250000
#define CAN_MODE 1  // TWAI_MODE_LISTEN_ONLY

// RTC Configuration
#define RTC_I2C_ADDRESS 0x68

// =============================================
// PAGE CONFIGURATION SYSTEM
// =============================================

// Enable/disable pages (true/false)
#define PAGE_CLOCK_ENABLED     true    // Page 1: Clock
#define PAGE_TEMP_ENABLED      true    // Page 2: Temperature  
#define PAGE_ELECTRICAL_ENABLED false  // Page 3: Electrical (currently disabled)

// =============================================
// PAGE 3 (ELECTRICAL) CONFIGURATION
// =============================================

// Sub-components configuration
#define PAGE3_SHOW_VOLTAGE    true      // Tampilkan voltage
#define PAGE3_SHOW_CURRENT    false     // Sembunyikan current (masih bermasalah)
#define PAGE3_SHOW_SOC        false     // Tampilkan SOC (optional)

// Uncomment kode di bawah jika ingin SOC di page 3 nanti:
// #define PAGE3_SHOW_SOC true

// Maximum number of user pages (excluding sport page)
#if PAGE_CLOCK_ENABLED && PAGE_TEMP_ENABLED && PAGE_ELECTRICAL_ENABLED
  #define MAX_USER_PAGES 3
#elif (PAGE_CLOCK_ENABLED && PAGE_TEMP_ENABLED) || \
      (PAGE_CLOCK_ENABLED && PAGE_ELECTRICAL_ENABLED) || \
      (PAGE_TEMP_ENABLED && PAGE_ELECTRICAL_ENABLED)
  #define MAX_USER_PAGES 2
#elif PAGE_CLOCK_ENABLED || PAGE_TEMP_ENABLED || PAGE_ELECTRICAL_ENABLED
  #define MAX_USER_PAGES 1
#else
  #define MAX_USER_PAGES 0
#endif

// =============================================
// KONFIGURASI VEHICLE
// =============================================

// Vehicle Mode Byte Values
#define MODE_BYTE_PARK 0x00
#define MODE_BYTE_DRIVE 0x70
#define MODE_BYTE_SPORT 0xB0
#define MODE_BYTE_CRUISE 0x74
#define MODE_BYTE_SPORT_CRUISE 0xB4
#define MODE_BYTE_CUTOFF_1 0xB2
#define MODE_BYTE_CUTOFF_2 0x72
#define MODE_BYTE_STANDBY_1 0x78
#define MODE_BYTE_STANDBY_2 0x08

// Speed Configuration
#define SPEED_TRIGGER_SPORT_PAGE 80  // Speed untuk trigger mode page sport (km/h)
#define RPM_TO_KMPH_FACTOR 0.1033    // RPM * 0.1033 = km/h
#define DEFAULT_TEMP 25
#define DATA_TIMEOUT_MS 10000

// =============================================
// KONFIGURASI DISPLAY TEXT
// =============================================

// Splash Screen Configuration
#define SPLASH_TEXT "WELCOME"       // Ganti tulisan saat kendaraan pertama kali dinyalakan
#define SPLASH_DURATION_MS 1500     // Durasi tulisan awal satuan ms atau milidetik

// Page 1: Clock Configuration
#define CLOCK_TIME_FORMAT "%02d:%02d"
#define CLOCK_DATE_FORMAT "%d %s"
#define CLOCK_YEAR_FORMAT "%04d"

// Page 2: Temperature Configuration
#define TEMP_LABEL_ECU "ECU"       // Label tulisan di atas suhu controller
#define TEMP_LABEL_MOTOR "MOTOR"   // Label tulisan di atas suhu bldc dinamo motor
#define TEMP_LABEL_BATT "BATT"     // Label tulisan di atas suhu baterai

// Page 3: Electrical Configuration
#define ELECTRICAL_LABEL_VOLT "VOLT"
#define ELECTRICAL_LABEL_CURR "CURR"

// Page 9: Sport Mode Configuration
#define SPORT_TEXT "SPORT"         // Tulisan ketika masuk mode Sport
#define CRUISE_TEXT "CRUISE"       // Tulisan ketika Cruise Control aktif
#define SPORT_MODE_LABEL "SPORT MODE"  // Tulisan kecil ketika kecepatan mulai melewati batas
#define KMH_TEXT "km/h"
#define RPM_TEXT "rpm"

// Setup Mode Configuration
#define SETUP_TEXT "SETUP"
#define SETUP_TIMEOUT_MS 30000

// =============================================
// KONFIGURASI CAN BUS ID
// =============================================

// CAN IDs untuk Fox EV
#define FOX_CAN_MODE_STATUS   0x0A010810UL  // Mode kendaraan + RPM + suhu
#define FOX_CAN_TEMP_CTRL_MOT 0x0A010A10UL  // Speed & suhu
#define FOX_CAN_TEMP_BATT_5S  0x0E6C0D09UL  // Suhu baterai 5 cell
#define FOX_CAN_TEMP_BATT_SGL 0x0A010A11UL  // Suhu baterai single

// CAN IDs untuk data performa (dokumen Votol)
#define FOX_CAN_VOLTAGE_CURRENT 0x0A6D0D09UL  // Tegangan (Byte 0-1) dan Arus (Byte 2-3)
#define FOX_CAN_SOC           0x0A6E0D09UL  // Persentase baterai atau State of Charge (%)
#define FOX_CAN_BMS_INFO      0x0A740D09UL  // Informasi BMS (Type & Merk) - IGNORE

// =============================================
// KONFIGURASI TAMPILAN
// =============================================

// Font Size Configuration
#define FONT_SIZE_SMALL 1
#define FONT_SIZE_MEDIUM 2
#define FONT_SIZE_LARGE 3

// Timing Configuration
#define UPDATE_INTERVAL_NORMAL_MS 1000
#define UPDATE_INTERVAL_ELECTRICAL_MS 500   // Update lebih cepat untuk page electrical
#define UPDATE_INTERVAL_SPORT_MS 10
#define UPDATE_INTERVAL_SETUP_MS 500
#define DEBUG_INTERVAL_MS 10000
#define BLINK_INTERVAL_MS 500

// BMS Configuration
#define BMS_DEADZONE_CURRENT 0.1          // Deadzone 0.1A
#define BMS_UPDATE_THRESHOLD_VOLTAGE 0.1  // 0.1V perubahan
#define BMS_UPDATE_THRESHOLD_CURRENT 0.5  // 0.5A perubahan

// Position Configuration
#define POS_TOP 0
#define POS_MIDDLE 12
#define POS_BOTTOM 25

// =============================================
// ENUM & STRUCTURE DEFINITIONS
// =============================================

// Vehicle Mode Enumeration
enum FoxVehicleMode {
    MODE_UNKNOWN = 0,
    MODE_PARK,
    MODE_DRIVE,
    MODE_SPORT,
    MODE_CUTOFF,
    MODE_STANDBY,
    MODE_REVERSE,
    MODE_NEUTRAL,
    MODE_CRUISE,
    MODE_SPORT_CRUISE
};

// Display Pages Enumeration - Decimal Jump System
enum DisplayPage {
    PAGE_CLOCK = 1,      // User page 1: Jam & Tanggal
    PAGE_TEMP = 2,       // User page 2: Suhu
    PAGE_ELECTRICAL = 3, // User page 3: Voltage & Current
    PAGE_SPORT = 9       // Hidden page: Sport Mode (auto-trigger only)
};

#endif
