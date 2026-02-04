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

void RpmSensorScreen::onShow() {
  // Respect current setting instead of forcing ON
  _currentRpm = 0;
  _maxRpm = 0;
  _currentLvl = 0;
  _currentSpeed = 0;
  _graphIndex = 0;
  _lastUpdate = 0;

  // Calculate RPM
  // _rpmPulses = 0;
  // _lastRpmCalcTime = millis();

  // Setup Interrupt for Inductive Clamp
  // MOVED TO GLOBAL GPS MANAGER
  // if (PIN_RPM_INPUT >= 0) {
  //   pinMode(PIN_RPM_INPUT, INPUT);
  //   attachInterrupt(digitalPinToInterrupt(PIN_RPM_INPUT), onPulse, FALLING);
  // }

  // Clear graph history
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    _graphHistory[i] = 0;
  }

  // Create Sprite for Flicker-Free Graph (8-bit for RAM safety)
  if (_graphSprite == nullptr) {
    _graphSprite = new TFT_eSprite(_ui->getTft());
    _graphSprite->setColorDepth(8); // Halves RAM usage
    if (_graphSprite->createSprite(GRAPH_WIDTH, GRAPH_HEIGHT)) {
      Serial.println("RPM Graph Sprite Created (8-bit)");
    } else {
      Serial.println("RPM Sprite FAILED even with 8-bit!");
    }
  }

  drawScreen();
}

void RpmSensorScreen::onHide() {
  // Don't disable globally; let the setting persist
  if (_graphSprite != nullptr) {
    _graphSprite->deleteSprite();
    delete _graphSprite;
    _graphSprite = nullptr;
  }
  // Manual Wipe to prevent ghosting
  TFT_eSPI *tft = _ui->getTft();
  tft->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                SCREEN_HEIGHT - STATUS_BAR_HEIGHT, TFT_BLACK);
}

