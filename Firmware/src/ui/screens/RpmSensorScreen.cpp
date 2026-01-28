#include "RpmSensorScreen.h"
#include "../../config.h"
#include "../fonts/Org_01.h"
#include <Arduino.h>
#include <Preferences.h>

// Define colors if not in config
#define COLOR_ORANGE 0xFDA0
#define COLOR_GREEN 0x07E0

// Initialize static members
// volatile unsigned long RpmSensorScreen::_rpmPulses = 0;
// volatile unsigned long RpmSensorScreen::_lastPulseMicros = 0;

// void IRAM_ATTR RpmSensorScreen::onPulse() {
//   unsigned long now = micros();
//   // Debounce: 1ms (1000us) dead time -> Max 60,000 RPM
//   // Filters out high-frequency ringing from spark
//   // if (now - _lastPulseMicros > 1000) {
//   //   _rpmPulses++;
//   //   _lastPulseMicros = now;
//   // }
// }

// External reference
#include "../../core/GPSManager.h"
extern GPSManager gpsManager;

void RpmSensorScreen::onShow() { drawScreen(); }

void RpmSensorScreen::onHide() {
  // No sprite to delete
  // Manual Wipe to prevent ghosting
  TFT_eSPI *tft = _ui->getTft();
  tft->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                SCREEN_HEIGHT - STATUS_BAR_HEIGHT, TFT_BLACK);
}

void RpmSensorScreen::update() {
  // 1. Back Button
  UIManager::TouchPoint p = _ui->getTouchPoint();
  if (p.x != -1 && p.x < 60 && p.y < 60) {
    _ui->switchScreen(SCREEN_MENU); // Instant Switch (Single Tap)
    return;
  }

  // 2. Real RPM Calculation (Get from Global)
  unsigned long now = millis();
  if (now - _lastRpmCalcTime > 100) {
    _lastRpmCalcTime = now;

    // Use Global Manager
    _currentRpm = gpsManager.getRPM();

    if (_currentRpm > _maxRpm)
      _maxRpm = _currentRpm;

    // RPM Level (0-12000 scaling)
    // int maxScale = 12000;
    // _currentLvl = map(constrain(_currentRpm, 0, maxScale), 0, maxScale, 0,
    // 100);

    // Redraw Dynamic Parts
    updateValues();
  }
}

void RpmSensorScreen::drawScreen() {
  TFT_eSPI *tft = _ui->getTft();
  // Clear only content area
  _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                            SCREEN_HEIGHT - STATUS_BAR_HEIGHT);

  // --- STANDARD HEADER ---
  int headerY = 20;
  tft->drawFastHLine(0, headerY, SCREEN_WIDTH, _ui->getSecondaryColor());

  // Title
  tft->setTextColor(TFT_WHITE, _ui->getBackgroundColor());
  tft->setTextDatum(TC_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->drawString("RPM SENSOR", SCREEN_WIDTH / 2, headerY + 8);

  // --- SENSOR STATUS ---
  tft->setTextSize(1);
  if (gpsManager.isRpmEnabled()) {
    tft->setTextColor(COLOR_GREEN, _ui->getBackgroundColor());
    tft->drawString("STATUS: ACTIVE", SCREEN_WIDTH / 2, headerY + 28);
  } else {
    tft->setTextColor(TFT_RED, _ui->getBackgroundColor());
    tft->drawString("STATUS: DISABLED (CHECK SETTINGS)", SCREEN_WIDTH / 2,
                    headerY + 28);
  }

  // Back Button (Blue Triangle) - Top Left (Standardized)
  // Triangle pointing Left: (10, 35), (22, 29), (22, 41)
  tft->fillTriangle(10, 35, 22, 29, 22, 41, TFT_BLUE);

  // --- INFO CARDS ---
  // --- INFO CARDS (Resized & Centered) ---
  int cardY = 80;
  int cardH = 180;                     // Tall & Big
  int cardW = (SCREEN_WIDTH - 30) / 2; // Split half with gap

  // MAX RPM Card (Left)
  tft->fillRoundRect(10, cardY, cardW, cardH, 12, 0x18E3); // Charcoal
  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->setTextSize(2); // Bigger Label
  tft->setTextDatum(TC_DATUM);
  tft->drawString("MAX RPM", 10 + cardW / 2, cardY + 15);

  // CURRENT RPM Card (Right)
  tft->fillRoundRect(20 + cardW, cardY, cardW, cardH, 12, 0x10A2); // Slate
  tft->setTextColor(TFT_SILVER, 0x10A2);
  tft->drawString("CURRENT", 20 + cardW + cardW / 2, cardY + 15);
}

void RpmSensorScreen::updateValues() {
  TFT_eSPI *tft = _ui->getTft();

  // Layout Constants (Must match drawScreen)
  int cardY = 80;
  int cardW = (SCREEN_WIDTH - 30) / 2;

  // Card Calculations (Mirror drawScreen)
  // Use Font 7 (Huge 7-Segment)
  tft->setTextFont(7);
  tft->setTextSize(1); // Size 1 is already big for Font 7
  tft->setTextDatum(MC_DATUM);

  // MAX Value
  char buf[10];
  sprintf(buf, "%05d", _maxRpm);
  tft->setTextColor(TFT_ORANGE, 0x18E3);
  tft->setTextPadding(cardW - 10);
  tft->drawString(buf, 10 + cardW / 2, cardY + 80); // Centered in card

  // CURRENT Value
  sprintf(buf, "%05d", _currentRpm);
  tft->setTextColor(TFT_CYAN, 0x10A2);
  tft->setTextPadding(cardW - 10);
  tft->drawString(buf, 20 + cardW + cardW / 2, cardY + 80); // Centered in card

  tft->setTextPadding(0);
}
