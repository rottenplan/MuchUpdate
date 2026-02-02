#include "GpsStatusScreen.h"
#include "../../core/GPSManager.h"
#include "../fonts/Org_01.h"

extern GPSManager gpsManager;

// Safe Area Offset (Status Bar Height)
#define TOP_OFFSET 25

void GpsStatusScreen::onShow() {
  TFT_eSPI *tft = _ui->getTft();
  // Clear entire screen to black to prevent artifacts
  tft->fillScreen(TFT_BLACK);

  // Force Status Bar Redraw to ensure no artifacts at top
  _ui->drawStatusBar(true);

  // Draw Top-Left Date/Time Box Frame (Optional, or just text)
  // Let's keep it clean black background.

  // Initial values reset
  _lastSats = -1;
  _lastHz = -1;
  _lastLat = -999;
  _lastLng = -999;
  _lastHdop = -1.0;   // Reset HDOP tracker
  _lastFixed = false; // Force re-draw of radar rings

  // Clear any potential leftover artifacts manually if needed,
  // but fillScreen should handle it. The 'white lines' might be status bar
  // related, but let's ensure clean state here.

  // Back Button (Blue Triangle) - Bottom Left
  tft->fillTriangle(10, SCREEN_HEIGHT - 25, 22, SCREEN_HEIGHT - 31, 22,
                    SCREEN_HEIGHT - 19, TFT_BLUE);

  // Log Button (Orange Label) - Bottom Right
  tft->fillRoundRect(SCREEN_WIDTH - 60, SCREEN_HEIGHT - 35, 50, 25, 4,
                     TFT_ORANGE);
  tft->setTextColor(TFT_BLACK, TFT_ORANGE);
  tft->setTextDatum(MC_DATUM);
  tft->setTextFont(1);
  tft->drawString("LOG", SCREEN_WIDTH - 35, SCREEN_HEIGHT - 23);

  drawStatus();
}

void GpsStatusScreen::update() {
  // Touch Handling
  UIManager::TouchPoint p = _ui->getTouchPoint();
  if (p.x != -1) {
    // Back Button Area
    // Back Button Area (Bottom Left)
    if (p.x < 80 && p.y > SCREEN_HEIGHT - 60) {
      static unsigned long lastBackTap = 0;
      if (millis() - lastBackTap < 500) {
        _ui->switchScreen(SCREEN_MENU);
        lastBackTap = 0;
      } else {
        lastBackTap = millis();
      }
      return;
    }

    // Check for "LOG" Button Area (Bottom Right)
    if (p.x > SCREEN_WIDTH - 80 && p.y > SCREEN_HEIGHT - 60) {
      _ui->switchScreen(SCREEN_GNSS_LOG);
      return;
    }

    // Check for Double Tap on body (Keep as backup)
    unsigned long now = millis();
    if (now - _lastTapTime < 500) {
      // Double Tap!
      _ui->switchScreen(SCREEN_GNSS_LOG);
      _lastTapTime = 0; // Reset
      return;
    }
    _lastTapTime = now;
  }

  drawStatus();
}

// Safe Area Offset (Status Bar Height)
#define TOP_OFFSET 25

