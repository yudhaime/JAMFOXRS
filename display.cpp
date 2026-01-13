#include "display.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "config.h"
#include "rtc.h"
#include "canbus.h"
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool displayInitialized = false;

void initDisplay() {
    Serial.println("Initializing OLED...");
    
    Wire.begin(SDA_PIN, SCL_PIN);
    delay(100);
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println("OLED FAILED!");
        displayInitialized = false;
        return;
    }
    
    Serial.println("OLED OK!");
    displayInitialized = true;
    
    // Clear display
    display.clearDisplay();
    
    // ===== SPLASH SCREEN "POLYTRON" DENGAN FreeSansBold12pt7b =====
    // Sama dengan font angka suhu di halaman 2
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(SSD1306_WHITE);
    
    // Hitung posisi untuk "POLYTRON"
    // FreeSansBold12pt: "POLYTRON" ≈ 100 pixel lebar
    // x = (128 - 100)/2 = 14
    // y = 18 untuk posisi vertikal yang pas (32-16)/2 + 2
    
    display.setCursor(0, 18);
    display.print("WELCOME");
    display.display();
    
    delay(1500); // Tampilkan 1.5 detik
    display.clearDisplay();
    display.display();
    
    // Kembali ke font default untuk mode normal
    display.setFont();
    display.setTextSize(1);
}

void updateDisplay(int page){
    if(!displayInitialized) return;
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    if(page == 1){
        // Page 1: Clock layout dengan font FreeSansBold18pt untuk jam
        RTCDateTime dt = getRTC();
        
        // ===== KOLOM KIRI: JAM BESAR HH:MM dengan FreeSansBold18pt =====
        // Format HH:MM
        char timeStr[6];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", dt.hour, dt.minute);
        
        // Gunakan font FreeSansBold18pt untuk jam
        display.setFont(&FreeSansBold18pt7b);
        
        // Posisi jam: x=0, y=28 (baseline diturunkan)
        display.setCursor(0, 28);
        display.print(timeStr);
        
        // ===== KOLOM KANAN: INFO HARI/TANGGAL/TAHUN =====
        // Kembali ke font DEFAULT dengan size 1 untuk semua
        display.setFont();  // Reset ke font default system
        display.setTextSize(1); // Size 1 adalah yang terkecil
        
        // Array nama hari dan bulan
        const char* hari[] = {"MINGGU", "SENIN", "SELASA", "RABU", "KAMIS", "JUMAT", "SABTU"};
        const char* bulan[] = {"JAN", "FEB", "MAR", "APR", "MEI", "JUN", 
                              "JUL", "AGU", "SEP", "OKT", "NOV", "DES"};
        
        // Konversi dayOfWeek ke index array
        int hariIndex = dt.dayOfWeek - 1;
        if(hariIndex < 0) hariIndex = 0;
        if(hariIndex > 6) hariIndex = 6;
        
        // Konversi month ke index array
        int bulanIndex = dt.month - 1;
        if(bulanIndex < 0) bulanIndex = 0;
        if(bulanIndex > 11) bulanIndex = 11;
        
        // Posisi kolom kanan
        int rightCol = 88;
        
        // Baris 1: Nama hari (MINGGU, SENIN, dll)
        display.setCursor(rightCol, 5);
        display.print(hari[hariIndex]);
        
        // Baris 2: Tanggal dan bulan (1 JAN)
        char tanggalBulan[10];
        snprintf(tanggalBulan, sizeof(tanggalBulan), "%d %s", dt.day, bulan[bulanIndex]);
        display.setCursor(rightCol, 15);
        display.print(tanggalBulan);
        
        // Baris 3: Tahun (2000)
        display.setCursor(rightCol, 25);
        display.print(dt.year);
    }
    else if(page == 2){
        // Page 2: Temperature - Layout baru dengan text size 2 untuk angka
        
        // ===== BARIS ATAS: LABEL DENGAN TEXT SIZE 1 =====
        display.setTextSize(1);
        
        // Kolom 1: ECU (0, 4)
        display.setCursor(0, 4);
        display.print("ECU");
        
        // Kolom 2: MOTOR (43, 4)
        display.setCursor(43, 4);
        display.print("MOTOR");
        
        // Kolom 3: BATT (86, 4)
        display.setCursor(86, 4);
        display.print("BATT");
        
        // ===== BARIS BAWAH: ANGKA SUHU DENGAN TEXT SIZE 2 =====
        display.setTextSize(2);
        
        // Format angka suhu (maks 3 digit: 100)
        char ecuStr[4], motorStr[4], battStr[4];
        snprintf(ecuStr, sizeof(ecuStr), "%d", tempCtrl);
        snprintf(motorStr, sizeof(motorStr), "%d", tempMotor);
        snprintf(battStr, sizeof(battStr), "%d", tempBatt);
        
        // Kolom 1: ECU temperature (0, 16)
        display.setCursor(0, 16);
        display.print(ecuStr);
        
        // Kolom 2: MOTOR temperature (43, 16)
        display.setCursor(43, 16);
        display.print(motorStr);
        
        // Kolom 3: BATT temperature (86, 16)
        display.setCursor(86, 16);
        display.print(battStr);
    }
    
    display.display();
}

void showSetupMode(bool blinkState) {
    if(!displayInitialized) return;
    
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 10);
    if (blinkState) {
        display.print("SETUP");
    }
    display.display();
}