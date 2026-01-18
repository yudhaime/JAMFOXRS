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
#define PAGE_ELECTRICAL_ENABLED true  // Page 3: Electrical (currently disabled)
// Page 9 (Sport) is always enabled for automatic mode switching

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
#define MODE_BYTE_STANDBY_3 0xB8
#define MODE_BYTE_CHARGING_1 0x61
#define MODE_BYTE_CHARGING_2 0xA1
#define MODE_BYTE_CHARGING_3 0xA9
#define MODE_BYTE_CHARGING_4 0x69  // Charging dengan side stand
#define MODE_BYTE_REVERSE 0x50
#define MODE_BYTE_NEUTRAL 0x40

// Macro untuk deteksi semua charging modes
#define IS_CHARGING_MODE(b) ((b) == MODE_BYTE_CHARGING_1 || \
                             (b) == MODE_BYTE_CHARGING_2 || \
                             (b) == MODE_BYTE_CHARGING_3 || \
                             (b) == MODE_BYTE_CHARGING_4)

// Macro untuk deteksi semua known modes
#define IS_KNOWN_MODE(b) ((b) == MODE_BYTE_PARK || \
                          (b) == MODE_BYTE_DRIVE || \
                          (b) == MODE_BYTE_SPORT || \
                          (b) == MODE_BYTE_CRUISE || \
                          (b) == MODE_BYTE_SPORT_CRUISE || \
                          (b) == MODE_BYTE_CUTOFF_1 || \
                          (b) == MODE_BYTE_CUTOFF_2 || \
                          (b) == MODE_BYTE_STANDBY_1 || \
                          (b) == MODE_BYTE_STANDBY_2 || \
                          (b) == MODE_BYTE_STANDBY_3 || \
                          (b) == MODE_BYTE_REVERSE || \
                          (b) == MODE_BYTE_NEUTRAL || \
                          IS_CHARGING_MODE(b))

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

// Charging Mode Configuration
#define CHARGING_TEXT "CHARGING"

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
#define FOX_CAN_VOLTAGE       0x0A6D0D09UL  // Tegangan baterai
#define FOX_CAN_SOC           0x0A6E0D09UL  // Persentase baterai atau State of Charge (%)
//#define FOX_CAN_CURRENT       0x0A6F0D09UL  // Arus (Current) //salah jadi saya hapus karena voltase dan current ada di satu message
#define FOX_CAN_VOLTAGE_CURRENT 0x0A6D0D09UL  // Gabungan voltage dan current

// CAN IDs untuk charger (filter)
#define FOX_CAN_CHARGER_1 0x1810D0F3UL  // Contoh ID charger 1
#define FOX_CAN_CHARGER_2 0x1811D0F3UL  // Contoh ID charger 2
#define FOX_CAN_BMS_INFO  0x0A010C10UL  // BMS info message

// =============================================
// OPTIMIZATION CONFIGURATION
// =============================================

// Smart Display Update Thresholds
#define CURRENT_UPDATE_THRESHOLD 0.05     // PERBAIKI: 0.05A change triggers update (dari 0.1)
#define VOLTAGE_UPDATE_THRESHOLD 0.1      // PERBAIKI: 0.1V change triggers update (dari 0.5)
#define SPEED_UPDATE_THRESHOLD 1          // 1 km/h change triggers update
#define TEMP_UPDATE_THRESHOLD 1           // 1°C change triggers update
#define SOC_UPDATE_THRESHOLD 1            // 1% change triggers update

// Update Intervals (Event-Driven + Fallback)
#define UPDATE_INTERVAL_CRITICAL_MS 50    // PERBAIKI: Current, Speed, RPM (dari 100ms)
#define UPDATE_INTERVAL_HIGH_MS 100       // PERBAIKI: Voltage (dari 250ms)
#define UPDATE_INTERVAL_MEDIUM_MS 500     // Temperatures
#define UPDATE_INTERVAL_LOW_MS 1000       // Clock, SOC
#define UPDATE_INTERVAL_SPORT_MS 50       // Sport page
#define UPDATE_INTERVAL_SETUP_MS 500      // Setup mode blink
#define UPDATE_INTERVAL_CHARGING_MS 5000  // Charging mode

// Watchdog Timeouts
#define DISPLAY_WATCHDOG_TIMEOUT_MS 500   // Display update timeout
#define I2C_WATCHDOG_TIMEOUT_MS 300       // I2C communication timeout
#define CAN_SILENCE_TIMEOUT_MS 2000       // CAN bus silence timeout

// Debug Configuration
#define DEBUG_INTERVAL_MS 10000
#define BLINK_INTERVAL_MS 500

// Font Size Configuration
#define FONT_SIZE_SMALL 1
#define FONT_SIZE_MEDIUM 2
#define FONT_SIZE_LARGE 3

// BMS Configuration
#define BMS_DEADZONE_CURRENT 0.1          // Deadzone 0.1A
#define BMS_UPDATE_THRESHOLD_VOLTAGE 0.1  // 0.1V perubahan
#define BMS_UPDATE_THRESHOLD_CURRENT 0.5  // 0.5A perubahan

// Position Configuration
#define POS_TOP 0
#define POS_MIDDLE 12
#define POS_BOTTOM 25

// Data freshness timeout
#define DATA_FRESH_TIMEOUT_MS 5000

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
    MODE_SPORT_CRUISE,
    MODE_CHARGING
};

// Display Pages Enumeration - Decimal Jump System
enum DisplayPage {
    PAGE_CLOCK = 1,      // User page 1: Jam & Tanggal
    PAGE_TEMP = 2,       // User page 2: Suhu
    PAGE_ELECTRICAL = 3, // User page 3: Voltage & Current
    PAGE_SPORT = 9       // Hidden page: Sport Mode (auto-trigger only)
};

// Display Update Priority (for smart updates)
enum DisplayPriority {
    PRIORITY_CRITICAL = 0,   // Current, Speed, RPM (< 100ms)
    PRIORITY_HIGH = 1,       // Voltage (< 250ms)
    PRIORITY_MEDIUM = 2,     // Temperatures (< 500ms)
    PRIORITY_LOW = 3         // Clock, SOC, Static text (< 1000ms)
};

// Display Zones for partial updates
enum DisplayZone {
    ZONE_NONE = 0,
    ZONE_CLOCK_TIME,
    ZONE_CLOCK_DATE,
    ZONE_TEMP_ECU,
    ZONE_TEMP_MOTOR,
    ZONE_TEMP_BATT,
    ZONE_VOLTAGE,
    ZONE_CURRENT,
    ZONE_SPEED,
    ZONE_RPM,
    ZONE_SOC,
    ZONE_MODE_TEXT
};

#endif
