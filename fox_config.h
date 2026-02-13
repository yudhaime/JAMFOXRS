#ifndef CONFIG_H
#define CONFIG_H

// =============================================
// DOIT DevKit V1 SPECIFIC CONFIG
// =============================================

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define SDA_PIN 16
#define SCL_PIN 17
#define OLED_ADDRESS 0x3C

// =============================================
// ANIMATION CONFIGURATION
// =============================================
#define ANIMATION_INTERVAL_MS 30         // 30ms = 33 FPS (smooth)
#define ANIMATION_SMOOTHNESS 0.25f       // 25% per frame (natural)
#define VOLTAGE_CHANGE_THRESHOLD 0.05f   // 0.05V threshold
#define CURRENT_CHANGE_THRESHOLD 0.1f    // 0.1A threshold untuk animasi
#define POWER_CHANGE_THRESHOLD 10.0f     // 10W threshold

// =============================================
// DUAL-CORE FREE RTOS CONFIGURATION
// =============================================
// Task priorities (higher number = higher priority)
#define TASK_PRIORITY_CAN       2
#define TASK_PRIORITY_DISPLAY   2
#define TASK_PRIORITY_SERIAL    1

// Stack sizes
#define STACK_SIZE_CAN          4096
#define STACK_SIZE_DISPLAY      4096
#define STACK_SIZE_SERIAL       3072

// Core allocation
#define CORE_CAN                0   // Core 0: Dedicated to CAN reading
#define CORE_DISPLAY            1   // Core 1: Display, UI, Serial

// CAN Task timing
#define CAN_TASK_UPDATE_MS      5
#define CAN_PROCESS_LIMIT       10

// =============================================
// CAN UPDATE CONFIGURATION
// =============================================
#define CAN_UPDATE_RATE_DRIVING_MS 20
#define CAN_UPDATE_RATE_CHARGING_MS 50
#define CURRENT_DISPLAY_DEADZONE 0.2f
#define CHARGER_TIMEOUT_MS 5000

// =============================================
// DATA FRESHNESS CONFIGURATION
// =============================================
#define DATA_FRESH_TIMEOUT_NORMAL_MS    10000   // 10 detik untuk normal mode
#define DATA_FRESH_TIMEOUT_CHARGING_MS  30000   // 30 detik untuk charging mode

// =============================================
// DISPLAY UPDATE RATE CONFIGURATION
// =============================================
#define DISPLAY_UPDATE_RATE_PAGE1_MS  10000  // 10 detik
#define DISPLAY_UPDATE_RATE_PAGE2_MS  3000   // 3 detik  
#define DISPLAY_UPDATE_RATE_PAGE3_MS  500    // 500ms
#define DISPLAY_UPDATE_RATE_PAGE4_MS  500    // 500ms

// =============================================
// SPLASH SCREEN CONFIGURATION
// =============================================
#define SPLASH_TEXT "POLYTRON"
#define SPLASH_DURATION_MS 1500
#define SPLASH_FONT_SIZE 1
#define SPLASH_POS_X 0
#define SPLASH_POS_Y 20

// Font width approximations
#define FONT_WIDTH_DEFAULT  6
#define FONT_WIDTH_9PT     18
#define FONT_WIDTH_12PT    24

// =============================================
// PAGE CONFIGURATION - FLEXIBLE ORDER
// =============================================
// Define page enable flags
#define PAGE_1_ENABLE true    // Clock
#define PAGE_2_ENABLE true    // Temperature  
#define PAGE_3_ENABLE true    // BMS Data
#define PAGE_4_ENABLE true    // Power Display

// Define page order (1-4 in desired order)
const uint8_t PAGE_ORDER[] = {1, 2, 3, 4};  // Default: normal order
const uint8_t PAGE_ORDER_COUNT = 4;         // Number of pages in order array

// =============================================
// CHARGING MODE CONFIGURATION
// =============================================
#define CHARGING_MODE_ENABLED false

// NEW: Enable/disable Charging Display Page
#define CHARGING_PAGE_ENABLED false   // Set false untuk disable charging page
                                     // Set true untuk enable charging page

// Charging-specific timing
#define CHARGING_CAN_UPDATE_MS 100
#define CHARGING_DISPLAY_UPDATE_MS 5000  // 5 detik update untuk charging page
#define CHARGING_CURRENT_DEADZONE 1.0f

// Charger timeout
#define CHARGER_TIMEOUT_MS 3000

// =============================================
// DAY & MONTH CONFIGURATION
// =============================================
static const char* DAY_NAMES[] = {
    "MINGGU", "SENIN", "SELASA", "RABU", 
    "KAMIS", "JUMAT", "SABTU"
};

static const char* MONTH_NAMES[] = {
    "JAN", "FEB", "MAR", "APR", "MEI", "JUN", 
    "JUL", "AGU", "SEP", "OKT", "NOV", "DES"
};

// =============================================
// BUTTON CONFIGURATION
// =============================================
#define BUTTON_PIN 25
#define DEBOUNCE_DELAY 20
#define BUTTON_COOLDOWN_MS 200
#define BUTTON_LONG_PRESS_MS 800

// =============================================
// CAN BUS CONFIGURATION
// =============================================
#define CAN_TX_PIN 22
#define CAN_RX_PIN 21
#define CAN_BAUDRATE 250000

// ID CAN MESSAGE
#define ID_CTRL_MOTOR       0x0A010810UL
#define ID_BATT_5S          0x0E6C0D09UL
#define ID_BATT_SINGLE      0x0A010A10UL
#define ID_VOLTAGE_CURRENT  0x0A6D0D09UL

