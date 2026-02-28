
# DISPLAY JAM CUSTOM MOTOR LISTRIK POLYTRON FOX R/S

Display jam untuk motor listrik Polytron Fox R/S disertai display status kendaraan secara realtime

## 💰 DONASI

Jika kode ini membantu mungkin sedikit donasi akan sangat saya hargai

[![Donate](https://cdn-icons-png.flaticon.com/256/5002/5002279.png)](https://saweria.co/yudhaisme)


## 🧩 Features

- Menampilkan jam
- Menampilkan suhu controller, bldc, dan baterai
- Menampilkan voltase dan arus baterai
- Menampilkan daya baterai dalam watt
- Pindah Halaman layar dengan menakan tombol
- Support aplikasi Unoficial Polyron EV (https://github.com/zexry619/pev-app-release/releases/)

## 🔩 Bahan/Alat yang Dibutuhkan
- ESP32 DOIT DEVKIT V1
- RTC DS3231 + IC AT24C32
- Baterai jam CR2032
- SN65HVD230 dengan kode CJMCU-230 (BUKAN MCP2515)
- I2C LED Display 0.91 inch SSD1306
- Step down ke 5v (jika ambil dari reducer 12v ke 5v)
- Kabel, Solder, dan Timah
- Kapasitor Elco 1000uF/16V
- Kapasitor Keramik 100nF 0.1uF 104 50V 10%

## 💿 SOFTWARE dan DRIVER
### ARDUINO IDE
download di https://www.arduino.cc/en/software/
### DRIVER
#### Untuk macOS:
- CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- CH340: Otomatis terinstall di macOS High Sierra ke atas
#### Untuk Windows:
- CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- CH340: http://www.wch.cn/download/CH341SER_EXE.html

## 📚 Library
### Adafruit SSD1306 (untuk OLED Display)
- Library: Adafruit GFX Library by Adafruit  
- Instal via: Library Manager (cari "Adafruit GFX")
- Versi: 1.11.9 atau yang terbaru
### Adafruit GFX Library (dependency untuk SSD1306)
- Library: Adafruit GFX Library by Adafruit  
- Instal via: Library Manager (cari "Adafruit GFX")
- Versi: 1.11.9 atau yang terbaru
### RTClib by Adafruit
- Library: RTClib by Adafruit
- Instal via: Library Manager (cari "RTClib")
- Versi: 2.1.3 atau yang terbaru

### 

## 🪛 WIRING DAN PIN DIAGRAM

### WIRING
1. RTC DS3231 ke ESP32:
   - SDA → ESP32 Pin RX2/D16 (kiri)
   - SCL → ESP32 Pin TX2/D17 (kiri)
   - VCC → ESP32 Pin 3V3 (kiri atas)
   - GND → ESP32 Pin GND mana saja
     
2. RTC DS3231 ke OLED
   - VCC RTC ke VCC OLED 
   - GND RTC ke GND OLED
   - HUBUNGKAN GND dan VCC OLED dengan Kapasitor Keramik 100nF 0.1uF 104 50V 10%
   - SDA RTC ke SDA OLED
   - SCL RTC ke SCK OLED

3. MODUL CAN ke ESP32:
   - CAN TX → ESP32 Pin D22 (kiri)
   - CAN RX → ESP32 Pin D21 (kiri)
   - CAN VCC → ESP32 Pin 3V3 (kiri atas)
   - CAN GND → ESP32 Pin GND mana saja

4. TOMBOL (BUTTON):
   - Satu kaki → ESP32 Pin D25 (kanan)
   - Kaki lain → GND

5. KELISTRIKAN Step down ke 5v
   - Arus Positif (+) ke VIN
   - Arus Negatif (-) ke GND
   - Hubungkan VIN dan GND dengan Kapasitor Elco 1000uF/16V
   - Kapasitor Elco 1000uF/16V Kaki Panjang ke VIN (JANGAN TERBALIK)
   - Kapasitor Elco 1000uF/16V Kaki Pendek ke GND (JANGAN TERBALIK)

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
┌───────────────────────────────────────────────────────────┐
│                    ESP32 DOIT DevKit V1                   │
│                                                           │
│  ┌────────────────────────────────────────────────────┐   │
│  │                                                    │   │
│  │  3V3  ──────────────┐    ┌─────────────────────┐   │   │
│  │  EN   ────────┐     │    │                     │   │   │
│  │  VP   ────────┤     │    │  CONNECTOR PINOUT   │   │   │
│  │  VN   ────────┤     │    │                     │   │   │
│  │  D34  ────────┤     │    └─────────────────────┘   │   │
│  │  D35  ────────┤     │                              │   │
│  │  D32  ────────┤     │    OLED DISPLAY (SSD1306)    │   │
│  │  D33  ────────┤     │    ┌─────────────────────┐   │   │
│  │  D25  ────────┼─────┼────┤ BUTTON   (GPIO25)   │   │   │
│  │  D26  ────────┤     │    │                     │   │   │
│  │  D27  ────────┤     │    │ SDA      (GPIO16)   ├───┼───┤
│  │  D14  ────────┤     │    │                     │   │   │
│  │  D12  ────────┤     │    │ SCL      (GPIO17)   ├───┼───┤
│  │  D13  ────────┤     │    │                     │   │   │
│  │  GND  ────────┼─────┼────┤ GND ──┬─ 100nF ──┐  │   │   │
│  │  D23  ────────┤     │    │       │          │  │   │   │
│  │  D22  ────────┼─────┼────┤ VCC ──┴─ 100nF ──┴─ │   │   │
│  │  TXD  ────────┤     │    │                     │   │   │
│  │  RXD  ────────┤     │    └─────────────────────┘   │   │
│  │  D21  ────────┼─────┼────┐                         │   │
│  │  GND  ────────┼─────┼────┤                         │   │
│  │  D19  ────────┤     │    │ SN65HVD230 CJMCU-230    │   │
│  │  D18  ────────┤     │    │ ┌───────────────────┐   │   │
│  │  D5   ────────┤     │    │ │                   │   │   │
│  │  D17  ────────┼─────┼────┤ │ CAN_RX   (GPIO21) ├───┼───┤
│  │  D16  ────────┼─────┼────┤ │                   │   │   │
│  │  D4   ────────┤     │    │ │ CAN_TX   (GPIO22) ├───┼───┤
│  │  D0   ────────┤     │    │ │                   │   │   │
│  │  D2   ────────┤     │    │ │ VCC      (5V/3V3) ├───┼───┤
│  │  D15  ────────┤     │    │ │                   │   │   │
│  │  D8   ────────┤     │    │ │ GND               ├───┼───┤
│  │  D7   ────────┤     │    │ │                   │   │   │
│  │  MOSI ────────┤     │    │ │ CS       (GPIO5)  │   │   │
│  │  MISO ────────┤     │    │ │                   │   │   │
│  │  5V   ────────┼─────┼────┤ │ SCK      (GPIO18) │   │   │
│  │  GND  ────────┼─────┼────┤ │                   │   │   │
│  │  GND  ────────┤     │    │ │ INT      (GPIO19) │   │   │
│  │  VIN  ────┐   │     │    │ └───────────────────┘   │   │
│  │           │   │     │    │                         │   │
│  └───────────┴───┴─────┴────┴─────────────────────────┘   │
│           │                                               │
│        [1000µF/16V]                                       │
│           │                                               │
│           ├─── GND                                        │
│                                                           │
└───────────────────────────────────────────────────────────┘
```

# Cara Instalasi di Arduino IDE:
Buka program ARDUINO IDE di PC/Mac lalu ikuti langkah berikut

## Metode 1: Via Library Manager (Direkomendasikan)

1. Buka Arduino IDE

- Sketch → Include Library → Manage Libraries...

2. Cari dan instal library berikut:

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

1. File → Preferences

1. Di "Additional Boards Manager URLs", tambahkan:
```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

### Instal ESP32 Board:

1. Tools → Board → Boards Manager...

1. Cari "ESP32"

1. Install "ESP32 by Espressif Systems"

1. Pilih Board:

1. Tools → Board → ESP32 Arduino

1. Pilih: ESP32 Dev Module

## Struktur File Projek

```JAMFOXRSV1
📁JAMFOXRS
├── JAMFOXRS.ino            # File Utama
├── fox_ble.h               # Header untuk BLE
├── fox_ble.cpp             # Fungsi untuk aplikasi via BLE
├── fox_canbus.h            # Header CAN bus
├── fox_canbus.cpp          # Implementasi CAN bus
├── fox_config.h            # Konfigurasi pin, label teks, posisi teks, dll
├── fox_display.h           # Header display
├── fox_display.cpp         # Implementasi display
├── fox_page.h              # Header page display
├── fox_page.cpp.h          # Implementasi sistem page
├── fox_rtc.h               # Header RTC
├── fox_rtc.cpp             # Implementasi RTC
├── fox_serial.h            # Header serial command
├── fox_serial.cpp          # Implementasi command serial
├── fox_task.h              # Header task
├── fox_task.cpp            # Implementasi pembagian tugas prosesor
├── fox_vehicle.cpp         # Parsing message canbus
└── fox_vehicle.h           # Header vehicle data
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

## 📱 Cara Menghubungkan Ke Aplikasi Unoficial Polytron EV
- Tekan tahan tombol sekitar 5-7 detik hingga muncul tulisan "APP MODE" di OLED Display
- Koneksikan modul melalui aplikasi Unoficial Polyron EV (https://github.com/zexry619/pev-app-release/releases/)
- Untuk keluar dari mode aplikasi tekan tahan tombol 5-7 detik hingga muncul tulisan "BLE OFF"


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
### Thanks to

- @zexry619 (https://github.com/zexry619/votol-esp32-can-bus)
- Tri Suliswanto on Facebook

Pull requests welcome! 
Untuk major changes, buka issue terlebih dahulu.

Dibuat dengan ❤️ untuk komunitas kendaraan listrik Indonesia
