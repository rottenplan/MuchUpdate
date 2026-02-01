#include "UIManager.h"
#include "../../config.h"
#include "../core/GPSManager.h"
#include "../core/WiFiManager.h"
#include "fonts/Org_01.h"
#include <Preferences.h>
#include <WiFi.h>

extern WiFiManager wifiManager;

// Sertakan layar (dibuat di langkah berikutnya)
#include "screens/DragMeterScreen.h"
#include "screens/GpsStatusScreen.h"
#include "screens/HistoryScreen.h"
#include "screens/LapTimerScreen.h"
#include "screens/MenuScreen.h"
#include "screens/RpmSensorScreen.h"
#include "screens/SettingsScreen.h"
#include "screens/SetupScreen.h"
#include "screens/SpeedometerScreen.h"
#include "screens/SplashScreen.h"
#include "screens/SynchronizeScreen.h"
#include "screens/TimeSettingScreen.h"
#include "screens/TimeSettingScreen.h" // Removed duplicate as per
// instruction #include "screens/AutoOffScreen.h"
#include "screens/GnssLogScreen.h"
#include "screens/GpsDebugScreen.h"
#include "screens/RpmSensorScreen.h"
#include "screens/TouchDebugScreen.h"
#include "screens/WebServerScreen.h"

UIManager::UIManager(TFT_eSPI *tft) : _tft(tft), _touch(nullptr) {
  _currentScreen = nullptr;

  // Initialize State Trackers
  _lastTimeStr = "";
  _lastHdop = -1.0;
  _lastFix = false;
  _lastSignalStrength = -1;
  _lastBat = -1;
  _lastLogging = false;
  _lastWifiStatus = -1;
  _isDarkMode = true;        // Default Dark
  _debugTouchEnabled = true; // FORCE ENABLE DEBUG
  _lastTouchProcessedTime = 0;
}

void UIManager::begin() {
  // Instansiasi Layar
  _splashScreen = new SplashScreen();
  _setupScreen = new SetupScreen();
  _menuScreen = new MenuScreen();
  _lapTimerScreen = new LapTimerScreen();
  _dragMeterScreen = new DragMeterScreen();
  _historyScreen = new HistoryScreen();
  _settingsScreen = new SettingsScreen();
  _timeSettingScreen = new TimeSettingScreen();
  // _autoOffScreen = new AutoOffScreen();
  _rpmSensorScreen = new RpmSensorScreen();
  _speedometerScreen = new SpeedometerScreen();
  _gpsStatusScreen = new GpsStatusScreen();
  _synchronizeScreen = new SynchronizeScreen();
  _gnssLogScreen = new GnssLogScreen();
  _webServerScreen = new WebServerScreen();
  _gpsDebugScreen = new GpsDebugScreen();
  _touchDebugScreen = new TouchDebugScreen();

  // Mulai Layar
  _splashScreen->begin(this);
  _setupScreen->begin(this);
  _menuScreen->begin(this);
  _lapTimerScreen->begin(this);
  _dragMeterScreen->begin(this);
  _historyScreen->begin(this);
  _settingsScreen->begin(this);
  _timeSettingScreen->begin(this);
  // _autoOffScreen->begin(this);
  _rpmSensorScreen->begin(this);
  _speedometerScreen->begin(this);
  _gpsStatusScreen->begin(this);
  _synchronizeScreen->begin(this);
  _gnssLogScreen->begin(this);
  _webServerScreen->begin(this);
  _gpsDebugScreen->begin(this);

  // Initialize Sleep Logic (Standardized with power_save)
  Preferences prefs;
  prefs.begin("laptimer", true);
  int psIdx = prefs.getInt("power_save", 1); // Default 5 min
  prefs.end();

  unsigned long ms = 0;
  switch (psIdx) {
  case 0:
    ms = 60000;
    break; // 1 min
  case 1:
    ms = 300000;
    break; // 5 min
  case 2:
    ms = 600000;
    break; // 10 min
  case 3:
    ms = 1800000;
    break; // 30 min
  case 4:
    ms = 0;
    break; // Never
  }
  setAutoOff(ms);

  // Load debug touch setting
  prefs.begin("laptimer", true);
  _debugTouchEnabled = prefs.getBool("debug_touch", false);
  prefs.end();
  _lastInteractionTime = millis();
  _isScreenOff = false;
  _currentBrightness = 255; // Default max, should load from prefs if we had a
                            // stored brightness variable

  _lastTapTime = 0;
  _wasTouched = false;

  // Mulai dengan Splash
  switchScreen(SCREEN_SPLASH);

  // Load Manual Time from NVS - HANDLED BY GPSManager
  // prefs.begin("laptimer", true);
  // g_manualHour = prefs.getInt("m_hour", 12);
  // g_manualMinute = prefs.getInt("m_min", 0);
  // g_utcOffset = prefs.getInt("utc_offset_idx", 12) - 12;
  // Theme load - Forced Dark Mode per user request
  prefs.begin("laptimer", true);
  // _isDarkMode = prefs.getBool("dark_mode", true);
  _isDarkMode = true; // Always Dark
  prefs.end();

  // prefs.end();
  // g_lastTimeUpdate = millis();
}

