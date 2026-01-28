#include "SplashScreen.h"
#include "../fonts/Org_01.h"
#include "SplashScreenAssets.h"
#include <Preferences.h>

void SplashScreen::onShow() {
  TFT_eSPI *tft = _ui->getTft();

  // Center bitmap (320x240) on 480x320 screen
  int dx = (SCREEN_WIDTH - 320) / 2;  // 80px offset
  int dy = (SCREEN_HEIGHT - 240) / 2; // 40px offset
  tft->drawBitmap(dx, dy, image_BOLONG_bits, 320, 240, 0xFFFF);

  // Draw "ENGINE STARTING" text centered (smaller font)
  tft->setTextColor(0xFFFF);
  tft->setTextSize(FONT_SIZE_SPLASH_TEXT);
  tft->setFreeFont(&Org_01);
  tft->setTextDatum(MC_DATUM);
  tft->drawString("ENGINE STARTING", SCREEN_WIDTH / 2, 214 + dy);

  // Initialize progress bar (thinner: 8px height, longer: 280px width,
  // centered)
  _progress = 0;
  int barX =
      (SCREEN_WIDTH - 280) / 2; // Center the 280px bar: (480-280)/2 = 100px
  tft->fillRect(barX, 198 + dy, _progress, 8, 0xFFFF);

  _lastUpdate = millis();
}

void SplashScreen::update() {
  int dx = (SCREEN_WIDTH - 320) / 2;
  int dy = (SCREEN_HEIGHT - 240) / 2;

  // Progress bar animation (thinner: 8px height, longer: 280px width, centered)
  if (_progress < 280) {
    if (millis() - _lastUpdate > 10) {
      _progress += 2;

      TFT_eSPI *tft = _ui->getTft();
      int barX = (SCREEN_WIDTH - 280) / 2; // Center the bar
      tft->fillRect(barX, 198 + dy, _progress, 8, 0xFFFF);

      _lastUpdate = millis();
    }
  } else {
    // Animasi selesai, tunggu sebentar lalu beralih
    if (millis() - _lastUpdate > 1000) {
      // Check if this is first launch
      Preferences prefs;
      prefs.begin("muchrace", true); // Read-only
      bool setupDone = prefs.getBool("setup_done", false);
      prefs.end();

      if (!setupDone) {
        // First launch - go to setup
        Serial.println("First launch detected, showing setup screen");
        _ui->switchScreen(SCREEN_SETUP);
      } else {
        // Normal launch - go to menu
        _ui->switchScreen(SCREEN_MENU);
      }
    }
  }
}