void GpsStatusScreen::drawStatus() {
  TFT_eSPI *tft = _ui->getTft();

  int sats = gpsManager.getSatellites();
  double hdop = gpsManager.getHDOP();
  int hz = gpsManager.getUpdateRate();
  double lat = gpsManager.getLatitude();
  double lng = gpsManager.getLongitude();

  // Colors
  uint16_t COLOR_CARD = 0x18E3;
  uint16_t COLOR_LABEL = TFT_SILVER;
  uint16_t COLOR_VALUE = TFT_WHITE;

  // --- 1. DATE/TIME & LAT/LON CARD (Left Side) ---
  // If first run or interval, redraw card base
  static unsigned long lastUpdate = 0;
  bool forceRedraw = (_lastSats == -1);

  if (forceRedraw || millis() - lastUpdate > 1000) {
    lastUpdate = millis();
    int cardX = 10;
    int cardY = TOP_OFFSET + 10;
    int cardW = 220; // Widened from 160
    int cardH = 145; // Slightly taller

    // Draw Card Background (only if needed or clean update)
    if (forceRedraw) {
      tft->fillRoundRect(cardX, cardY, cardW, cardH, 8, COLOR_CARD); // Charcoal
      tft->drawRoundRect(cardX, cardY, cardW, cardH, 8, TFT_DARKGREY);

      // Labels
      tft->setTextColor(COLOR_LABEL, COLOR_CARD);
      tft->setTextDatum(TL_DATUM);
      tft->setFreeFont(&Org_01);
      tft->setTextSize(1);
      tft->drawString("LOCATION", cardX + 10, cardY + 5);

      tft->drawLine(cardX + 5, cardY + 20, cardX + cardW - 5, cardY + 20,
                    TFT_DARKGREY);
    }

    // Dynamic Values - Time
    int h, m, s, d, mo, y;
    gpsManager.getLocalTime(h, m, s, d, mo, y);

    char dateBuf[32];
    sprintf(dateBuf, "%02d/%02d/%04d", d, mo, y);
    char timeBuf[16];
    sprintf(timeBuf, "%02d:%02d:%02d", h, m, s);

    tft->setTextColor(TFT_SKYBLUE, COLOR_CARD);
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1); // Force reset size
    tft->setTextFont(2);
    tft->drawString(dateBuf, cardX + 10, cardY + 25);

    tft->setTextSize(1); // Force reset size
    tft->setTextFont(4); // Big Time
    tft->setTextColor(TFT_WHITE, COLOR_CARD);
    tft->drawString(timeBuf, cardX + 10, cardY + 42);

    // Dynamic Values - Lat/Lon
    // LAT
    tft->setTextColor(COLOR_LABEL, COLOR_CARD);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(1);
    tft->drawString("LAT", cardX + 10, cardY + 75);

    tft->setTextColor(TFT_WHITE, COLOR_CARD);
    tft->setTextSize(1); // Reset
    tft->setTextFont(2);
    tft->drawString(String(lat, 6), cardX + 40, cardY + 72);

    // LON
    tft->setTextColor(COLOR_LABEL, COLOR_CARD);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(1);
    tft->drawString("LON", cardX + 10, cardY + 95);

    tft->setTextColor(TFT_WHITE, COLOR_CARD);
    tft->setTextSize(1); // Reset
    tft->setTextFont(2);
    tft->drawString(String(lng, 6), cardX + 40, cardY + 92);

    // Alt / Heading tiny
    int alt = (int)gpsManager.getAltitude();
    int head = (int)gpsManager.getHeading();
    tft->setTextColor(TFT_ORANGE, COLOR_CARD);
    tft->setFreeFont(&Org_01);
    tft->drawString("ALT: " + String(alt) + "m", cardX + 10, cardY + 115);
    tft->drawString("DIR: " + String(head), cardX + 120,
                    cardY + 115); // Spread out
  }

  // --- 2. SATELLITE INFO (Bottom Card) ---
  if (forceRedraw || sats != _lastSats || hz != _lastHz) {
    int cardX = 10;
    int cardY = TOP_OFFSET + 165; // Balanced with taller radar
    int cardW = SCREEN_WIDTH - 20;
    int cardH = 70; // Increased height

    if (forceRedraw) {
      tft->fillRoundRect(cardX, cardY, cardW, cardH, 8, 0x10A2); // Slate
    }

    // Clear text area inside card
    tft->fillRoundRect(cardX + 2, cardY + 25, cardW - 4, 33, 0, 0x10A2);

    tft->setTextColor(TFT_SILVER, 0x10A2);
    tft->setTextDatum(TL_DATUM);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(1);

    if (forceRedraw) {
      tft->drawString("STATUS", cardX + 10, cardY + 5);
      // Labels Centered with Values
      tft->setTextDatum(MC_DATUM);
      tft->drawString("SATS", cardX + 70, cardY + 18);
      tft->drawString("Hz", cardX + 160, cardY + 18);
      tft->drawString("HDOP", cardX + 250, cardY + 18);
    }

    // Sat Count (Values moved DOWN)
    int valY = cardY + 42;

    tft->setTextColor(TFT_GREEN, 0x10A2);
    tft->setTextSize(1); // Force reset size
    tft->setTextFont(4);
    tft->setTextDatum(MC_DATUM);
    tft->drawString(String(sats), cardX + 70, valY);

    // Hz
    tft->setTextColor(TFT_CYAN, 0x10A2);
    tft->setTextSize(1); // Force reset size
    tft->setTextFont(4);
    tft->drawString(String(hz), cardX + 160, valY);

    // HDOP
    tft->setTextColor(TFT_YELLOW, 0x10A2);
    tft->setTextSize(1); // Force reset size
    tft->setTextFont(4);
    tft->drawString(String(hdop, 1), cardX + 250, valY);

    // Fix Quality
    String fixStr = gpsManager.isFixed() ? "3D FIX" : "NO FIX";
    uint16_t fixColor = gpsManager.isFixed() ? TFT_GREEN : TFT_RED;
    tft->setTextColor(fixColor, 0x10A2);
    tft->setTextSize(1); // Reset
    tft->setTextFont(2);
    tft->setTextDatum(MR_DATUM);
    tft->drawString(fixStr, cardX + cardW - 10, valY);

    _lastSats = sats;
    _lastHz = hz;
  }

  // --- 3. RADAR (Right Side) ---
  int radarX = 250;
  int radarY = TOP_OFFSET + 10;
  int radarW = SCREEN_WIDTH - radarX - 10;
  int radarH = 145;
  int cX = radarX + radarW / 2;
  int cY = radarY + radarH / 2;
  int r = 65; // Enlarged

  if (forceRedraw) {
    // Container
    tft->fillRoundRect(radarX, radarY, radarW, radarH, 8,
                       TFT_BLACK); // Keep Black for contrast
    tft->drawRoundRect(radarX, radarY, radarW, radarH, 8, TFT_DARKGREY);

    // Circles
    tft->drawCircle(cX, cY, r, TFT_DARKGREY);
    tft->drawCircle(cX, cY, r * 0.6, TFT_DARKGREY);
    tft->drawCircle(cX, cY, r * 0.2, TFT_DARKGREY);

    // Crosshair
    tft->drawLine(cX - r, cY, cX + r, cY, TFT_DARKGREY);
    tft->drawLine(cX, cY - r, cX, cY + r, TFT_DARKGREY);

    // Labels
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(1);
    tft->drawString("N", cX, cY - r - 5);
  }

  // --- DRAW SATELLITES (Dots) ---
  // Redraw every time (clean radar first if needed, but for now just additive)
  // Ideally we should clear the radar circle area if we want to animate,
  // but let's stick to forceRedraw or Periodic updates.
  // Actually, to prevent ghosting, we should clear the radar area if we have
  // changes. For now, let's rely on forceRedraw trigger from update() changes.

  if (forceRedraw || sats != _lastSats || hz != _lastHz) {
    // Clear Radar Area (Inside)
    tft->fillCircle(cX, cY, r - 1, TFT_BLACK);
    // Re-draw crosshair
    tft->drawLine(cX - r, cY, cX + r, cY, TFT_DARKGREY);
    tft->drawLine(cX, cY - r, cX, cY + r, TFT_DARKGREY);

    std::vector<SatelliteInfo> satData = gpsManager.getSatellitesData();
    for (const auto &sat : satData) {
      if (sat.elevation < 0)
        continue;

      // Polar to Cartesian
      // Radius: 0 (Center/90deg) to r (Edge/0deg)
      float rad = r * (1.0 - (sat.elevation / 90.0));

      // Angle: Azimuth 0 is North (Up). Math 0 is East (Right).
      // Theta = (Azimuth - 90) * DEG_TO_RAD
      float angle = (sat.azimuth - 90) * DEG_TO_RAD;

      int sx = cX + (rad * cos(angle));
      int sy = cY + (rad * sin(angle));

      // Color by SNR (0-99)
      uint16_t dotColor = TFT_RED;
      if (sat.snr > 35)
        dotColor = TFT_GREEN;
      else if (sat.snr > 25)
        dotColor = TFT_YELLOW;
      else if (sat.snr > 15)
        dotColor = TFT_ORANGE;

      tft->fillCircle(sx, sy, 2, dotColor);
    }
  }

  if (!_lastFixed) {
    // Logic for satellites if we had them or just static crosshair
    _lastFixed = true;
  }

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
}