void UIManager::update() {
  // Perbarui logika layar saat ini
  if (_currentScreen) {
    if (!_isScreenOff) {
      _currentScreen->update();
    } else {
      // Handle Wakeup from Off State (Double Tap)
      if (_touch) {
        _touch->read();
        bool touched = _touch->isTouched;

        // Detect Rising Edge (Touch Start)
        if (touched && !_wasTouched) {
          unsigned long now = millis();
          if (now - _lastTapTime < 500) {
            // Double Tap Detected!
            wakeUp();
            _lastTapTime = 0; // Reset
          } else {
            _lastTapTime = now;
          }
        }
        _wasTouched = touched;
      }
    }
  }

  checkSleep();

  // --- PENANGANAN SENTUH ---
  // (Opsional) Kita dapat menangani sentuhan global tertentu di sini, tetapi
  // untuk saat ini membiarkan layar menanganinya memberikan kontrol konteks
  // yang lebih banyak.

  // Periodically update status bar (at least once per second)
  static unsigned long lastStatusUpdate = 0;
  if (_currentType != SCREEN_SPLASH && _currentType != SCREEN_SETUP &&
      (millis() - lastStatusUpdate >= 1000)) {
    drawStatusBar();
    lastStatusUpdate = millis();
  }
}

UIManager::TouchPoint UIManager::getTouchPoint() {
  TouchPoint p = {-1, -1};
  if (!_touch)
    return p;

  // 1. Initial Read
  _touch->read();
  if (_touch->isTouched) {
    int x1 = _touch->points[0].x;
    int y1 = _touch->points[0].y;

    // Reject immediate invalid zeros
    if (x1 == 0 && y1 == 0)
      return p;

    // 2. Stability Delay (Short wait to filter transient electrical spikes)
    delay(20);

    // 3. Verification Read
    _touch->read();
    if (!_touch->isTouched) {
      // It was a ghost spike!
      return p;
    }

    int x2 = _touch->points[0].x;
    int y2 = _touch->points[0].y;

    // 4. Coordinate Consistency Check
    // Ghost touches often jump wildly. Real touches are stable.
    // Calculate distance squared to avoid sqrt
    int dx = x1 - x2;
    int dy = y1 - y2;
    int distSq = dx * dx + dy * dy;

    // Threshold: 20px variance allowed (finger wiggle)
    if (distSq > 400) {
      return p; // Inconsistent coordinates -> Noise
    }

    // --- PROCESSING VALID TOUCH ---
    updateInteraction();

    // Calibration Logic (Use the stable second read X2, Y2)
    int pX = x2;
    int pY = y2;

    if (TOUCH_SWAP_XY) {
      int temp = pX;
      pX = pY;
      pY = temp;
    }
    if (TOUCH_INVERT_X) {
      pX = SCREEN_WIDTH - 1 - pX;
    }
    if (TOUCH_INVERT_Y) {
      pY = SCREEN_HEIGHT - 1 - pY;
    }

    p.x = pX;
    p.y = pY;

    // Safety Clamp
    if (p.x < 0)
      p.x = 0;
    if (p.x > SCREEN_WIDTH)
      p.x = SCREEN_WIDTH;
    if (p.y < 0)
      p.y = 0;
    if (p.y > SCREEN_HEIGHT)
      p.y = SCREEN_HEIGHT;

    _wasTouched = true;
    _lastTapTime = millis();
    updateInteraction();

    // Global Debounce (Time between valid clicks)
    unsigned long now = millis();
    if (now - _lastTouchProcessedTime <
        150) { // Increased to 150ms for safer UI
      p.x = -1;
      p.y = -1;
      return p;
    }
    _lastTouchProcessedTime = now;

    if (_debugTouchEnabled) {
      _tft->fillCircle(p.x, p.y, 3, TFT_RED);
    }
  }
  return p;
}

