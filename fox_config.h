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
#define SPLASH_TEXT "WELCOME"
#define SPLASH_DURATION_MS 1500

// Page 1: Clock Configuration
#define CLOCK_TIME_FORMAT "%02d:%02d"
#define CLOCK_DATE_FORMAT "%d %s"
#define CLOCK_YEAR_FORMAT "%04d"

// Page 2: Temperature Configuration
#define TEMP_LABEL_ECU "ECU"
#define TEMP_LABEL_MOTOR "MOTOR"
#define TEMP_LABEL_BATT "BATT"

// Page 3: Sport Mode Configuration
#define SPORT_TEXT "SPORT"
#define CRUISE_TEXT "CRUISE"
#define SPORT_MODE_LABEL "SPORT MODE"
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
#define FOX_CAN_VOLTAGE       0x0A6D0D09UL  // Tegangan baterai
#define FOX_CAN_SOC           0x0A6E0D09UL  // State of Charge (%)
#define FOX_CAN_CURRENT       0x0A6F0D09UL  // Arus (Current)

// =============================================
// KONFIGURASI TAMPILAN
// =============================================

// Font Size Configuration
#define FONT_SIZE_SMALL 1
#define FONT_SIZE_MEDIUM 2
#define FONT_SIZE_LARGE 3

// Timing Configuration
#define UPDATE_INTERVAL_NORMAL_MS 1000
#define UPDATE_INTERVAL_SPORT_MS 10
#define UPDATE_INTERVAL_SETUP_MS 500
#define DEBUG_INTERVAL_MS 10000
#define BLINK_INTERVAL_MS 500

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

// Display Pages Enumeration
enum DisplayPage {
    PAGE_CLOCK = 1,
    PAGE_TEMP = 2,
    PAGE_SPORT = 3
};

#endif
