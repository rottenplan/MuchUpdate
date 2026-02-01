#include "GnssLogScreen.h"
#include "../../core/GPSManager.h"
#include "../fonts/Org_01.h"

extern GPSManager gpsManager;
extern TFT_eSPI tft; // Or access via _ui->getTft()

void GnssLogScreen::onShow() {
  // Clear full screen
  TFT_eSPI *tft = _ui->getTft();
  _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                            SCREEN_HEIGHT - STATUS_BAR_HEIGHT); // COLOR_BG
  _ui->drawStatusBar(true);

  _lines.clear();
  _paused = false;
  _lastDataTime = 0;
  _lastStatusConnected = false;
  _buffer = "";
  _needsRedraw = true;

  // --- 1. HEADER ---
  // --- 1. HEADER ---
  // Status Bar Height is typically 25. Move header below it.
  int headerY = STATUS_BAR_HEIGHT;
  int lineY = headerY + 25; // Line under title

  tft->drawFastHLine(0, lineY, SCREEN_WIDTH, COLOR_SECONDARY);

  // Title
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->setTextDatum(MC_DATUM); // Middle Center
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->drawString("GPS LOG", SCREEN_WIDTH / 2,
                  headerY + 12); // Centered between Status Bar and Line

  // Back Button (Blue Triangle) - Bottom Left
  tft->fillTriangle(10, SCREEN_HEIGHT - 25, 22, SCREEN_HEIGHT - 31, 22,
                    SCREEN_HEIGHT - 19, TFT_BLUE);

  // --- 2. CHECKBOXES (Top area) ---
  drawCheckboxes();

  // --- 3. LOG CARD ---

  // Widened for 480x320: W=440 (Centered: X=20)
  tft->fillRoundRect(20, 95, 440, 180, 8, 0x18E3);
  tft->drawRoundRect(20, 95, 440, 180, 8, TFT_DARKGREY);

  // Register Callback
  gpsManager.setRawDataCallback([this](uint8_t c) {
    if (_paused)
      return;

    _lastDataTime = millis(); // Update timestamp for connection status

    // Ignore CR, handle LF as line break
    if (c == '\r')
      return;

    if (c == '\n') {
      if (_buffer.length() > 0) {
        // Truncate to fit box width (440px -> ~70 chars at font size 1)
        if (_buffer.length() > 70) {
          _buffer = _buffer.substring(0, 70);
        }
        _lines.push_back(_buffer);
        if (_lines.size() > 13) // Max 13 lines with 12px spacing for H=180
          _lines.erase(_lines.begin());
        _buffer = "";
        _needsRedraw = true; // Trigger redraw
      }
    } else {
      // Only accept printable ASCII characters (32-126)
      if (_buffer.length() < 70 && c >= 32 && c <= 126)
        _buffer += (char)c; // Prevent infinite growth
    }
  });
}

void GnssLogScreen::onHide() {
  gpsManager.setRawDataCallback(nullptr);
  _lines.clear();
  _buffer = "";
}

void GnssLogScreen::drawCheckboxes() {
  TFT_eSPI *tft = _ui->getTft();

  // Reset Font
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextSize(1);
  tft->setTextDatum(TL_DATUM);
  tft->setTextColor(TFT_SILVER, TFT_BLACK); // Silver on Black

  int yTop = 55; // Lowered to fit under new header

  // Helper to draw a "checkbox" style item
  auto drawCheckItem = [&](int x, String label, bool checked) {
    // Checkbox square (Larger 16x16)
    tft->drawRect(x, yTop, 16, 16, TFT_DARKGREY);

    if (checked) {
      tft->fillRect(x + 3, yTop + 3, 10, 10, TFT_GREEN); // Green check
    } else {
      tft->fillRect(x + 3, yTop + 3, 10, 10, TFT_BLACK); // Uncheck
    }
    tft->drawString(label, x + 20, yTop + 1); // Adjusted text pos
  };

  uint8_t m = gpsManager.getGnssMode();
  // Mapping logic
  bool checkGPS = true; // Always on
  bool checkGLO = (m == 0 || m == 1 || m == 2 || m == 7);
  bool checkSBAS = (m == 0 || m == 1 || m == 2 || m == 3 || m == 4 || m == 6);
  bool checkGAL = (m == 0 || m == 2 || m == 3);

  // Spacing: Spread across 480px
  drawCheckItem(20, "GPS+", checkGPS);
  drawCheckItem(130, "GLONASS", checkGLO);
  drawCheckItem(260, "SBAS", checkSBAS);
  drawCheckItem(370, "GALILEO", checkGAL);
}

