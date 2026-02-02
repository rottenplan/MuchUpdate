#include "SpeedometerScreen.h"
#include "../../config.h"
#include "../../core/GPSManager.h"
#include "../../core/IMUManager.h"
#include "../fonts/Org_01.h"
#include <Preferences.h>

extern GPSManager gpsManager;

void SpeedometerScreen::onShow() {
  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(_ui->getBackgroundColor()); // Latar belakang gelap

  _lastSpeed = -1;
  _lastRPM = -1;
  _lastTrip = -1;
  _lastTime = "";
  _lastGear = -1;
  _lastBat = -1;
  _lastRoll = 0;
  _lastAccY = 0;
  _maxSpeed = 0;
  _maxRPM = 0;
  _lastSats = -1;

  // SETUP RPM SENSOR
  // MOVED TO GPSManager for global access

  drawDashboard(true);
}

void SpeedometerScreen::update() {
  // 1. Tombol Kembali
  UIManager::TouchPoint p = _ui->getTouchPoint();
  if (p.x != -1 && p.x < 60 && p.y < 60) {
    static unsigned long lastBackTap = 0;
    if (millis() - lastBackTap < 500) {
      if (PIN_RPM_INPUT >= 0) {
        // detachInterrupt(digitalPinToInterrupt(PIN_RPM_INPUT)); // STOP SENSOR
        // - NO, IT'S GLOBAL NOW
      }
      _ui->switchScreen(SCREEN_MENU);
      lastBackTap = 0;
    } else {
      lastBackTap = millis();
    }
    return;
  }

  // 2. Pembaruan Data GPS
  float speed = gpsManager.getSpeedKmph();
  float trip = gpsManager.getTotalTrip();

  // Waktu Nyata dari GPS
  int h, m, s, d, mo, y;
  gpsManager.getLocalTime(h, m, s, d, mo, y);
  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", h, m);
  String timeStr = String(timeBuf);

  // 3. HITUNG RPM (Real Time)
  // NOW HANDLED BY GPS MANAGER GLOBALLY
  int rpm = gpsManager.getRPM();

  // Push to Web API not needed as GPSManager has it already
  // gpsManager.setRPM(rpm);

  // Placeholder lainnya
  int gear = 0;
  int bat = 100;

  // Cek satuan (km/h atau mph)
  Preferences prefs;
  prefs.begin("laptimer", true);
  bool useMph = prefs.getInt("units", 0) == 1; // 0=km/h, 1=mph
  prefs.end();

  if (useMph) {
    speed *= 0.621371;
    trip *= 0.621371;
  }

  // Cek apakah ada perubahan data untuk digambar ulang
  int sats = gpsManager.getSatellites();
  if (rpm > _maxRPM)
    _maxRPM = rpm;
  if (speed > _maxSpeed)
    _maxSpeed = speed;

  // IMU Data
  float roll = imuManager.getAngleX(); // Assuming X is roll based on mounting
  float accY = imuManager.getAccY();   // Lateral G

  if (speed != _lastSpeed || rpm != _lastRPM || useMph != _lastUnits ||
      timeStr != _lastTime || trip != _lastTrip || sats != _lastSats ||
      abs(roll - _lastRoll) > 1.0f || abs(accY - _lastAccY) > 0.05f) {
    _lastSpeed = speed;
    _lastRPM = rpm;
    _lastUnits = useMph;
    _lastTime = timeStr;
    _lastTrip = trip;
    _lastSats = sats;
    _lastRoll = roll;
    _lastAccY = accY;
    drawDashboard(false);
  }
}

// Pembantu untuk menggambar segmen jajargenjang
void drawSegment(TFT_eSPI *tft, int x, int y, int w, int h, int angleOffset,
                 uint16_t color) {
  tft->fillTriangle(x, y + h, x + w, y + h, x + angleOffset, y, color);
  tft->fillTriangle(x + w, y + h, x + w + angleOffset, y, x + angleOffset, y,
                    color);
}