// CHARGER SPAM MESSAGE IDs
#define ORI_CHARGER_SPAM_ID 0x10261041UL
#define CHARGER_DATA_ID_1   0x1810D0F3UL
#define CHARGER_DATA_ID_2   0x1811D0F3UL
#define BMS_CHARGING_FLAG   0x0AB40D09UL

// =============================================
// BMS CONFIGURATION
// =============================================
#define BMS_VOLTAGE_RESOLUTION 0.1f
#define BMS_CURRENT_RESOLUTION 0.1f
#define BMS_SIGN_BIT 7

// =============================================
// TEMPERATURE CONFIGURATION
// =============================================
#define DEFAULT_TEMP 25
#define DATA_TIMEOUT 10000

// =============================================
// RTC CONFIGURATION
// =============================================
#define RTC_I2C_ADDRESS 0x68

// =============================================
// DISPLAY TEXT CONFIGURATION
// =============================================
#define CLOCK_TIME_FORMAT "%02d:%02d"
#define CLOCK_DATE_FORMAT "%d %s"
#define CLOCK_YEAR_FORMAT "%04d"

#define TEMP_LABEL_ECU "ECU"
#define TEMP_LABEL_MOTOR "MOTOR"
#define TEMP_LABEL_BATT "BATT"

#define BMS_LABEL_VOLTAGE "VOLT"
#define BMS_LABEL_CURRENT "ARUS"

#define POWER_LABEL_TEXT "WATT"

#define SETUP_TEXT "SETUP"
#define SETUP_TIMEOUT_MS 30000

// =============================================
// VEHICLE MODE BYTE VALUES
// =============================================
#define MODE_BYTE_PARK 0x00
#define MODE_BYTE_DRIVE 0x70
#define MODE_BYTE_SPORT 0xB0
#define MODE_BYTE_CRUISE 0x74
#define MODE_BYTE_SPORT_CRUISE 0xB4
#define MODE_BYTE_CUTOFF_DRIVE 0x72
#define MODE_BYTE_CUTOFF_SPORT 0xB2
#define MODE_BYTE_STANDBY_1 0x78
#define MODE_BYTE_STANDBY_2 0x08
#define MODE_BYTE_STANDBY_3 0xB8
#define MODE_BYTE_CHARGING_1 0x61
#define MODE_BYTE_CHARGING_2 0xA1
#define MODE_BYTE_CHARGING_3 0xA9
#define MODE_BYTE_CHARGING_4 0x69
#define MODE_BYTE_REVERSE 0x50
#define MODE_BYTE_NEUTRAL 0x40

// =============================================
// POWER DISPLAY CONFIGURATION
// =============================================
#define POWER_LABEL_POS_X 0
#define POWER_LABEL_POS_Y 0
#define POWER_VALUE_POS_X 0
#define POWER_VALUE_POS_Y 4      
#define POWER_UNIT_POS_X 110     
#define POWER_UNIT_POS_Y 28     

#define POWER_FONT_SIZE 3

#define MAX_DISPLAY_POWER 9999
#define MIN_DISPLAY_POWER -9999

// =============================================
// DISPLAY POSITION CONFIGURATION
// =============================================
#define SPLASH_POS_X 0
#define SPLASH_POS_Y 20

#define SETUP_MODE_POS_X 0
#define SETUP_MODE_POS_Y 20

// Page 1: Clock Positions
#define CLOCK_TIME_POS_X 0
#define CLOCK_TIME_POS_Y 28
#define CLOCK_DATE_POS_X 88
#define CLOCK_DATE_POS_Y 5
#define CLOCK_DAY_POS_X 88
#define CLOCK_DAY_POS_Y 15
#define CLOCK_YEAR_POS_X 88
#define CLOCK_YEAR_POS_Y 25

// Page 2: Temperature Positions
#define TEMP_LABEL_ECU_POS_X 0
#define TEMP_LABEL_ECU_POS_Y 4
#define TEMP_LABEL_MOTOR_POS_X 43
#define TEMP_LABEL_MOTOR_POS_Y 4
#define TEMP_LABEL_BATT_POS_X 86
#define TEMP_LABEL_BATT_POS_Y 4
#define TEMP_VALUE_ECU_POS_X 0
#define TEMP_VALUE_ECU_POS_Y 16
#define TEMP_VALUE_MOTOR_POS_X 43
#define TEMP_VALUE_MOTOR_POS_Y 16
#define TEMP_VALUE_BATT_POS_X 86
#define TEMP_VALUE_BATT_POS_Y 16

// Page 3: BMS Data Positions
#define BMS_LABEL_VOLTAGE_POS_X 8
#define BMS_LABEL_VOLTAGE_POS_Y 4
#define BMS_LABEL_CURRENT_POS_X 86
#define BMS_LABEL_CURRENT_POS_Y 4
#define BMS_VALUE_VOLTAGE_POS_X 0
#define BMS_VALUE_VOLTAGE_POS_Y 16
#define BMS_VALUE_CURRENT_POS_X 78
#define BMS_VALUE_CURRENT_POS_Y 16

// Page 4: Power Display Positions
#define POWER_LABEL_POS_X 0
#define POWER_LABEL_POS_Y 0
#define POWER_VALUE_POS_X 0
#define POWER_VALUE_POS_Y 4      
#define POWER_UNIT_POS_X 110     
#define POWER_UNIT_POS_Y 28     

// =============================================
// I2C CONFIGURATION
// =============================================
#define I2C_RETRY_COUNT 3
#define I2C_RETRY_DELAY_MS 5
#define I2C_MUTEX_TIMEOUT_MS 100
#define DATA_MUTEX_TIMEOUT_MS 50

#endif
