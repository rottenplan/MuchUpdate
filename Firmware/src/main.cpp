#include "config.h"
#include "core/GPSManager.h"
#include <TAMC_GT911.h>

#include "core/SessionManager.h"
#include "core/SyncManager.h"
#include "core/WiFiManager.h"
#include "ui/UIManager.h"
#include <Arduino.h>

// Objek Perangkat Keras
TFT_eSPI tft = TFT_eSPI();
TAMC_GT911 touch(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH,
                 TOUCH_HEIGHT);
// TouchLib touch(
//     Wire,
//     0x00); // Placeholder, menunggu hasil list_dir untuk tahu konstruktor
UIManager uiManager(&tft);
GPSManager gpsManager;
SessionManager sessionManager;
WiFiManager wifiManager;
SyncManager syncManager;

// SPIClass touchSpi = SPIClass(HSPI); // Tidak diperlukan untuk I2C

void setup() {
  Serial.begin(115200); // Standard PC Debugging on GPIO 1/3
  Serial.println("Starting RaceBox Clone...");

  // Inisialisasi Pin
  // Force Backlight OFF immediately to prevent startup glitches
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, LOW);

  // SAFETY: Ensure Pin 21 (Touch Int) is not driven High by default
  pinMode(21, INPUT_PULLUP);

  // Inisialisasi UI (TFT)
  tft.init();
  tft.setRotation(1); // 0=Potret, 1=Lanskap. Periksa pemasangan Anda!
  // tft.fillScreen(COLOR_BG); // Removed to prevent startup flash; SplashScreen
  // will fill it.

  // Inisialisasi Sentuh
  // Inisialisasi instance SPI khusus untuk Sentuh
  // Pin dari Skematik: CLK=14, MISO=12, MOSI=13, CS=33
  touch.begin();
  // touch.setRotation(1); // Coba 1 (Kiri/Lanskap) untuk mencocokkan Rotasi
  // TFT 1. touch.setRotation(ROTATION_RIGHT); // Cocokkan rotasi TFT (1) if
  // (!touch.begin()) { // XPT begin biasanya mengembalikan void, atau kita
  // asumsikan itu bekerja
  //   Serial.println("Inisialisasi sentuh gagal!");
  // }

  // Inisialisasi PWM Lampu Latar (Start OFF)
  ledcSetup(0, 5000, 8); // Saluran 0, 5kHz, 8-bit
  ledcAttachPin(PIN_TFT_BL, 0);
  ledcWrite(0, 0); // Matikan backlight dulu untuk mencegah glitch

  // Inisialisasi Inti
  gpsManager.begin();
  sessionManager.begin();

  // Link GPS to WiFi for Web API
  wifiManager.setGPS(&gpsManager);

  wifiManager.begin(); // Setup AP and Server

  // Bersihkan layar sebelum menyalakan backlight
  tft.fillScreen(TFT_BLACK);
  delay(100); // Ensure black frame is rendered

  // Nyalakan Backlight
  ledcWrite(0, 255);

  uiManager.setTouch(&touch); // Teruskan objek sentuh ke UI
  uiManager.begin();

  // Serial.println("System Ready"); // DISABLED: Serial used for GPS
}

void loop() {
  gpsManager.update();

  uiManager.update();
  wifiManager.update();
  // sessionManager menyimpan otomatis saat buffer flush jika diperlukan, tetapi
  // kita tulis langsung untuk saat ini
  delay(5);
}
