
# DISPLAY JAM CUSTOM MOTOR LISTRIK POLYTRON FOX R/S

Display jam untuk motor listrik Polytron Fox R/S disertai display suhu kendaraan secara realtime

## DONASI

Jika kode ini membantu mungkin sedikit donasi akan sangat saya hargai

[![Donate](https://cdn-icons-png.flaticon.com/256/5002/5002279.png)](https://saweria.co/yudhaisme)


## Features

- Menampilkan jam
- Menampilkan suhu controller, bldc, dan baterai
- Menampilkan speed 3 digit untuk mengatasi limitasi speedo bawaan yang hanya dua digit
- Notifikasi Cruise aktif
- Pindah Halaman layar dengan menakan tombol


## Bahan/Alat yang Dibutuhkan
- ESP32 DOIT DEVKIT V1
- RTC DS3231 + IC AT24C32
- Baterai jam CR2032
- CJMCU-230
- SN65HVD230 dengan kode CJMCU-230 (BUKAN MCP2515)
- Step down ke 5v (jika ambil dari reducer 12v ke 5v)
- Kabel, Solder, dan Timah
## SOFTWARE dan DRIVER
### ARDUINO IDE
download di https://www.arduino.cc/en/software/
### DRIVER
#### Untuk macOS:
- CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- CH340: Otomatis terinstall di macOS High Sierra ke atas
#### Untuk Windows:
- CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- CH340: http://www.wch.cn/download/CH341SER_EXE.html

## Library
### Adafruit SSD1306 (untuk OLED Display)
Library: Adafruit GFX Library by Adafruit  
Instal via: Library Manager (cari "Adafruit GFX")
Versi: 1.11.9 atau yang terbaru
### Adafruit GFX Library (dependency untuk SSD1306)
Library: Adafruit GFX Library by Adafruit  
Instal via: Library Manager (cari "Adafruit GFX")
Versi: 1.11.9 atau yang terbaru
### RTClib by Adafruit
Library: RTClib by Adafruit
Instal via: Library Manager (cari "RTClib")
Versi: 2.1.3 atau yang terbaru
### 

## PIN DIAGRAM

### ESP32 PIN DIAGRAM
```
┌─────────────────────────────────────────────┐
│              ESP32 DEV MODULE               │
│                                             │
│  [3V3] ────○ [EN]                           │
│  [GND] ────○ [VP]                           │
│  [D13] ────○ [D34]                          │
│  [D12] ────○ [D35]                          │
│  [D14] ────○ [D32]                          │
│  [D27] ────○ [D33]                          │
│  [D26] ────○ [D25] ←── BUTTON               │
│  [D25] ────○ [D26]                          │
│  [D33] ────○ [D27]                          │
│  [D32] ────○ [D14]                          │
│  [D35] ────○ [D12]                          │
│  [D34] ────○ [D13]                          │
│  [VN]  ────○ [GND]                          │
│  [VP]  ────○ [3V3]                          │
│  [EN]  ────○ [EN]                           │
│                                             │
│  [GND] ────○ [D15]                          │
│  [D2]  ────○ [D2]                           │
│  [D4]  ────○ [D4]                           │
│  [RX2] ────○ [RX2]                          │
│  [TX2] ────○ [TX2]                          │
│  [D5]  ────○ [D5]                           │
│  [D18] ────○ [D18]                          │
│  [D19] ────○ [D19]                          │
│  [D21] ←───○ [D21] ─── CAN RX               │
│  [RX0] ────○ [RX0]                          │
│  [TX0] ────○ [TX0]                          │
│  [D22] ←───○ [D22] ─── CAN TX               │
│  [D23] ────○ [D23]                          │
│  [GND] ────○ [GND]                          │
│  [D16] ←───○ [D16] ─── OLED SDA/RTC SDA     │
│  [D17] ←───○ [D17] ─── OLED SCL/RTC SCL     │
│                                             │
└─────────────────────────────────────────────┘
```

### Pin Configuration di fox_config.h
```
// Hardware Pin Configuration
#define SDA_PIN 16      // GPIO16 untuk OLED SDA dan RTC SDA
#define SCL_PIN 17      // GPIO17 untuk OLED SCL dan RTC SCL
#define BUTTON_PIN 25   // GPIO25 untuk tombol
#define CAN_TX_PIN 22   // GPIO22 untuk CAN TX
#define CAN_RX_PIN 21   // GPIO21 untuk CAN RX
```

### Diagram Sistem Lengkap - I2C Star Topology
```
┌─────────────────────────────────────────────────────────┐
│                     JAMFOXRS SYSTEM                     │
│                                                         │
│  ┌────────────┐      ┌────────────┐      ┌────────────┐ │
│  │            │      │            │      │            │ │
│  │   ESP32    │      │    RTC     │      │   OLED     │ │
│  │   DEV      │      │   DS3231   │      │  128x32    │ │
│  │  MODULE    │      │ (Tengah)   │      │   I2C      │ │
│  │            │      │            │      │            │ │
│  │            │      │            │      │            │ │
│  │  GPIO16────┼──────┤ SDA        ├──────┤ SDA        │ │
│  │            │      │            │      │            │ │
│  │  GPIO17────┼──────┤ SCL        ├──────┤ SCL        │ │
│  │            │      │            │      │            │ │
│  │    3.3V────┼──────┤ VCC        ├──────┤ VCC        │ │
│  │            │      │            │      │            │ │
│  │    GND─────┼──────┤ GND        ├──────┤ GND        │ │
│  │            │      │            │      │            │ │
│  └────────────┘      └────────────┘      └────────────┘ │
│          │                       │                      │
│          │  ┌────────────┐       │                      │
│          │  │            │       │                      │
│          │  │   CAN      │       │                      │
│          │  │ TRANSCEIVER│       │                      │
│          │  │ MCP2551    │       │                      │
│          │  │            │       │                      │
│  GPIO22──┼──┤ TX         │       │                      │
│          │  │            │       │                      │
│  GPIO21──┼──┤ RX         │       │                      │
│          │  │            │       │                      │
│    3.3V──┼──┤ VCC        │       │                      │
│          │  │            │       │                      │
│    GND───┼──┤ GND        │       │                      │
│          │  │            │       │                      │
│          │  └────────────┘       │                      │
│          │         │             │                      │
│          │     ┌───┴───┐         │                      │
│          │     │ CAN_H │         │                      │
│          │     │ CAN_L │         │                      │
│          │     └───┬───┘         │                      │
│          │         │             │                      │
│          │  ┌──────┴──────┐      │                      │
│          │  │  FOX EV     │      │                      │
│          │  │  CAN BUS    │      │                      │
│          │  └─────────────┘      │                      │
│                                  │                      │
│  ┌────────────┐                  │                      │
│  │            │                  │                      │
│  │   PUSH     │                  │                      │
│  │  BUTTON    │                  │                      │
│  │            │                  │                      │
│  │     ├──────┼──────────────────┘                      │
│  │     │      │                                         │
│  └─────┼──────┘                                         │
│        │ GPIO25                                         │
│        ▼                                                │
│  ┌────────────┐                                         │
│  │   GND      │                                         │
│  └────────────┘                                         │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

# Cara Instalasi di Arduino IDE:

## Metode 1: Via Library Manager (Direkomendasikan)

Buka Arduino IDE
Sketch → Include Library → Manage Libraries...

Cari dan instal library berikut:

- Adafruit SSD1306
- Adafruit GFX Library
- RTClib by Adafruit 

## Metode 2: Manual Install (jika perlu)

Download dari GitHub:

https://github.com/adafruit/Adafruit_SSD1306

https://github.com/adafruit/Adafruit-GFX-Library

Sketch → Include Library → Add .ZIP Library...
Pilih file ZIP yang sudah didownload
## Konfigurasi Board ESP32:
### Tambahkan Board Manager URL:

File → Preferences

Di "Additional Boards Manager URLs", tambahkan:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

### Instal ESP32 Board:

Tools → Board → Boards Manager...

Cari "ESP32"

Install "ESP32 by Espressif Systems"

Pilih Board:

Tools → Board → ESP32 Arduino

Pilih: ESP32 Dev Module
## Struktur File Projek

```JAMFOXRSV1
├── JAMFOXRSV1.ino          # Main sketch
├── fox_config.h            # Konfigurasi pin dan parameter
├── fox_display.h           # Header display
├── fox_display.cpp         # Implementasi display
├── fox_canbus.h            # Header CAN bus
├── fox_canbus.cpp          # Implementasi CAN bus
├── fox_vehicle.h           # Header vehicle data
├── fox_vehicle.cpp         # Implementasi vehicle data
├── fox_rtc.h              # Header RTC
└── fox_rtc.cpp            # Implementasi RTC
```


## Instal JAMFOXRS

1. Download kode di github ini (download sebagai zip)

1. Extract dan pastikan semua file berada di dalam satu folder

1. Buat nama folder sama dengan file .ino (dalam kasus ini buat nama folder jadi JAMFOXRS)

1. Buka file JAMFOXRS.ino di arduino IDE

1. Silahkan edit file fox_config.h jika ingin mengubah beberapa hal sesuai keinginan

1. Pastikan ESP32 sudak terkoneksi dengan benar

1. Pilih board DOIT ESP32 DEVKIT V1

1. Pilih port COM yang benar

1. Upload sketch ke esp32 board

1. Setelah upload selesai Buka Serial Monitor (115200 baud)

## Serial Command

Ketik HELP di serial command maka akan muncul

```
=== COMMAND LIST ===
HELP          - Show this help
DAY [1-7]     - Set day of week (1=Minggu, 7=SABTU)
TIME HH:MM:SS - Set time (24h format)
DATE DD/MM/YYYY - Set date
DEBUG         - Show RTC info
DEBUG ON/OFF  - Enable/disable periodic debug
SETUP         - Enter setup mode
SAVE          - Exit setup mode
PAGE [1|2]    - Switch display page
VEHICLE       - Show vehicle data
CAPTURE ON    - Enable unknown CAN ID capture
CAPTURE OFF   - Disable unknown CAN ID capture
==========================
Day mapping: 1=MINGGU, 2=SENIN, 3=SELASA,
             4=RABU, 5=KAMIS, 6=JUMAT, 7=SABTU
==========================
```

### Cara Set waktu dan tanggal

#### Di Serial Monitor ketik seperti di bawah ini

##### Ketik TIME JAM:MENIT:DETIK lalu enter

```contoh: TIME 18:30:40 set waktu ke jam 18 menit 30 detik 40```

##### Ketik DATE TANGGAL/BULAN/TAHUN lalu enter

```contoh: DATE 17/09/2026 set waktu ke 17 Agustus 2026```

##### Ketik DAY lalu diikuti angka 1-7 lalu enter

```contoh: DAY 1 akan set hari ke Minggu```


## ⚠️ Safety Warning

- Jangan modifikasi kendaraan tanpa pengetahuan yang cukup
   - Saya tidak bertanggung jawab atas kerusakan yang bisa ditimbulkan jika ada kesalahan dalam mengimplementasikan kode ini

- DILARANG MEMPERJUALBELIKAN kode ini baik dalam bentuk software maupun fisik (hardware)
   - Kode belum tentu bekerja di semua jenis kendaraan Polytron bahkan untuk yang setype sekalipun, karena itu dimohon untuk tidak memperjualkan kode ini ke umum
   - Mungkin hanya bekerja di kendaraan dengan controller Votol EM-100
   - Kesalahan mungkin dapat membuat baterai rusak apalagi jika sistem sewa anda bisa dituntut untuk mengganti baterai yang rusak

- Test di area aman sebelum digunakan di jalan


## 📄 License

MIT License - bebas untuk digunakan dan dimodifikasi.

## 🤝 Contributing

Pull requests welcome! 
Untuk major changes, buka issue terlebih dahulu.

Dibuat dengan ❤️ untuk komunitas kendaraan listrik Indonesia
