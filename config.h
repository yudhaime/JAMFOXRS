#ifndef CONFIG_H
#define CONFIG_H

// =============================================
// CONFIGURASI
// =============================================

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define SDA_PIN 16
#define SCL_PIN 17
#define OLED_ADDRESS 0x3C

// Tombol
#define BUTTON_PIN 25
#define DEBOUNCE_DELAY 50

// CAN Bus
#define CAN_TX_PIN 22
#define CAN_RX_PIN 21
#define CAN_BAUDRATE 250000
#define CAN_MODE 1  // TWAI_MODE_LISTEN_ONLY

// ID Pesan CAN
#define ID_CTRL_MOTOR   0x0A010810UL
#define ID_BATT_5S      0x0E6C0D09UL  
#define ID_BATT_SINGLE  0x0A010A10UL

// Suhu
#define DEFAULT_TEMP 25
#define DATA_TIMEOUT 10000

// RTC
#define RTC_I2C_ADDRESS 0x68

// =============================================
// END CONFIG
// =============================================

#endif