#include "../core/GPSManager.h"
#include "../core/SessionManager.h"

extern GPSManager gpsManager;
extern SessionManager sessionManager;

void UIManager::switchScreen(ScreenType type) {
  // Global Cleanup: Ensure no GPS log callbacks persist across screens
  gpsManager.setRawDataCallback(nullptr);

  if (_currentScreen) {
    _currentScreen->onHide();
  }
  // Tambahkan penundaan 1 detik untuk transisi yang mulus (kecuali dari Splash)
  // Tidak ada penundaan untuk peralihan instan

  _currentType = type;

  _tft->fillScreen(getBackgroundColor());

  switch (type) {
  case SCREEN_SPLASH:
    _currentScreen = _splashScreen;
    _screenTitle = "";
    break;
  case SCREEN_SETUP:
    _currentScreen = _setupScreen;
    _screenTitle = "";
    break;
  case SCREEN_MENU:
    _currentScreen = _menuScreen;
    _screenTitle = ""; // Kosong = Tampilkan Waktu
    break;
  case SCREEN_LAP_TIMER:
    _currentScreen = _lapTimerScreen;
    _screenTitle = "LAP TIMER";
    break;
  case SCREEN_DRAG_METER:
    _currentScreen = _dragMeterScreen;
    _screenTitle = "DRAG METER";
    break;
  case SCREEN_HISTORY:
    _currentScreen = _historyScreen;
    _screenTitle = "HISTORY";
    break;
  case SCREEN_SETTINGS:
    _currentScreen = _settingsScreen;
    _screenTitle = "SETTINGS";
    break;
  case SCREEN_TIME_SETTINGS:
    _currentScreen = _timeSettingScreen;
    _screenTitle = "CLOCK";
    break;
  // case SCREEN_AUTO_OFF:
  //   _currentScreen = _autoOffScreen;
  //   _screenTitle = "AUTO OFF";
  //   break;
  case SCREEN_RPM_SENSOR:
    _currentScreen = _rpmSensorScreen;
    _screenTitle = "RPM SENSOR";
    break;
  case SCREEN_SPEEDOMETER:
    _currentScreen = _speedometerScreen;
    // setTitle("Speedometer"); // Custom Title handled in screen
    break;
  case SCREEN_GPS_STATUS:
    _currentScreen = _gpsStatusScreen;
    break;
  case SCREEN_SYNCHRONIZE:
    _currentScreen = _synchronizeScreen;
    _screenTitle = "SYNCHRONIZE";
    break;
  case SCREEN_GNSS_LOG:
    _currentScreen = _gnssLogScreen;
    _screenTitle =
        ""; // Empty to show Time, avoids overlap with "GPS LOG" header
    break;
  case SCREEN_WEB_SERVER:
    _currentScreen = _webServerScreen;
    _screenTitle = "OFFLINE SERVER";
    break;
  case SCREEN_GPS_DEBUG:
    _currentScreen = _gpsDebugScreen;
    _screenTitle = "GPS DEBUG";
    break;
  case SCREEN_TOUCH_DEBUG:
    _currentScreen = _touchDebugScreen;
    _screenTitle = "TOUCH DEBUG"; // Assuming a title for consistency
    break;
  }

  if (_currentScreen) {
    _currentScreen->onShow();
  }

  // Gambar Bilah Status setelah onShow agar tidak tertimpa oleh fillScreen di
  // layar
  if (_currentType != SCREEN_SPLASH && _currentType != SCREEN_SETUP) {
    drawStatusBar(true); // Force redraw on switch
  }
}

