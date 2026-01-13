# JAMFOXRS
Display jam untuk motor listrik Polytron Fox R/S disertai display suhu kendaraan secara realtime

## Features
- **Dual Page Display**: 
  - Page 1: Clock dengan jam besar dan tanggal
  - Page 2: Temperature dari CAN bus (ECU, Motor, Baterai)
- **RTC DS3231**: Waktu real-time dengan backup baterai
- **CAN Bus Interface**: Membaca data suhu dari sistem EV
- **Serial Command Interface**: Konfigurasi via Serial Monitor
- **Setup Mode**: Mode konfigurasi dengan indikator
- **Button Navigation**: Tombol untuk ganti halaman

## Hardware Requirements
- ESP32 (WROOM-32)
- OLED 128x32 (SSD1306, I2C)
- DS3231 RTC dengan baterai CR2032
- CAN Transceiver (MCP2515/TJA1050)
- Push button
- Power supply 5V/3.3V

## Pin Connections
| ESP32 Pin | Connected To |
|-----------|--------------|
| GPIO16    | OLED SDA + RTC SDA |
| GPIO17    | OLED SCL + RTC SCL |
| GPIO22    | CAN Transmitter (TX) |
| GPIO21    | CAN Receiver (RX) |
| GPIO25    | Push Button (to GND) |
| 3.3V      | OLED VCC, RTC VCC |
| GND       | All grounds |

## Project Structure
- `EVDisplayRTC.ino` - Main file
- `config.h` - Pin configuration
- `display.h/cpp` - OLED display module
- `canbus.h/cpp` - CAN bus reader
- `rtc.h/cpp` - DS3231 RTC module

## Installation
1. Install Arduino IDE dengan ESP32 board support
2. Install libraries: Adafruit SSD1306, Adafruit GFX
3. Upload semua file ke ESP32
4. Open Serial Monitor (115200 baud)

## Serial Commands
Berikut adalah menu yang bisa digunakan di Serial monitor
| Command | Description | Example |
|---------|-------------|---------|
| `HELP` | Menunjukkan semua command | `HELP` |
| `TIME HH:MM:SS` | Set waktu (format 24jam) | `TIME 14:30:00` |
| `DATE DD/MM/YYYY` | Set tanggal | `DATE 20/03/2024` |
| `DAY 1-7` | Set hari (1=Minggu) | `DAY 3` |
| `DEBUG` | Tampilkan info RTC | `DEBUG` |
| `DEBUG ON/OFF` | nyalakan/matikan debug | `DEBUG ON` |
| `SETUP` | masuk mode setup | `SETUP` |
| `SAVE` | keluar mode setup | `SAVE` |
| `PAGE 1\|2` | Ganti halaman | `PAGE 2` |

## Konfigurasi CAN Bus
- **Baud Rate CAN**: 250kbps
- **Mode CAN**: Hanya Mendengarkan (Listen Only)
- **Suhu Default**: 25°C
- **ID CAN**:
  - `0x0A010810`: Suhu Controller & Motor
  - `0x0E6C0D09`: Suhu Baterai (5 sel)
  - `0x0A010A10`: Suhu Baterai tunggal

## Penggunaan Tombol
- **Tekan sekali**: Berganti antara Halaman 1 dan Halaman 2
- **Tombol dinonaktifkan** selama Mode Setup

## Mode Setup
- **Masuk**: Ketik `SETUP` di Serial Monitor
- **Keluar**: Ketik `SAVE` atau tunggu 30 detik
- **Display**: Menampilkan "SETUP" berkedip

## Troubleshooting
1. **OLED tidak menyala**: Cek koneksi I2C, coba address 0x3D
2. **RTC tidak terdeteksi**: Cek baterai CR2032, verifikasi koneksi I2C
3. **Data CAN tidak masuk**: Verifikasi baud rate 250kbps, cek resistor termination
4. **Tombol tidak bekerja**: Cek wiring, pastikan INPUT_PULLUP aktif
5. **Waktu tidak akurat**: Gunakan command TIME/DATE untuk set ulang

## License
MIT License

## Notes
Proyek ini untuk tujuan edukasi dan hobi. Gunakan dengan risiko sendiri pada kendaraan nyata.
