#ifndef CONFIG_H
#define CONFIG_H

// =============================================
// DOIT DevKit V1 SPECIFIC CONFIG
// =============================================

// OLED Display (GPIO16/17 di DOIT)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define SDA_PIN 16    // DOIT: GPIO16
#define SCL_PIN 17    // DOIT: GPIO17
#define OLED_ADDRESS 0x3C  // Coba 0x3D jika tidak work

// =============================================
// CAN UPDATE RATE CONFIGURATION
// =============================================

// CAN Update Rate untuk responsivitas (ms)
// 100ms = 10Hz (default, stabil)
// 50ms  = 20Hz (lebih responsif) 
// 30ms  = 33Hz (paling responsif - RECOMMENDED)
#define CAN_UPDATE_RATE_FAST_MS 30

// Current Display Deadzone (A) - filtering di display level
#define CURRENT_DISPLAY_DEADZONE 0.1f  // 0.1A deadzone di display

// =============================================
// SPLASH SCREEN CONFIGURATION
// =============================================
#define SPLASH_TEXT "WELCOME"     // TEXT sambutan ketika layar pertama kali menyala
#define SPLASH_DURATION_MS 1500   // durasi lama teks sambutan 1000=1 detik

// Pilihan font untuk splash screen:
// 0: Default font (6x8) - KECIL
// 1: FreeSansBold9pt7b  - SEDANG  
// 2: FreeSansBold12pt7b - BESAR
#define SPLASH_FONT_SIZE 1  // ganti jika tulisan terpotong

// Position configuration
#define SPLASH_POS_X 0      // 0 = auto-center, atau nilai spesifik
#define SPLASH_POS_Y 20     // Y position untuk splash text

// Font width approximations (dalam pixels)
#define FONT_WIDTH_DEFAULT  6    // Default font 6x8
#define FONT_WIDTH_9PT     18    // FreeSansBold9pt7b approx width
#define FONT_WIDTH_12PT    24    // FreeSansBold12pt7b approx width

// =============================================
// PAGE CONFIGURATION
// =============================================
#define PAGE_1_ENABLE true     // Page 1: Clock
#define PAGE_2_ENABLE true     // Page 2: Temperature
#define PAGE_3_ENABLE false     // Page 3: Voltage & Current (Masih BETA tidak responsif)
#define PAGE_SPECIAL_ID 10     // Page untuk semua mode khusus (Sport/Cruise/Charging)

// =============================================
// DAY & MONTH CONFIGURATION
// =============================================

// Array nama hari (1=Minggu, 7=Sabtu)
static const char* DAY_NAMES[] = {
    "MINGGU", "SENIN", "SELASA", "RABU", 
    "KAMIS", "JUMAT", "SABTU"
};

// Array nama bulan singkat (1=Jan, 12=Des)
static const char* MONTH_NAMES[] = {
    "JAN", "FEB", "MAR", "APR", "MEI", "JUN", 
    "JUL", "AGU", "SEP", "OKT", "NOV", "DES"
};

// =============================================
// BUTTON CONFIGURATION (OPTIMIZED)
// =============================================
#define BUTTON_PIN 25
#define DEBOUNCE_DELAY 20        // Reduced from 100ms for faster response
#define BUTTON_COOLDOWN_MS 200   // Minimum time between presses
#define BUTTON_LONG_PRESS_MS 800 // For future long-press features

// =============================================
// CAN BUS CONFIGURATION
// =============================================
#define CAN_TX_PIN 22
#define CAN_RX_PIN 21
#define CAN_BAUDRATE 250000

// ID Pesan CAN
#define ID_CTRL_MOTOR   0x0A010810UL
#define ID_BATT_5S      0x0E6C0D09UL
#define ID_BATT_SINGLE  0x0A010A10UL
#define ID_CHARGER_FAST 0x10261041UL
#define ID_VOLTAGE_CURRENT  0x0A6D0D09UL  // Gabungan voltage dan current
#define ID_SOC              0x0A6E0D09UL  // Persentase baterai atau State of Charge