void UIManager::drawStatusBar(bool force) {
  // 1. Time Update Logic - Managed by GPSManager

  // Force redraw periodically if time changes (simple check)
  static int lastMin = -1;
  int h, m, s, d, mo, y;
  extern GPSManager gpsManager;
  gpsManager.getLocalTime(h, m, s, d, mo, y);

  if (m != lastMin) {
    force = true;
    lastMin = m;
  }

  // 1. Elemen Statis (Hanya gambar sekali atau jika dipaksa? Untuk saat ini,
  // kita asumsikan latar belakang dihapus hanya pada peralihan layar) Jika kita
  // ingin menghindari kedipan, kita TIDAK boleh menghapus seluruh bilah setiap
  // bingkai. Kita mengandalkan warna latar belakang teks untuk menimpa teks
  // lama.

  // Gambar garis pemisah (aman untuk digambar ulang, cepat)
  if (force) {
    // Explicitly CLEAR the entire status bar area to prevent overlap/artifacts
    _tft->fillRect(0, 0, SCREEN_WIDTH, 20, getBackgroundColor());
    _tft->drawFastHLine(0, 20, SCREEN_WIDTH, getSecondaryColor());
  }

  _tft->setFreeFont(&Org_01);
  _tft->setTextSize(FONT_SIZE_STATUS_BAR);
  _tft->setTextColor(getTextColor(), getBackgroundColor());

  // --- WiFi Section (Left: x=5) ---
  int wifiStatus = wifiManager.isConnected() ? 1 : 0;
  if (force || wifiStatus != _lastWifiStatus) {
    // Clear WiFi Area (0 to 30)
    _tft->fillRect(0, 0, 30, 20, getBackgroundColor());

    uint16_t color = (wifiStatus == 1) ? TFT_GREEN : TFT_RED;
    int wx = 12; // Centered at 12
    int wy = 17;

    // WiFi Icon
    _tft->fillCircle(wx, wy - 1, 1, color);
    _tft->drawFastHLine(wx - 1, wy - 4, 3, color);
    _tft->drawPixel(wx - 2, wy - 3, color);
    _tft->drawPixel(wx + 2, wy - 3, color);
    _tft->drawFastHLine(wx - 3, wy - 7, 7, color);
    _tft->drawPixel(wx - 4, wy - 6, color);
    _tft->drawPixel(wx + 4, wy - 6, color);
    _tft->drawPixel(wx - 5, wy - 5, color);
    _tft->drawPixel(wx + 5, wy - 5, color);
    _tft->drawFastHLine(wx - 5, wy - 10, 11, color);
    _tft->drawPixel(wx - 6, wy - 9, color);
    _tft->drawPixel(wx + 6, wy - 9, color);
    _tft->drawPixel(wx - 7, wy - 8, color);
    _tft->drawPixel(wx + 7, wy - 8, color);

    _lastWifiStatus = wifiStatus;
  }

  // --- GPS Section (Right of WiFi: x=30+) ---
  double hdop = gpsManager.getHDOP();
  bool fix = gpsManager.isFixed();
  int signalStrength = 0;
  if (fix) {
    if (hdop <= 0.8)
      signalStrength = 4;
    else if (hdop <= 1.0)
      signalStrength = 3;
    else if (hdop <= 1.5)
      signalStrength = 2;
    else
      signalStrength = 1;
  }

  int sats = gpsManager.getSatellites();
  static int _lastSats = -1;
  if (force || fix != _lastFix || signalStrength != _lastSignalStrength ||
      sats != _lastSats) {
    _lastSats = sats;
    // Clear GPS Area (30 to 100)
    _tft->fillRect(30, 0, 70, 20, getBackgroundColor());

    // CRITICAL: Reset Font to Standard
    _tft->setFreeFont(NULL);
    _tft->setTextFont(1);

    // 1. Draw Signal Bars (Left of Text)
    int barX = 35; // Start bars immediately after WiFi
    int barY = 16;
    int barW = 3;
    int barGap = 2;

    for (int i = 0; i < 4; i++) {
      int h = (i + 1) * 3;
      int x = barX + (i * (barW + barGap));
      int y = barY - h;

      if (i < signalStrength) {
        uint16_t color = fix ? TFT_GREEN : TFT_RED;
        _tft->fillRect(x, y, barW, h, color);
      } else {
        _tft->drawRect(x, y, barW, h, getTextColor());
      }
    }

    // 2. Draw Label "SAT: XX" (Right of Bars)
    _tft->setTextDatum(ML_DATUM);
    _tft->setTextColor(getTextColor(), getBackgroundColor());
    _tft->setTextSize(1);

    String satStr = "SAT:" + String(sats);
    // Bars end approx at 35 + (4*5) = 55
    _tft->drawString(satStr, 60, 10); // Start text at 60

    _lastFix = fix;
    _lastSignalStrength = signalStrength;
  }

  // --- Bagian Waktu / Judul ---
  String centerText;
  if (_screenTitle.length() > 0) {
    centerText = _screenTitle;
  } else {
    char buf[16];
    sprintf(buf, "%02d:%02d", h, m);
    centerText = String(buf);
  }

  if (force || centerText != _lastTimeStr) {
    int areaW = 120;
    _tft->setTextPadding(areaW);
    _tft->setTextDatum(MC_DATUM);
    _tft->setTextColor(getTextColor(), getBackgroundColor());
    _tft->drawString(centerText, SCREEN_WIDTH / 2, 10);
    _tft->setTextPadding(0);
    _lastTimeStr = centerText;
  }

  // --- Bagian Baterai ---
  // Read Battery Voltage every 5 seconds to prevent slowing down loop
  static unsigned long lastBatRead = 0;
  static int lastPct = -1;
  static float lastVolts = 0;

#ifdef PIN_BATTERY
  if (millis() - lastBatRead > 5000 || lastPct == -1) {
    lastBatRead = millis();
    int rawADC = analogRead(PIN_BATTERY);
    // 3.3V ~ 4095
    lastVolts = (rawADC / 4095.0) * 3.3 * 2.0; // *2 for divider

    // Percentage Calculation (Linear 3.0V - 4.2V)
    if (lastVolts >= 4.2)
      lastPct = 100;
    else if (lastVolts <= 3.0)
      lastPct = 0;
    else {
      lastPct = (int)((lastVolts - 3.0) / (4.2 - 3.0) * 100);
    }
  }
#endif

  int pct = lastPct;
  float voltage = lastVolts;

  // Percentage Calculation (Linear 3.0V - 4.2V)
  // Max = 4.2, Min = 3.0
  // (Logic moved above)

#ifdef PIN_BATTERY
  if (force || abs(pct - _lastBat) > 2) { // Update if changed by > 2%
    _lastBat = pct;

    // Hapus Area Bar + Teks
    // Area Kanan: 60px lebar?
    _tft->fillRect(SCREEN_WIDTH - 60, 0, 60, 20, getBackgroundColor());

    // Gambar Teks Persentase
    _tft->setTextDatum(MR_DATUM); // Rata Kanan Tengah
    _tft->setTextSize(1);
    _tft->setTextColor(getTextColor(), getBackgroundColor());
    String pctStr = String(pct) + "%";
    _tft->drawString(pctStr, SCREEN_WIDTH - 32, 10); // Centered at 10

    // Gambar Ikon Baterai
    int batX = SCREEN_WIDTH - 28; // Geser sedikit ke kiri
    int batY = 5;                 // Centered (5 to 15)
    int batW = 20;
    int batH = 10;

    _tft->drawRect(batX, batY, batW, batH, getTextColor());
    _tft->fillRect(batX + batW, batY + 2, 2, 6, getTextColor());

    // Isi Level
    int innerW = batW - 4;
    int fillW = (innerW * pct) / 100;

    if (pct > 20)
      _tft->fillRect(batX + 2, batY + 2, fillW, batH - 4, TFT_GREEN);
    else
      _tft->fillRect(batX + 2, batY + 2, fillW, batH - 4, TFT_RED);

    _lastBat = pct;
  }
#endif

  // --- Indikator Perekaman ---
  bool isLogging = sessionManager.isLogging();
  if (force || isLogging != _lastLogging) {
    // Hapus Area Titik
    int dotX = (SCREEN_WIDTH / 2) + 40;
    _tft->fillRect(dotX - 5, 0, 10, 20, getBackgroundColor());

    if (isLogging) {
      _tft->fillCircle(dotX, 10, 3, TFT_RED);
    }
    _lastLogging = isLogging;
  }
}

