#include "SpeedometerScreen.h"
#include "../../config.h"
#include "../../core/GPSManager.h"
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

  if (speed != _lastSpeed || rpm != _lastRPM || useMph != _lastUnits ||
      timeStr != _lastTime || trip != _lastTrip || sats != _lastSats) {
    _lastSpeed = speed;
    _lastRPM = rpm;
    _lastUnits = useMph;
    _lastTime = timeStr;
    _lastTrip = trip;
    _lastSats = sats;
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

void SpeedometerScreen::drawDashboard(bool force) {
  TFT_eSPI *tft = _ui->getTft();

  // --- PENGATURAN WARNA ---
  uint16_t colTheme = TFT_GREEN;                 // Warna utama (Hijau)
  uint16_t colRed = TFT_RED;                     // Warna merah untuk Top Speed
  uint16_t colWhite = _ui->getTextColor();       // Warna putih (Teks Utama)
  uint16_t colBlack = _ui->getBackgroundColor(); // Warna hitam (Latar Belakang)

  // Special case: Inverted boxes (e.g. Center Box was White BG, Black Text)
  // If Dark Mode: BG=Black, colWhite=White. CenterBox=White, Text=Black. OK.
  // If Light Mode: BG=White, colWhite=Black. CenterBox=Black (should be?),
  // Text=White. We need distinct "Contrast Text" and "Contrast BG" or just
  // strictly use logic.

  // Let's redefine for clarity in this specific dashboard which uses "Boxes"
  uint16_t colBoxBg =
      _ui->getTextColor(); // Box Background (Opposite of Main BG)
  uint16_t colBoxText =
      _ui->getBackgroundColor(); // Text inside Box (Same as Main BG)

  // --- PENGATURAN POSISI (OFFSET) ---
  int offTop = 15; // Geser Atas (Dikurangi dari 33 agar naik)
  int offBot = 10; // Geser Bawah (Dikurangi dari 28)
  // int dx = (SCREEN_WIDTH - 320) / 2; // REMOVED: Utilizing full screen
  // (480px)

  if (force) {
    // Clear only content area
    _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                              SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
    _ui->drawStatusBar(true);

    // --- 0. INDIKATOR KECIL DI ATAS (Revised width for 480px) ---
    // Layout: 3 Boxes. Approx 130px width each.
    // Margin 25px. Spacing 15px.
    // Box 1 X=25. Box 2 X=170. Box 3 X=315.
    // Total Width used: 25 + 130 + 15 + 130 + 15 + 130 + 25 = 470. Fits nicely.

    int boxY = 22;
    int boxH = 30;
    int boxW = 130;
    int gap = 15;
    int startX = 25;

    // Layout: Left(25) - Center(170) - Right(315)
    tft->fillRect(startX, boxY, boxW, boxH, colTheme); // Hijau
    tft->fillRect(startX + boxW + gap, boxY, boxW, boxH,
                  colBoxBg); // Putih (Contrast)
    tft->fillRect(startX + (boxW + gap) * 2, boxY, boxW, boxH, colRed); // Merah

    // --- TEXT INDIKATOR ---
    char buf[10];
    tft->setFreeFont(&Org_01);
    tft->setTextSize(2); // Perbesar teks agar sesuai box
    tft->setTextDatum(MC_DATUM);

    int yTextMid = boxY + (boxH / 2) + 2; // +2 adjustment for font baseline

    // Box 1 (Green/Kiri): Max RPM
    tft->setTextColor(colBoxText, colTheme);
    sprintf(buf, "%d", _maxRPM);
    tft->drawString(buf, startX + (boxW / 2), yTextMid);

    // Box 2 (White/Tengah): Max Speed
    tft->setTextColor(colBoxText, colBoxBg);
    sprintf(buf, "%.0f", _maxSpeed);
    tft->drawString(buf, startX + boxW + gap + (boxW / 2), yTextMid);

    // Box 3 (Red/Kanan): GPS Signal
    tft->setTextColor(colBoxText, colRed);
    sprintf(buf, "%d", _lastSats);
    tft->drawString(buf, startX + (boxW + gap) * 2 + (boxW / 2), yTextMid);

    tft->setTextDatum(TL_DATUM); // Reset

    // --- 1. HEADER BAR (SPEED) ---
    int headerY = 70; // Ensure separation from Top Boxes (Y=22+30=52)
    tft->fillRect(0, headerY, SCREEN_WIDTH, 22, TFT_WHITE); // Force White BG

    // Tulisan "SPEED" - DI CENTERKAN
    tft->setTextColor(TFT_BLACK, TFT_WHITE); // Force Black Text
    tft->setTextFont(2);
    tft->setTextSize(1);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("SPEED", SCREEN_WIDTH / 2, headerY + 11);

    // Requested Titles "RPM" (Left) and "MAX" (Right)
    tft->drawString("RPM", startX + (boxW / 2), headerY + 11);
    tft->drawString("MAX", startX + (boxW + gap) * 2 + (boxW / 2),
                    headerY + 11);

    tft->setTextDatum(TL_DATUM);

    // --- 2. ANGKA KECEPATAN BESAR ---
    tft->setTextColor(colTheme, _ui->getBackgroundColor());
    tft->setTextFont(7); // 7-Segment Font
    tft->setTextSize(2); // Massive (100px high)
    tft->setTextDatum(MC_DATUM);

    int yCenterSpeed = 100; // Moved UP relative to 125

    char speedBuf[10];
    sprintf(speedBuf, "%.0f", _lastSpeed);
    tft->drawString(speedBuf, SCREEN_WIDTH / 2, yCenterSpeed);
    tft->setTextDatum(TL_DATUM);

    // Satuan "Km/H"
    tft->setTextFont(2);
    tft->setTextSize(1);
    tft->drawCentreString("Km/H", SCREEN_WIDTH / 2, 155, 1); // Below Speed

    // --- 3. DATA TRIP ---
    tft->setTextColor(colTheme, _ui->getBackgroundColor());
    tft->setTextFont(2); // Small Font for Label
    tft->setTextSize(1);
    tft->drawCentreString("LONG TRIP", SCREEN_WIDTH / 2, 175, 1);

    // Trip Value
    tft->setTextFont(4); // Larger Font for Value (Squares style)
    tft->setTextSize(1);
    char tripBuf[10];
    sprintf(tripBuf, "%04.0f", _lastTrip);
    tft->drawCentreString(tripBuf, SCREEN_WIDTH / 2, 195, 1);

    // --- 5. SKALA & GARIS ---
    int yScaleVal = 225; // Adjusted down

    tft->setTextColor(colWhite, _ui->getBackgroundColor());
    tft->setTextSize(1);

    // Scale positions for 480px width
    // Range ~20 to ~460. Width ~440.
    // Ticks: 0, 40, 80, 100, 120, 150 (6 ticks).
    // Let's space them: X=40, 110, 180, 240, 310, 380 ??? Can be arbitrary.
    // Let's try to map the old positions proportionally.
    // Old: 45, 83, 126, 170, 217, 269 (Range ~224px).
    // New Scale factor ~1.5 -> Range ~336px? No, use full width.
    // Let's use simpler fixed points for wider stance.
    // 0 -> X=60
    // 40 -> X=120
    // 80 -> X=180
    // 100 -> X=240 (Center)
    // 120 -> X=300
    // 150 -> X=360
    // ... This is tight.

    // Wider:
    // 0 -> 40
    // 40 -> 110
    // 80 -> 180
    // 100 -> 240 (Center seems weird for linear speed if 100 is center. But
    // this isn't linear scale ticks, just labels) Let's stick to the visual
    // look: Spread out. 0, 40, 80, 100, 120, 150 X Positions:
    // Wider to match RPM Bar (40 to 440)
    int x1 = 40, x2 = 120, x3 = 200, x4 = 280, x5 = 360, x6 = 440;

    tft->drawString("0", x1, yScaleVal);
    tft->drawString("40", x2, yScaleVal);
    tft->drawString("80", x3, yScaleVal);
    tft->drawString("100", x4, yScaleVal);
    tft->drawString("120", x5, yScaleVal);
    tft->drawString("150", x6, yScaleVal);

    // Garis Horizontal
    uint16_t c = colWhite;
    int yLine = 240; // Adjusted down
    // Lines below numbers. ~30px wide segments.
    int segW = 30;
    // Align centers with numbers roughly. Num width ~15px.
    // Center at x + 8 approx.
    tft->drawLine(x1 + 5, yLine, x1 + segW, yLine, c);
    tft->drawLine(x2 + 5, yLine, x2 + segW, yLine, c);
    tft->drawLine(x3 + 5, yLine, x3 + segW, yLine, c);
    tft->drawLine(x4 + 5, yLine, x4 + segW, yLine, c);
    tft->drawLine(x5 + 5, yLine, x5 + segW, yLine, c);
    // tft->drawLine(x6+5, yLine, x6+segW, yLine, c); // Skip last? Old code
    // skipped one? No, 5 ticks.

    // Garis Vertikal (Ticks) - Draw at end of segments?
    tft->drawLine(x1 + segW, yLine, x1 + segW, yLine - 3, c);
    tft->drawLine(x2 + segW, yLine, x2 + segW, yLine - 3, c);
    tft->drawLine(x3 + segW, yLine, x3 + segW, yLine - 3, c);
    tft->drawLine(x4 + segW, yLine, x4 + segW, yLine - 3, c);
    tft->drawLine(x5 + segW, yLine, x5 + segW, yLine - 3, c);

    // --- 6. BAR RPM ---
    int yRPM = 290; // Bottom
    int rpmBarX = 40;
    int rpmBarW = 400; // Wider
    tft->drawRect(rpmBarX - 1, yRPM, rpmBarW + 2, 17, colWhite);

    // ANGKA LIVE RPM
    tft->setTextSize(2);
    tft->setTextColor(colWhite, _ui->getBackgroundColor());
    char rpmBuf[10];
    int dispRpm = (_lastRPM < 0) ? 0 : _lastRPM;
    sprintf(rpmBuf, "%d", dispRpm);
    tft->drawCentreString(rpmBuf, SCREEN_WIDTH / 2, 260, 1); // Above Bar

    // Isi Bar RPM
    int curRpmWidth = map(constrain(_lastRPM, 0, 8000), 0, 8000, 0, rpmBarW);

    tft->fillRect(rpmBarX, yRPM + 1, curRpmWidth, 15, colTheme);
    tft->fillRect(rpmBarX + curRpmWidth, yRPM + 1, rpmBarW - curRpmWidth, 15,
                  COLOR_BG);
  }

  // --- UPDATE DINAMIS ---
  if (!force) {
    char buf[10];
    tft->setFreeFont(&Org_01);

    // Dynamic Layout Values (Must match Static!)
    int boxY = 22;
    int boxH = 30;
    int boxW = 130;
    int gap = 15;
    int startX = 25;
    int yTextMid = boxY + (boxH / 2) + 2;

    // Update Indikator Atas
    tft->setTextSize(2);
    tft->setTextDatum(MC_DATUM);

    // Box 1: Max RPM
    tft->setTextColor(colBoxText, colTheme);
    tft->setTextPadding(100);
    sprintf(buf, "%d", _maxRPM);
    tft->drawString(buf, startX + (boxW / 2), yTextMid);
    tft->setTextPadding(0);

    // Box 2: Max Speed
    tft->setTextColor(colBoxText, colBoxBg);
    tft->setTextPadding(100);
    sprintf(buf, "%.0f", _maxSpeed);
    tft->drawString(buf, startX + boxW + gap + (boxW / 2), yTextMid);
    tft->setTextPadding(0);

    // Box 3: GPS Satellites
    tft->setTextColor(colBoxText, colRed);
    tft->setTextPadding(100);
    sprintf(buf, "%d", _lastSats);
    tft->drawString(buf, startX + (boxW + gap) * 2 + (boxW / 2), yTextMid);
    tft->setTextPadding(0);

    tft->setTextDatum(TL_DATUM);

    // 2. Update Speed Utama
    tft->setTextColor(colTheme, _ui->getBackgroundColor());
    tft->setTextFont(7);
    tft->setTextSize(2);
    tft->setTextDatum(MC_DATUM);
    tft->setTextPadding(480); // Full width padding to clear old number
    int yCenterSpeed = 100;   // Synced with Static
    char speedBuf[10];
    sprintf(speedBuf, "%.0f", _lastSpeed);
    tft->drawString(speedBuf, SCREEN_WIDTH / 2, yCenterSpeed);
    tft->setTextPadding(0);
    tft->setTextDatum(TL_DATUM);

    // 3. Update Trip
    tft->setTextColor(colTheme, _ui->getBackgroundColor());
    tft->setTextFont(4); // Match Static
    tft->setTextSize(1);
    tft->setTextPadding(200);
    // Format: "0000"
    char tripBuf[10];
    sprintf(tripBuf, "%04.0f", _lastTrip);
    tft->drawCentreString(tripBuf, SCREEN_WIDTH / 2, 195, 1);
    tft->setTextPadding(0);

    // 4. Update Bar RPM & ANGKA RPM LIVE
    int rpmBarX = 40;
    int rpmBarW = 400;
    int curRpmWidth = map(constrain(_lastRPM, 0, 8000), 0, 8000, 0, rpmBarW);
    int yRPM = 290; // Synced with Static

    // Update Bar
    tft->fillRect(rpmBarX, yRPM + 1, curRpmWidth, 15, colTheme);
    tft->fillRect(rpmBarX + curRpmWidth, yRPM + 1, rpmBarW - curRpmWidth, 15,
                  COLOR_BG);

    // Update Angka RPM
    tft->setTextSize(2);
    tft->setTextColor(colWhite, _ui->getBackgroundColor());
    tft->setTextPadding(100);
    char rpmBuf[10];
    int dispRpm = (_lastRPM < 0) ? 0 : _lastRPM;
    sprintf(rpmBuf, "%d", dispRpm);
    tft->drawCentreString(rpmBuf, SCREEN_WIDTH / 2, 260,
                          1); // Synced with Static
    tft->setTextPadding(0);
  }
}