// =============================================
// BMS CONFIGURATION
// =============================================
#define BMS_VOLTAGE_RESOLUTION 0.1f    // 0.1V per bit
#define BMS_CURRENT_RESOLUTION 0.1f    // 0.1A per bit
#define BMS_SIGN_BIT 7                 // Bit 7 sebagai sign bit

// Titik-titik kunci mapping BMS value -> SOC %
#define SOC_KEY_POINTS_COUNT 7

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
// LABEL TEXT untuk ditampilkan di display sebaiknya kurang dari 5 huruf
// =============================================
#define CLOCK_TIME_FORMAT "%02d:%02d"
#define CLOCK_DATE_FORMAT "%d %s"
#define CLOCK_YEAR_FORMAT "%04d"

#define TEMP_LABEL_ECU "ECU"
#define TEMP_LABEL_MOTOR "MOTOR"
#define TEMP_LABEL_BATT "BATT"

// TAMBAHKAN LABEL UNTUK PAGE 3 (BMS DATA)
#define BMS_LABEL_VOLTAGE "VOLT"
#define BMS_LABEL_CURRENT "ARUS"

#define SETUP_TEXT "SETUP"
#define SETUP_TIMEOUT_MS 30000

#define SPORT_TEXT "SPORT"
#define CRUISE_TEXT "CRUISE"
#define BLINK_INTERVAL_MS 500  // Interval kedipan untuk CRUISE

#define CHARGING_PAGE PAGE_SPECIAL_ID  // Semua mode khusus pakai page 10

// Durasi sebelum kembali ke page normal
#define AUTO_MODE_TIMEOUT_MS 5000  // 5 detik - lebih responsif

// =============================================
// VEHICLE MODE BYTE VALUES
// =============================================
#define MODE_BYTE_PARK 0x00           // Park mode
#define MODE_BYTE_DRIVE 0x70          // Drive mode normal
#define MODE_BYTE_SPORT 0xB0          // Sport mode normal
#define MODE_BYTE_CRUISE 0x74         // Drive + Cruise
#define MODE_BYTE_SPORT_CRUISE 0xB4   // Sport + Cruise
#define MODE_BYTE_CUTOFF_DRIVE 0x72   // Cutoff + Drive
#define MODE_BYTE_CUTOFF_SPORT 0xB2   // Cutoff + Sport
#define MODE_BYTE_STANDBY_1 0x78      // Standar + Drive
#define MODE_BYTE_STANDBY_2 0x08      // Standar + Park
#define MODE_BYTE_STANDBY_3 0xB8      // Standar + Sport
#define MODE_BYTE_CHARGING_1 0x61     // Locked
#define MODE_BYTE_CHARGING_2 0xA1     // Locked + Charging
#define MODE_BYTE_CHARGING_3 0xA9     // Standar + Locked
#define MODE_BYTE_CHARGING_4 0x69     // Standar + Locked + Charging
#define MODE_BYTE_REVERSE 0x50        // Reverse
#define MODE_BYTE_NEUTRAL 0x40

// =============================================
// MODE DETECTION MACROS
// =============================================
#define IS_CHARGING_MODE(b) ((b) == MODE_BYTE_CHARGING_1 || \
                             (b) == MODE_BYTE_CHARGING_2 || \
                             (b) == MODE_BYTE_CHARGING_3 || \
                             (b) == MODE_BYTE_CHARGING_4)

// PERUBAHAN: Hapus SPORT_CRUISE dari sport mode
// Sport mode HANYA 0xB0 dan 0xB2
#define IS_SPORT_MODE(b) ((b) == MODE_BYTE_SPORT || \
                         (b) == MODE_BYTE_CUTOFF_SPORT)

// PERUBAHAN: Cruise mode termasuk 0xB4 (SPORT+CRUISE dianggap cruise saja)
#define IS_CRUISE_MODE(b) ((b) == MODE_BYTE_CRUISE || \
                          (b) == MODE_BYTE_SPORT_CRUISE)  // 0xB4 dianggap cruise

#define IS_CUTOFF_MODE(b) ((b) == MODE_BYTE_CUTOFF_DRIVE || \
                          (b) == MODE_BYTE_CUTOFF_SPORT)