// --- Auto Off Logic ---

void UIManager::setAutoOff(unsigned long ms) {
  _autoOffMs = ms;
  updateInteraction();
}

void UIManager::setBrightness(int level) {
  _currentBrightness = level;
  if (!_isScreenOff) {
    ledcWrite(0, _currentBrightness);
  }
}
void UIManager::updateInteraction() {
  _lastInteractionTime = millis();
  // if (_isScreenOff) {
  //     wakeUp();
  // }
}

void UIManager::checkSleep() {
  if (_autoOffMs > 0 && !_isScreenOff) {
    if (millis() - _lastInteractionTime > _autoOffMs) {
      _isScreenOff = true;
      ledcWrite(0, 0);
    }
  }
}

void UIManager::wakeUp() {
  _isScreenOff = false;
  _lastInteractionTime = millis();
  ledcWrite(0, _currentBrightness);
}

void UIManager::showToast(String message, int duration) {
  int w = 180; // Smaller width
  int h = 40;  // Smaller height
  int x = (SCREEN_WIDTH - w) / 2;
  int y = 100; // Center-ish (Top-Mid) to avoid covering bottom buttons

  // Colors
  uint16_t COLOR_CARD = 0x18E3; // Charcoal
  uint16_t COLOR_BORDER = TFT_SILVER;

  // Draw Toast Background (Premium Card Style)
  _tft->fillRoundRect(x, y, w, h, 8, COLOR_CARD);
  _tft->drawRoundRect(x, y, w, h, 8, COLOR_BORDER);

  // Draw Text
  _tft->setTextColor(TFT_WHITE, COLOR_CARD);
  _tft->setFreeFont(&Org_01); // Use styled font
  _tft->setTextSize(1);       // Size 1 is readable for Org_01
  _tft->setTextDatum(MC_DATUM);
  _tft->drawString(message, SCREEN_WIDTH / 2,
                   y + h / 2 - 2); // Slight adjustment for font baseline

  delay(duration);
}

void UIManager::drawCarbonBackground(int x, int y, int w, int h) {
  _tft->fillRect(x, y, w, h, getBackgroundColor());
}

// --- Theme Support ---
void UIManager::setDarkMode(bool enable) {
  if (_isDarkMode == enable)
    return;
  _isDarkMode = enable;

  // Force global redraw if possible, or just let next update handle it?
  // Ideally, we trigger a screen refresh.
  // Simplest: fill screen and re-show current
  if (_currentScreen) {
    _tft->fillScreen(getBackgroundColor());
    drawStatusBar(true);
    _currentScreen->onShow(); // Reload screen to apply new colors
  }
}

uint16_t UIManager::getBackgroundColor() {
  return _isDarkMode ? TFT_BLACK : TFT_WHITE;
}

uint16_t UIManager::getTextColor() {
  return _isDarkMode ? TFT_WHITE : TFT_BLACK;
}

uint16_t UIManager::getSecondaryColor() {
  return _isDarkMode ? TFT_DARKGREY : TFT_LIGHTGREY;
}