void RpmSensorScreen::update() {
  // 1. Back Button (Standardized 100x80)
  UIManager::TouchPoint p = _ui->getTouchPoint();
  if (p.x < 80 && p.y > 240) {
    _ui->switchScreen(SCREEN_MENU);
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
    int maxScale = 12000;
    _currentLvl = map(constrain(_currentRpm, 0, maxScale), 0, maxScale, 0, 100);

    // Calculate Speed Level (0-150 km/h scaling)
    // Update Graph Buffers
    _graphHistory[_graphIndex] = _currentLvl;

    _graphIndex = (_graphIndex + 1) % GRAPH_WIDTH;

    // Redraw Dynamic Parts
    updateValues();
    drawGraphLine();
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

  // Back Button (Blue Triangle) - Bottom Left
  // Centered vertically with progress bar (bar y=294, h=14, center=301)
  tft->fillTriangle(10, 301 - 6, 22, 301 - 12, 22, 301, TFT_BLUE);

  // --- INFO CARDS ---
  int cardY = 55;
  int cardH = 65;                      // Slightly taller
  int cardW = (SCREEN_WIDTH - 25) / 2; // Widened for 480

  // MAX RPM Card (Left)
  tft->fillRoundRect(10, cardY, cardW, cardH, 8, 0x18E3); // Charcoal
  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->setTextSize(1);
  tft->setTextDatum(TL_DATUM);
  tft->drawString("MAX RPM", 20, cardY + 5);

  // CURRENT RPM Card (Right)
  tft->fillRoundRect(15 + cardW, cardY, cardW, cardH, 8, 0x10A2); // Slate
  tft->setTextColor(TFT_SILVER, 0x10A2);
  tft->drawString("CURRENT", 25 + cardW, cardY + 5);

  // --- GRAPH AREA ---
  drawGraphGrid();

  // --- BOTTOM BAR ---
  // Background container at the very bottom
  // Shifted right (x=40) to avoid Back Button (x=10-22)
  tft->fillRoundRect(40, 292, SCREEN_WIDTH - 50, 18, 4, 0x18E3);

  // Initial draw of graph (grid)
  drawGraphLine();
}

void RpmSensorScreen::updateValues() {
  TFT_eSPI *tft = _ui->getTft();

  // Card Calculations (Mirror drawScreen)
  int cardY = 55;
  int cardH = 65;
  int cardW = (SCREEN_WIDTH - 25) / 2;

  // Use Font 4 (LCD/Digital style, 26px)
  tft->setTextFont(4);
  tft->setTextSize(1);
  tft->setTextDatum(MC_DATUM);

  // MAX Value
  char buf[10];
  sprintf(buf, "%05d", _maxRpm);
  tft->setTextColor(TFT_ORANGE, 0x18E3);
  // Manual clear or just overwrite with background color text (Charcoal)
  // Font 4 is monospaced-ish but padding helps
  tft->setTextPadding(cardW - 10);
  tft->drawString(buf, 10 + cardW / 2, cardY + 38);

  // CURRENT Value
  sprintf(buf, "%05d", _currentRpm);
  tft->setTextColor(TFT_CYAN, 0x10A2);
  tft->setTextPadding(cardW - 10);
  tft->drawString(buf, 15 + cardW + cardW / 2, cardY + 38);

  tft->setTextPadding(0); // Reset padding

  // Update Bottom Bar
  int barStartX = 42; // Avoid back button
  int barMaxWidth = SCREEN_WIDTH - barStartX - 10;
  int barW = map(_currentLvl, 0, 100, 0, barMaxWidth);
  if (barW < 0)
    barW = 0;
  if (barW > barMaxWidth)
    barW = barMaxWidth;

  int barY = 294;
  int barH = 14;
  int barX = barStartX;

  // Draw Bar (Fill Green for active, Charcoal for empty)
  if (barW > 0)
    tft->fillRect(barX, barY, barW, barH, COLOR_GREEN);
  if (barW < barMaxWidth)
    tft->fillRect(barX + barW, barY, barMaxWidth - barW, barH, 0x18E3);
}

void RpmSensorScreen::drawGraphGrid() {
  TFT_eSPI *tft = _ui->getTft();
  int gY = 132; // Centered with subtle gaps (10px top, 5px bottom)
  int gH = GRAPH_HEIGHT;
  int gW = GRAPH_WIDTH;

  // Draw Graph Container Frame
  tft->drawRoundRect(10, gY - 2, SCREEN_WIDTH - 20, gH + 4, 4, TFT_DARKGREY);

  // Axis Labels
  tft->setTextFont(1);
  tft->setTextSize(1);
  tft->setTextColor(TFT_SILVER);

  // Left Axis (RPM) - Aligned with start, mid, end of sprite
  tft->setTextDatum(MR_DATUM);
  tft->drawString("12k", 8, gY);
  tft->drawString("6k", 8, gY + gH / 2);
  tft->drawString("0", 8, gY + gH);
}

void RpmSensorScreen::drawGraphLine() {
  if (_graphSprite == nullptr)
    return;

  // 1. Clear Sprite Background
  _graphSprite->fillSprite(COLOR_BG); // Standard black from config.h

  // 2. Redraw Grid on Sprite
  int sW = GRAPH_WIDTH;
  int sH = GRAPH_HEIGHT;

  // Vertical Grid (Perfect 6 divisions: 450/6 = 75)
  for (int x = 75; x < sW; x += 75) {
    _graphSprite->drawFastVLine(x, 0, sH, 0x39E7);
  }

  // Horizontal Grid (Perfect 8 divisions: 154/8 = 19.25? No, use midpoint
  // symmetry) Let's use 4 divisions (25%, 50%, 75%) for clarity
  for (int i = 1; i < 4; i++) {
    int y = sH * i / 4;
    _graphSprite->drawFastHLine(0, y, sW, 0x39E7);
  }

  // 3. Draw Lines
  int start = _graphIndex;
  for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
    int idx1 = (start + i) % GRAPH_WIDTH;
    int idx2 = (start + i + 1) % GRAPH_WIDTH;

    // RPM (Red)
    int yR1 = (sH - 1) - (int)((_graphHistory[idx1] / 100.0) * (sH - 1));
    int yR2 = (sH - 1) - (int)((_graphHistory[idx2] / 100.0) * (sH - 1));

    // Clamp
    if (yR1 < 0)
      yR1 = 0;
    if (yR2 < 0)
      yR2 = 0;

    _graphSprite->drawLine(i, yR1, i + 1, yR2, TFT_RED);
  }

  // 4. Push Sprite to Screen (Coordinate matches gY)
  // Inside the frame at 132 to 285.
  _graphSprite->pushSprite(11, 132);
}