#define IS_DRIVING_MODE(b) ((b) == MODE_BYTE_DRIVE || \
                           (b) == MODE_BYTE_SPORT || \
                           (b) == MODE_BYTE_CRUISE || \
                           (b) == MODE_BYTE_SPORT_CRUISE || \
                           (b) == MODE_BYTE_CUTOFF_DRIVE || \
                           (b) == MODE_BYTE_CUTOFF_SPORT)

// =============================================
// DISPLAY POSITION CONFIGURATION
// x untuk jarak dari kiri semakin besar nilainya semakin ke kanan masukan nilai minus jika ingin lebih ke kiri
// y untuk jarak dari atas semakin besar nilainya semakin ke bawah masukan nilai minus jika ingin lebih ke atas
// =============================================

// Splash Screen Position
#define SPLASH_POS_X 0      // 0 = auto-center, atau nilai spesifik
#define SPLASH_POS_Y 20     // Y position untuk splash text

// Special Modes Position (Sport/Cruise/Charging)
#define SPECIAL_MODE_POS_X 10   // 0 = auto-center
#define SPECIAL_MODE_POS_Y 20  // Y position untuk mode text

// Setup Mode Position
#define SETUP_MODE_POS_X 0     // 0 = auto-center  
#define SETUP_MODE_POS_Y 20    // Y position untuk setup text

// Page 1: Clock Positions
#define CLOCK_TIME_POS_X 0     // Jam besar (kiri)
#define CLOCK_TIME_POS_Y 28
#define CLOCK_DATE_POS_X 88    // Tanggal (kanan)
#define CLOCK_DATE_POS_Y 5
#define CLOCK_DAY_POS_X 88     // Hari (kanan)
#define CLOCK_DAY_POS_Y 15
#define CLOCK_YEAR_POS_X 88    // Tahun (kanan)
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

// Page 3: BMS Data Positions - RESPONSIF VERSION
#define BMS_LABEL_VOLTAGE_POS_X 0
#define BMS_LABEL_VOLTAGE_POS_Y 4
#define BMS_LABEL_CURRENT_POS_X 86
#define BMS_LABEL_CURRENT_POS_Y 4
#define BMS_VALUE_VOLTAGE_POS_X 0
#define BMS_VALUE_VOLTAGE_POS_Y 16
#define BMS_VALUE_CURRENT_POS_X 75
#define BMS_VALUE_CURRENT_POS_Y 16

// =============================================
// FREERTOS CONFIGURATION (OPTIMIZED FOR STABILITY)
// SEBAIKNYA JANGAN DIUBAH JIKA TIDAK ADA MASALAH
// =============================================
#define I2C_RETRY_COUNT 3
#define I2C_RETRY_DELAY_MS 5
#define I2C_MUTEX_TIMEOUT_MS 100
#define DATA_MUTEX_TIMEOUT_MS 50
#define CAN_PAUSE_DURING_I2C_MS 2

// Task priorities (Reduced for stability)
#define TASK_PRIORITY_DISPLAY   3  // Reduced from 4
#define TASK_PRIORITY_CAN       2
#define TASK_PRIORITY_SERIAL    1
#define TASK_PRIORITY_DEBUG     0

// Stack sizes (INCREASED FOR STABILITY)
#define STACK_SIZE_DISPLAY      4096  // Increased from 3072
#define STACK_SIZE_CAN          4096  // Increased from 3072
#define STACK_SIZE_SERIAL       3072  // Increased from 2048
#define STACK_SIZE_DEBUG        2048  // Increased from 1024

// Core affinity
#define CORE_DISPLAY            0
#define CORE_CAN                1
#define CORE_SERIAL             0
#define CORE_DEBUG              1

// Task update rates (SLOWED DOWN FOR STABILITY)
#define DISPLAY_UPDATE_RATE_MS  50     // Reduced from 20ms to 50ms (20Hz)
#define CAN_UPDATE_RATE_MS      20     // Reduced from 10ms to 20ms (50Hz)
#define SERIAL_UPDATE_RATE_MS   100    // Reduced from 50ms to 100ms (10Hz)
#define DEBUG_UPDATE_RATE_MS    30000  // Increased from 10s to 30s

#endif