void GnssLogScreen::update() {
  // Touch Handling
  UIManager::TouchPoint p = _ui->getTouchPoint();
  if (p.x != -1) {
    if (p.y > 200 && p.x < 60) {
      // Back (Triangle area)
      static unsigned long lastBackTap = 0;
      if (millis() - lastBackTap < 500) {
        gpsManager.setRawDataCallback(nullptr); // Disable callback
        _ui->switchScreen(SCREEN_GPS_STATUS);
        lastBackTap = 0;
      } else {
        lastBackTap = millis();
      }
      return;
    }

    // Toggle Pause (Tap on log area)
    if (p.x > 20 && p.x < 460 && p.y > 90 && p.y < 280) {
      _paused = !_paused;
      _needsRedraw = true;
    }
  }

  if (p.y < 90) { // Expanded touch area from 60 to 90
    // Checkbox Area
    int tapX = p.x;
    // 10, 90, 170, 250. Width ~60?

    uint8_t m = gpsManager.getGnssMode();
    bool glo = (m == 0 || m == 1 || m == 2 || m == 7);
    bool sbas = (m == 0 || m == 1 || m == 2 || m == 3 || m == 4 || m == 6);
    bool gal = (m == 0 || m == 2 || m == 3);

    if (tapX > 10 && tapX < 120) { // GPS Area
      // GNSS always on
    } else if (tapX > 130 && tapX < 250) {
      glo = !glo;
    } else if (tapX > 260 && tapX < 360) {
      sbas = !sbas;
    } else if (tapX > 370 && tapX < 480) {
      gal = !gal;
    }

    // Resolve new mode
    uint8_t newMode = 1; // Default fallback

    if (glo && gal && sbas)
      newMode = 0; // All
    else if (glo && !gal && sbas)
      newMode = 1; // GPS+GLO+SBAS
    else if (glo && gal && !sbas)
      newMode = 2; // GPS+GAL+GLO
    else if (!glo && gal && sbas)
      newMode = 3; // GPS+GAL+SBAS
    else if (!glo && !gal && sbas)
      newMode = 4; // GPS+SBAS
    else if (!glo && !gal && !sbas)
      newMode = 5; // GPS Only
    else if (glo && !gal && !sbas)
      newMode = 7; // GPS+GLO

    if (newMode != m) {
      gpsManager.setGnssMode(newMode);
      // _ui->switchScreen(SCREEN_GNSS_LOG); // Reload to redraw checks <--
      // REMOVED
      drawCheckboxes(); // Update only checkboxes
      return;
    }
  }

  // Status Indicator - REMOVED (redundant)
  /*
  bool connected = (millis() - _lastDataTime < 1000) && (_lastDataTime != 0);
  if (connected != _lastStatusConnected) {
    _lastStatusConnected = connected;
    _needsRedraw = true;

    // Update status immediately
    TFT_eSPI *tft = _ui->getTft();
    tft->setTextSize(1);
    tft->setTextDatum(TR_DATUM);
    // Draw status below log box (log ends at Y=275, add margin)
    if (connected) {
      tft->setTextColor(TFT_GREEN, TFT_BLACK);
      tft->drawString("GPS: CONNECTED  ", SCREEN_WIDTH - 10, 280);
    } else {
      tft->setTextColor(TFT_RED, TFT_BLACK);
      tft->drawString("GPS: NO DATA    ", SCREEN_WIDTH - 10, 280);
    }
  }
  */

  static unsigned long lastDrawTime = 0;
  if (_needsRedraw && (millis() - lastDrawTime > 300)) { // Throttle to ~3 FPS
    drawLines();
    _needsRedraw = false;
    lastDrawTime = millis();
  }
}

void GnssLogScreen::drawLines() {
  TFT_eSPI *tft = _ui->getTft();

  // Revert to Standard Font
  tft->setTextFont(1);
  tft->setTextSize(1);
  tft->setFreeFont(NULL);

  // Green text for logs
  tft->setTextColor(TFT_GREEN, 0x18E3);
  tft->setTextDatum(TL_DATUM);
  tft->setTextPadding(420); // Full width padding

  // Margin 10px from X=20 -> X=30. Y=102 (better vertical centering)
  int innerX = 30;
  int innerY = 102;

  int y = innerY;
  for (const auto &line : _lines) {
    tft->drawString(line, innerX, y);
    y += 12; // Improved spacing (13 lines * 12px = 156px < 180px)
  }
}