// --- LAYOUT CORRECTIONS ---
void SpeedometerScreen::drawDashboard(bool force) {
  TFT_eSPI *tft = _ui->getTft();

  // --- THEME COLORS ---
  uint16_t colPrimary = COLOR_PRIMARY;
  uint16_t colText = _ui->getTextColor();
  uint16_t colBg = _ui->getBackgroundColor();
  uint16_t colCardBorder = TFT_DARKGREY;

  // Layout Constants (Optimized for 480x320)
  int cardY = 25; // Moved UP (Status bar is 20)
  int cardH = 50; // Taller to fit content
  int cardW = 130;
  int gap = 15;
  int startX = 25; // 25 + 130 + 15 + 130 + 15 + 130 + 25 = 470

  // Y Positions
  int valY = cardY + 30; // Center values in card
  int speedY = 140;      // Moved DOWN (was 115) to avoid overlap
  int unitY = speedY + 50;
  int tripLabelY = unitY + 30;
  int tripValY = tripLabelY + 20;

  if (force) {
    // Clear Content
    _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                              SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
    _ui->drawStatusBar(true);

    // --- TOP DATA CARDS ---
    tft->drawRoundRect(startX, cardY, cardW, cardH, 6, colCardBorder);
    tft->drawRoundRect(startX + cardW + gap, cardY, cardW, cardH, 6,
                       colCardBorder);
    tft->drawRoundRect(startX + (cardW + gap) * 2, cardY, cardW, cardH, 6,
                       colCardBorder);

    // --- LABELS (Inside Cards, Top) ---
    tft->setFreeFont(&Org_01);
    tft->setTextSize(1);
    tft->setTextColor(TFT_SILVER, colBg);
    tft->setTextDatum(TC_DATUM);

    int labelY = cardY + 4;
    tft->drawString("MAX RPM", startX + (cardW / 2), labelY);
    tft->drawString("MAX SPEED", startX + cardW + gap + (cardW / 2), labelY);
    tft->drawString("SATELLITES", startX + (cardW + gap) * 2 + (cardW / 2),
                    labelY);

    // --- UNIT ---
    tft->setTextFont(2);
    tft->setTextSize(1);
    tft->setTextColor(colPrimary, colBg);
    tft->drawCentreString("km/h", SCREEN_WIDTH / 2, unitY, 1);

    // --- TRIP METER CARD ---
    int tripBoxW = 200;
    int tripBoxH = 50;
    int tripBoxX = (SCREEN_WIDTH - tripBoxW) / 2;
    int tripBoxY = unitY + 25; // Spacing below unit

    tft->drawRoundRect(tripBoxX, tripBoxY, tripBoxW, tripBoxH, 6,
                       colCardBorder);

    // Label
    tft->setFreeFont(&Org_01);
    tft->setTextSize(1);
    tft->setTextColor(TFT_SILVER, colBg);
    tft->setTextDatum(TC_DATUM);
    tft->drawString("TRIP DISTANCE", SCREEN_WIDTH / 2, tripBoxY + 5);

    // --- RPM BAR OUTLINE ---
    int rpmY = 290;
    int rpmH = 12;
    int rpmW = 400;
    int rpmX = (SCREEN_WIDTH - rpmW) / 2;

    tft->drawRect(rpmX - 1, rpmY - 1, rpmW + 2, rpmH + 2, TFT_DARKGREY);
    tft->setTextDatum(MR_DATUM);
    tft->drawString("RPM", rpmX - 10, rpmY + 6);

    // --- IMU Labels ---
    int imuY = 220;
    tft->setTextColor(TFT_SILVER, colBg);
    tft->setTextDatum(TC_DATUM);
    tft->drawString("ROLL", SCREEN_WIDTH / 4, imuY);
    tft->drawString("LAT G", (SCREEN_WIDTH * 3) / 4, imuY);
  }

  // --- DYNAMIC UPDATES ---
  char buf[32];

  // 1. UPDATE CARDS VALUES
  // Use Font 4 but handle padding carefully
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(colText, colBg);
  tft->setTextFont(4);
  tft->setTextSize(1);

  // Padding - Use Background Color to clear
  // To avoid box overlap, we must clamp padding or use fillRect
  int padW = cardW - 10;

  // Max RPM
  tft->setTextPadding(padW);
  sprintf(buf, "%d", _maxRPM);
  tft->drawString(buf, startX + (cardW / 2), valY);

  // Max Speed
  sprintf(buf, "%.0f", _maxSpeed);
  tft->drawString(buf, startX + cardW + gap + (cardW / 2), valY);

  // Sats
  sprintf(buf, "%d", _lastSats);
  tft->drawString(buf, startX + (cardW + gap) * 2 + (cardW / 2), valY);
  tft->setTextPadding(0);

  // 2. MAIN SPEED
  tft->setTextFont(7); // 7-Segment
  tft->setTextSize(2);
  tft->setTextColor(colPrimary, colBg);
  tft->setTextDatum(MC_DATUM);

  // Use fillRect to clear previous speed strictly within the speed area
  // to avoid clearing the cards above if font is huge
  // Font 7 Size 2 is approx 100px high
  // Y=140. Top=90. Bottom=190.
  // Previous overlap was hitting Y=80 (Card bottom)

  // Only redraw if changed? The caller handles that check usually,
  // but force means everything. This 'if(!force)' block is for updates.
  // Actually, the caller logic:
  // if (speed != _lastSpeed ... ) drawDashboard(false);
  // So we are safe to draw.

  tft->setTextPadding(SCREEN_WIDTH);
  sprintf(buf, "%.0f", _lastSpeed);
  tft->drawString(buf, SCREEN_WIDTH / 2, speedY);
  tft->setTextPadding(0);

  // 3. TRIP VALUE
  // Recalculate Y based on new Box logic
  int tripBoxY = unitY + 25;
  tripValY = tripBoxY + 28; // Lower half of box

  tft->setTextFont(4);
  tft->setTextSize(1);
  tft->setTextColor(colText, colBg);

  tft->setTextPadding(180); // Clear width of box interior
  sprintf(buf, "%04.1f", _lastTrip);
  tft->drawString(buf, SCREEN_WIDTH / 2, tripValY);
  tft->setTextPadding(0);

  // 4. RPM BAR & VALUE
  int rpmY = 290;
  int rpmH = 12;
  int rpmW = 400;
  int rpmX = (SCREEN_WIDTH - rpmW) / 2;

  // Draw Fill
  int fillW = map(constrain(_lastRPM, 0, 9000), 0, 9000, 0, rpmW);
  if (fillW > 0)
    tft->fillRect(rpmX, rpmY, fillW, rpmH, colPrimary);
  if (fillW < rpmW)
    tft->fillRect(rpmX + fillW, rpmY, rpmW - fillW, rpmH, colBg);

  // RPM Value
  tft->setTextFont(2);
  tft->setTextSize(1);
  tft->setTextColor(colText, colBg);
  tft->setTextDatum(ML_DATUM);
  tft->setTextPadding(60);
  sprintf(buf, "%d", _lastRPM);
  tft->drawString(buf, rpmX + rpmW + 10, rpmY + 6);

  // 5. IMU VALUES
  int imuValY = 245;
  tft->setTextFont(4);
  tft->setTextColor(colText, colBg);
  tft->setTextDatum(TC_DATUM);

  tft->setTextPadding(100);
  sprintf(buf, "%.0f*", _lastRoll); // * for degree symbol
  tft->drawString(buf, SCREEN_WIDTH / 4, imuValY);

  sprintf(buf, "%.2fG", _lastAccY);
  tft->drawString(buf, (SCREEN_WIDTH * 3) / 4, imuValY);
  tft->setTextPadding(0);

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
}
