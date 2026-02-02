#include "SettingsScreen.h"
#include "../../config.h"
#include "../../core/GPSManager.h"
#include "../../core/SessionManager.h"
#include "../../core/SyncManager.h"
#include "../../core/WiFiManager.h"
#include "../fonts/Org_01.h"
#include "../fonts/Picopixel.h"
#include "TimeSettingScreen.h"

extern SessionManager sessionManager;
extern WiFiManager wifiManager;
extern SyncManager syncManager;

// Static pointer for callback
static TFT_eSPI *static_tft = nullptr;

void sdProgressCallback(int percent, String status) {
  if (!static_tft)
    return;

  // Draw Status
  static_tft->setTextColor(TFT_WHITE, TFT_BLACK);
  static_tft->setTextDatum(TC_DATUM); // Top Center
  static_tft->setTextSize(1);
  static_tft->setFreeFont(&Org_01);

  // Clear text area (approx y=100-120)
  // Use SCREEN_WIDTH (480) for centering
  static_tft->fillRect(0, 100, SCREEN_WIDTH, 30, COLOR_BG);
  static_tft->drawString(status, SCREEN_WIDTH / 2, 110);

  // Draw Bar
  int barW = 300; // Wider bar for 480px screen
  int barH = 12;
  int barX = (SCREEN_WIDTH - barW) / 2;
  int barY = 140;

  // Outline
  static_tft->drawRect(barX - 1, barY - 1, barW + 2, barH + 2, TFT_WHITE);

  // Fill
  int fillW = (barW * percent) / 100;
  static_tft->fillRect(barX, barY, fillW, barH, TFT_GREEN);
  static_tft->fillRect(barX + fillW, barY, barW - fillW, barH,
                       COLOR_BG); // Clear rest
}

void SettingsScreen::onShow() {
  _selectedIdx = -1;
  _lastSelectedIdx = -1;
  _scrollOffset = 0;        // Reset scroll
  _currentMode = MODE_MAIN; // Start at Main
  loadSettings();           // Reload to ensure sync

  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(_ui->getBackgroundColor());
  drawList(0, true);
}

void SettingsScreen::loadSettings() {
  _settings.clear();

  if (_currentMode == MODE_MAIN) {
    _settings.push_back({"CLOCK SETTING", TYPE_ACTION});

    _prefs.begin("laptimer", false);

    // Power Save (Auto Off)
    SettingItem powerSave = {"POWER SAVE", TYPE_VALUE, "power_save"};
    powerSave.options = {"1 min", "5 min", "10 min", "30 min", "Never"};
    powerSave.currentOptionIdx =
        _prefs.getInt("power_save", 1); // Default 5 min
    _settings.push_back(powerSave);

    // Theme (Dark/Light) - REMOVED per user request
    // SettingItem theme = {"THEME", TYPE_TOGGLE, "theme"};
    // theme.checkState = _ui->isDarkMode();
    // _settings.push_back(theme);

    // Brightness
    SettingItem brightness = {"BRIGHTNESS", TYPE_VALUE, "brightness"};
    brightness.options = {"10%", "20%", "30%", "40%", "50%",
                          "60%", "70%", "80%", "90%", "100%"};
    brightness.currentOptionIdx =
        _prefs.getInt("brightness", 9); // Default 100%
    _settings.push_back(brightness);

    // Debug Touch Toggle
    SettingItem debugTouch = {"DEBUG TOUCH", TYPE_TOGGLE, "debug_touch"};
    debugTouch.checkState = _prefs.getBool("debug_touch", false);
    _settings.push_back(debugTouch);

    _prefs.end();

    // GPS Status Sub-menu REMOVED
    // _settings.push_back({"GPS STATUS", TYPE_ACTION});
    // _settings.push_back({"GNSS LOG", TYPE_ACTION}); // Moved to GPS Status

    // Double Tap
    _settings.push_back({"GNSS FINE TUNING", TYPE_ACTION});

    // Utility Sub-menu (New)
    _settings.push_back({"UTILITY", TYPE_ACTION});

    // RPM Settings Sub-menu
    _settings.push_back({"RPM SETTING", TYPE_ACTION});

    // WiFi / Cloud Sub-menu
    _settings.push_back({"WIFI / CLOUD", TYPE_ACTION});

    // About Device
    _settings.push_back({"ABOUT DEVICE", TYPE_ACTION});

  } else if (_currentMode == MODE_WIFI_MENU) {
    _prefs.begin("laptimer", false);

    // Offline Server (Replaces Toggle)
    _settings.push_back({"OFFLINE SERVER", TYPE_ACTION});

    // WiFi Setup (Client Mode)
    _settings.push_back({"WIFI SETUP", TYPE_ACTION});

    // Cloud Sync
    _settings.push_back({"SYNC WITH CLOUD", TYPE_ACTION});

    // Account Management
    _settings.push_back({"REMOVE ACCOUNT", TYPE_ACTION});

    _prefs.end();

  } else if (_currentMode == MODE_ENGINE) {
    _prefs.begin("laptimer", false);

    // Engine Hours (Read-only display)
    SettingItem engineHours = {"TOTAL HOURS", TYPE_VALUE, "engine_hours"};
    float hours = _prefs.getFloat("engine_hours", 0.0);
    engineHours.options = {String(hours, 1) + " hrs"};
    engineHours.currentOptionIdx = 0;
    _settings.push_back(engineHours);

    _prefs.end();
  } else if (_currentMode == MODE_RPM) {
    _prefs.begin("laptimer", false);

    // Pulses Per Revolution (PPR)
    SettingItem ppr = {"PULSE PER REV", TYPE_VALUE, "rpm_ppr"};
    ppr.options = {"1.0 (2T/4T Wasted)", "0.5 (1p/2r)", "2.0 (2T Twin)",
                   "3.0 (3p/1r)", "4.0 (4p/1r)"};
    ppr.currentOptionIdx = _prefs.getInt("rpm_ppr", 0);
    if (ppr.currentOptionIdx < 0 || ppr.currentOptionIdx >= ppr.options.size())
      ppr.currentOptionIdx = 0;

    _settings.push_back(ppr);

    // RPM ON/OFF
    // Use TYPE_TOGGLE
    extern GPSManager gpsManager;
    SettingItem rpmOnOff = {"RPM SENSOR", TYPE_TOGGLE, "rpm_enabled"};
    rpmOnOff.checkState = gpsManager.isRpmEnabled();
    _settings.push_back(rpmOnOff);

    // Engine Hours (Moved here)
    _settings.push_back({"ENGINE HOURS", TYPE_ACTION});

    // Units (Moved from Main)
    SettingItem units = {"UNITS", TYPE_VALUE, "units"};
    units.options = {"Metric (km/h)", "Imperial (mph)"};
    units.currentOptionIdx = _prefs.getInt("units", 0); // Default Metric
    _settings.push_back(units);

    _prefs.end();
  } else if (_currentMode == MODE_CLOCK) {
    _prefs.begin("laptimer", false);

    // UTC Time Zone
    SettingItem utcZone = {"UTC TIME ZONE", TYPE_VALUE, "utc_offset_idx"};
    for (int i = -12; i <= 12; i++) {
      char buf[16];
      if (i == 0)
        sprintf(buf, "UTC +00:00");
      else
        sprintf(buf, "UTC %s%02d:00", (i > 0 ? "+" : ""), i);
      utcZone.options.push_back(String(buf));
    }
    utcZone.currentOptionIdx =
        _prefs.getInt("utc_offset_idx", 12); // Default UTC+0
    extern GPSManager gpsManager;
    gpsManager.setUtcOffset(utcZone.currentOptionIdx - 12); // Ensure sync
    _settings.push_back(utcZone);

    _settings.push_back({"MANUAL CLOCK ADJUST", TYPE_ACTION});

    _prefs.end();
  } else if (_currentMode == MODE_GNSS_CONFIG) {
    _prefs.begin("laptimer", false);

    // 1. GNSS Mode
    SettingItem mode = {"GNSS MODE", TYPE_VALUE, "gnss_mode"};
    mode.options = {
        "All (10Hz)",         // 0
        "GPS+GLO+SBAS(16Hz)", // 1
        "GPS+GAL+GLO(10Hz)",  // 2
        "GPS+GAL+SBAS(20Hz)", // 3
        "GPS+SBAS (25Hz)",    // 4
        "GPS Only (25Hz)",    // 5
        "GPS+BEI+SBAS(12Hz)", // 6
        "GPS+GLO (16Hz)"      // 7
    };
    mode.currentOptionIdx = _prefs.getInt("gnss_mode", 1);
    _settings.push_back(mode);

    // 2. GNSS Coordinate Projection
    SettingItem proj = {"COORD PROJECTION", TYPE_VALUE, "gnss_proj"};
    proj.options = {"No Projection", "Projection (Def)"};
    // Map bool to index: true -> 1, false -> 0
    // But we need to check how we stored it? We stored as bool.
    // Let's rely on internal state or load fresh.
    bool projState = _prefs.getBool("gnss_proj", true);
    proj.currentOptionIdx = projState ? 1 : 0;
    _settings.push_back(proj);

    // 3. Frequency Limit
    SettingItem freq = {"FREQUENCY LIMIT", TYPE_VALUE, "gnss_freq_limit"};
    freq.options = {"1 Hz", "2 Hz", "5 Hz", "10 Hz", "18 Hz"};
    freq.currentOptionIdx =
        _prefs.getInt("gnss_freq_limit", 2); // Default 5Hz (Index 2)
    _settings.push_back(freq);

    // 4. Dynamic Model
    SettingItem dyn = {"DYNAMIC MODEL", TYPE_VALUE, "gnss_model"};
    dyn.options = {
        "Portable",         // 0
        "Stationary",       // 1
        "Pedestrian",       // 2
        "Automotive (Def)", // 3
        "At Sea",           // 4
        "Airborne <1g",     // 5
        "Airborne <2g",     // 6
        "Airborne <4g"      // 7
    };
    dyn.currentOptionIdx = _prefs.getInt("gnss_model", 3);
    _settings.push_back(dyn);

    // 5. SBAS Config
    SettingItem sbas = {"SBAS SYSTEM", TYPE_VALUE, "gnss_sbas"};
    sbas.options = {
        "EUROPE (EGNOS)",   // 0
        "USA (WAAS)",       // 1
        "RUSSIA (SDCM)",    // 2
        "JAPAN (MSAS)",     // 3
        "INDIA (GAGAN)",    // 4
        "AUS (SouthPAN)",   // 5
        "S.AMERICA (NONE)", // 6
        "MID-EAST (NONE)",  // 7
        "AFRICA (NONE)",    // 8
        "CHINA (BDSBAS)",   // 9
        "KOREA (KASS)"      // 10
    };
    sbas.currentOptionIdx = _prefs.getInt("gnss_sbas", 0);
    _settings.push_back(sbas);

    // 6. GNSS RX PIN
    SettingItem rxPin = {"GNSS RX PIN", TYPE_VALUE, "gps_rx_pin"};
    // Options: 22(Def), 21
    rxPin.options = {"22 (Def)", "21"};

    extern GPSManager gpsManager;
    int curRx = gpsManager.getRxPin();
    rxPin.currentOptionIdx = 0; // Default 22
    int validRxCheck[] = {22, 21};
    for (int i = 0; i < 2; i++) {
      if (validRxCheck[i] == curRx) {
        rxPin.currentOptionIdx = i;
        break;
      }
    }
    _settings.push_back(rxPin);

    // 7. GNSS TX PIN
    SettingItem txPin = {"GNSS TX PIN", TYPE_VALUE, "gps_tx_pin"};
    // Options: 21(Def), 22
    txPin.options = {"21 (Def)", "22"};
    int curTx = gpsManager.getTxPin();
    txPin.currentOptionIdx = 0; // Default 21
    int validTxCheck[] = {21, 22};
    for (int i = 0; i < 2; i++) {
      if (validTxCheck[i] == curTx) {
        txPin.currentOptionIdx = i;
        break;
      }
    }
    _settings.push_back(txPin);

    // 8. GNSS BAUD RATE
    SettingItem baud = {"GNSS BAUD RATE", TYPE_VALUE, "gps_baud"};
    baud.options = {"9600 bps", "19200 bps", "38400 bps", "57600 bps",
                    "115200 bps"};
    int curBaud = gpsManager.getBaud();
    baud.currentOptionIdx = 0; // Default 9600
    int validBauds[] = {9600, 19200, 38400, 57600, 115200};
    for (int i = 0; i < 5; i++) {
      if (validBauds[i] == curBaud) {
        baud.currentOptionIdx = i;
        break;
      }
    }
    _settings.push_back(baud);

    _settings.push_back({"GPS DEBUG", TYPE_ACTION});

    _prefs.end();

  } else if (_currentMode == MODE_UTILITY) {
    _prefs.begin("laptimer", false);

    // SD Card Test (Moved here)
    _settings.push_back({"SD CARD TEST", TYPE_ACTION});

    // TFT Benchmark (Standard)
    _settings.push_back({"TFT BENCHMARK", TYPE_ACTION});

    _prefs.end();
  }
}

void SettingsScreen::saveSetting(int idx) {
  if (idx < 0 || idx >= _settings.size())
    return;

  _prefs.begin("laptimer", false);
  SettingItem &item = _settings[idx];

  if (item.type == TYPE_VALUE) {
    _prefs.putInt(item.key.c_str(), item.currentOptionIdx);

    // Apply immediate effects
    if (item.key == "brightness") {
      // Map 0-9 (10%-100%) to PWM duty cycle
      int duty = map(item.currentOptionIdx, 0, 9, 26, 255); // 10% to 100%
      _ui->setBrightness(duty);
    }

    if (item.key == "power_save") {
      unsigned long ms = 0;
      switch (item.currentOptionIdx) {
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
      _ui->setAutoOff(ms);
    }

    if (item.key == "rpm_ppr") {
      extern GPSManager gpsManager;
      gpsManager.setPPRIndex(item.currentOptionIdx);
    }

    // GPS Config Handlers
    extern GPSManager gpsManager;

    if (item.key == "utc_offset_idx") {
      int offset = item.currentOptionIdx - 12;
      gpsManager.setUtcOffset(offset);
      _ui->drawStatusBar(true);
    }

    if (item.key == "gnss_mode") {
      gpsManager.setGnssMode(item.currentOptionIdx);
    }
    if (item.key == "gnss_proj") {
      gpsManager.setProjection(item.currentOptionIdx == 1);
    }
    if (item.key == "gnss_model") {
      gpsManager.setDynamicModel(item.currentOptionIdx);
    }
    if (item.key == "gnss_sbas") {
      gpsManager.setSBASConfig(item.currentOptionIdx);
    }
    if (item.key == "gnss_freq_limit") {
      int freq = 5;
      switch (item.currentOptionIdx) {
      case 0:
        freq = 1;
        break;
      case 1:
        freq = 2;
        break;
      case 2:
        freq = 5;
        break;
      case 3:
        freq = 10;
        break;
      case 4:
        freq = 18;
        break;
      }
      gpsManager.setFrequencyLimit(freq);
    }

    if (item.key == "gps_rx_pin" || item.key == "gps_tx_pin") {
      // Map index to pin value
      int validRxCheck[] = {17, 16, 1, 3};
      int validTxCheck[] = {17, 16, 1, 3};

      // Get fresh pin values from other settings if just changing one
      int newRx = gpsManager.getRxPin();
      int newTx = gpsManager.getTxPin();

      if (item.key == "gps_rx_pin") {
        int validRxCheck[] = {22, 21};
        if (item.currentOptionIdx >= 0 && item.currentOptionIdx < 2)
          newRx = validRxCheck[item.currentOptionIdx];
      }
      if (item.key == "gps_tx_pin") {
        int validTxCheck[] = {21, 22};
        if (item.currentOptionIdx >= 0 && item.currentOptionIdx < 2)
          newTx = validTxCheck[item.currentOptionIdx];
      }

      // Apply new pins
      gpsManager.setPins(newRx, newTx);
    }

    if (item.key == "gps_baud") {
      int validBauds[] = {9600, 19200, 38400, 57600, 115200};
      if (item.currentOptionIdx >= 0 && item.currentOptionIdx < 5) {
        gpsManager.setBaud(validBauds[item.currentOptionIdx]);
      }
    }

  } else if (item.type == TYPE_TOGGLE) {
    // Update Pref
    // Note: putBool is done below

    if (item.key == "rpm_enabled") {
      extern GPSManager gpsManager;
      gpsManager.setRpmEnabled(
          !item.checkState); // Invert? No, item.checkState is the CURRENT state
                             // *before* toggle?
      // Wait, let's look at logic below: item.checkState = !item.checkState;
      // The logic in handleTouch (line 601) toggles it BEFORE calling
      // saveSetting. So item.checkState is the NEW desired state.
      gpsManager.setRpmEnabled(item.checkState);
    }

    if (item.key == "wifi_hotspot") {
      wifiManager.setEnabled(item.checkState);
    }

    if (item.key == "theme") {
      // _ui->setDarkMode(item.checkState);
      // _prefs.putBool("dark_mode", item.checkState);
    }

    if (item.key == "debug_touch") {
      _ui->setDebugTouch(item.checkState);
    }

    _prefs.putBool(item.key.c_str(), item.checkState);
  }
  _prefs.end();
}

void SettingsScreen::update() {
  static unsigned long lastSettingTouch = 0;

  // WiFi Scanning Animation (runs without touch)
  if (_isScanning) {
    if (millis() - _lastScanAnim > 500) {
      _lastScanAnim = millis();
      _scanAnimStep = (_scanAnimStep + 1) % 4;

      TFT_eSPI *tft = _ui->getTft();
      tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
      tft->setTextDatum(MC_DATUM);
      String dots = "";
      for (int i = 0; i < _scanAnimStep; i++)
        dots += ".";
      tft->drawString("Scanning" + dots + "   ", SCREEN_WIDTH / 2, 120);
    }

    int n = WiFi.scanComplete();
    if (n >= 0) {
      _isScanning = false;
      _scanCount = n;
      // Clear only content area
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true);
      drawWiFiList();
      _lastWiFiTouch = millis();
    }
    return; // Block other input while scanning
  }

  UIManager::TouchPoint p = _ui->getTouchPoint();
  if (p.x == -1)
    return;

  // 1. Footer Buttons Zone (Standardized Bottom-Bar y > 260)
  if (p.y > 260) {
    if (millis() - lastSettingTouch < 200)
      return;
    lastSettingTouch = millis();

    // A. Tombol Kembali (Bottom-Left)
    if (p.x < 120) {
      // Visual Feedback (Selection)
      if (_selectedIdx != -2) {
        _selectedIdx = -2;
        drawList(_scrollOffset, false);
      }

      // Logic Back (Single Tap)
      if (_currentMode == MODE_MAIN) {
        _ui->switchScreen(SCREEN_MENU);
        return;
      } else if (_currentMode == MODE_WIFI_MENU) {
        _currentMode = MODE_MAIN;
        _ui->setTitle("SETTINGS");
        loadSettings();
      } else if (_currentMode == MODE_ENGINE) {
        _currentMode = MODE_RPM;
        _ui->setTitle("RPM SETTINGS");
        loadSettings();
      } else if (_currentMode == MODE_UTILITY) {
        _currentMode = MODE_MAIN;
        _ui->setTitle("SETTINGS");
        loadSettings();
      } else if (_currentMode == MODE_GRAPHIC_TEST) {
        endGraphicTest();
        _currentMode = MODE_UTILITY;
        _ui->setTitle("UTILITY");
        loadSettings();
      } else {
        _currentMode = MODE_MAIN;
        _ui->setTitle("SETTINGS");
        loadSettings();
      }

      _scrollOffset = 0;
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true);
      drawList(0, true);
      return;
    }

    // B. Scroll Buttons (Bottom-Right)
    if (p.x > SCREEN_WIDTH - 120) {
      int listY = 40;
      int itemH = 24;
      int maxY = 260;
      int visibleItems = (maxY - listY) / itemH;

      if (p.x < SCREEN_WIDTH - 60) { // Up
        if (_scrollOffset > 0) {
          _scrollOffset--;
          drawList(_scrollOffset, true);
        }
      } else { // Down
        if (_scrollOffset + visibleItems < _settings.size()) {
          _scrollOffset++;
          drawList(_scrollOffset, true);
        }
      }
      return;
    }
  }

  // 2. List Zone (40 to 260)
  int listY = 40;
  int itemH = 24;
  int maxY = 260;

  if (_currentMode == MODE_MAIN || _currentMode == MODE_RPM ||
      _currentMode == MODE_CLOCK || _currentMode == MODE_ENGINE ||
      _currentMode == MODE_GNSS_CONFIG || _currentMode == MODE_WIFI_MENU ||
      _currentMode == MODE_UTILITY) {
    if (p.y >= listY && p.y < maxY) {
      if (millis() - lastSettingTouch < 200)
        return;
      lastSettingTouch = millis();

      int idx = _scrollOffset + ((p.y - listY) / itemH);
      if (idx >= 0 && idx < _settings.size()) {
        // Simple Single-Tap to select and double-tap effect or single-tap
        // action
        if (_selectedIdx == idx) {
          handleTouch(idx); // Already selected -> Execute
        } else {
          _selectedIdx = idx; // Select first
          drawList(_scrollOffset, false);
        }
      }
    }
  }

  // WiFi List Mode
  if (_currentMode == MODE_WIFI && p.y > 60 && p.y < 260) {
    if (millis() - _lastWiFiTouch > 300) {
      _lastWiFiTouch = millis();
      int idx = (p.y - 60) / 24;
      if (idx >= 0 && idx < _scanCount) {
        _selectedWiFiIdx = idx;
        _targetSSID = WiFi.SSID(idx);
        _enteredPass = "";
        _currentMode = MODE_WIFI_PASS;
        // Clear only content area
        _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
        _ui->drawStatusBar(true);
        drawKeyboard();
        _lastKeyboardTouch = millis(); // Prevent immediate key press
        return;
      }
    }
  }

  // WiFi Keyboard Mode - handle key presses
  if (_currentMode == MODE_WIFI_PASS) {
    if (millis() - _lastKeyboardTouch > 200) {
      _lastKeyboardTouch = millis();

      // Keyboard input
      KeyboardComponent::KeyResult res = _keyboard.handleTouch(p.x, p.y, 95);
      switch (res.type) {
      case KeyboardComponent::KEY_CHAR: {
        char c = res.value;
        if (!_isUppercase && c >= 'A' && c <= 'Z')
          c += ('a' - 'A');
        _enteredPass += c;
        drawKeyboard(false);
        break;
      }
      case KeyboardComponent::KEY_SHIFT:
        _isUppercase = !_isUppercase;
        drawKeyboard(false);
        break;
      case KeyboardComponent::KEY_DEL:
        if (_enteredPass.length() > 0) {
          _enteredPass.remove(_enteredPass.length() - 1);
          drawKeyboard(false);
        }
        break;
      case KeyboardComponent::KEY_SPACE:
        _enteredPass += " ";
        drawKeyboard(false);
        break;
      case KeyboardComponent::KEY_OK:
        connectWiFi();
        break;
      default:
        // Password Visibility Toggle (Touch area near the password field)
        if (p.x >= SCREEN_WIDTH - 60 && p.x < SCREEN_WIDTH - 10 && p.y >= 80 &&
            p.y < 105) {
          _showPassword = !_showPassword;
          drawKeyboard(false);
        }
        break;
      }
    }
  }

  // Continuous Update for GPS Mode
  if (_currentMode == MODE_GPS) {
    drawGPSStatus();
  }

  if (_currentMode == MODE_GRAPHIC_TEST) {
    updateGraphicTest();
  }

  if (_currentMode == MODE_ABOUT) {
    UIManager::TouchPoint p = _ui->getTouchPoint();
    if (p.x != -1) {
      if (millis() - lastSettingTouch > 200) {
        lastSettingTouch = millis();
        // Exit on any touch or specific back button
        _currentMode = MODE_MAIN;
        loadSettings();
        _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
        _ui->drawStatusBar(true);
        drawList(0, true);
      }
    }
  }
}

void SettingsScreen::handleTouch(int idx) {
  if (idx < 0 || idx >= _settings.size())
    return;

  SettingItem &item = _settings[idx];

  if (item.type == TYPE_VALUE) {
    item.currentOptionIdx++;
    if (item.currentOptionIdx >= item.options.size()) {
      item.currentOptionIdx = 0;
    }
    saveSetting(idx);
    drawList(_scrollOffset, false);
  } else if (item.type == TYPE_TOGGLE) {
    item.checkState = !item.checkState;
    saveSetting(idx);
    drawList(_scrollOffset, false);
  } else if (item.type == TYPE_ACTION) {
    if (item.name == "CLOCK SETTING") {
      _currentMode = MODE_CLOCK;
      loadSettings();
      // Clear only content area
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->switchScreen(SCREEN_TIME_SETTINGS);
      return;
    } else if (item.name == "OFFLINE SERVER") {
      _ui->switchScreen(SCREEN_WEB_SERVER);
      return;
    } else if (item.name == "GPS DEBUG") {
      _ui->switchScreen(SCREEN_GPS_DEBUG);
      return;
    } else if (item.name == "ENGINE HOURS") {
      _currentMode = MODE_ENGINE;
      loadSettings();
      // Clear only content area
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true);
      drawList(0, true);
      _ui->drawStatusBar(true);
      drawList(0, true);
    } else if (item.name == "WIFI / CLOUD") {
      _currentMode = MODE_WIFI_MENU;
      loadSettings();
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true);
      drawList(0, true);
    } else if (item.name == "ABOUT DEVICE") {
      _currentMode = MODE_ABOUT;
      drawAbout();
    } else if (item.name == "WIFI SETUP") {
      // Start WiFi scan
      _currentMode = MODE_WIFI;
      TFT_eSPI *tft = _ui->getTft();
      // Clear only content area
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true);

      tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
      tft->setTextDatum(MC_DATUM);
      tft->drawString("Scanning...", SCREEN_WIDTH / 2, 120);

      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      delay(100);
      WiFi.scanNetworks(true); // Async scan
      _isScanning = true;
      _lastScanAnim = millis();
      _scanAnimStep = 0;
    } else if (item.name == "SYNC WITH CLOUD") {
      // Trigger cloud sync
      TFT_eSPI *tft = _ui->getTft();
      // Clear only content area
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true);

      tft->setTextColor(TFT_CYAN, COLOR_BG);
      tft->setTextDatum(MC_DATUM);
      tft->setTextSize(1);
      tft->setFreeFont(&Org_01);
      tft->drawString("Syncing...", SCREEN_WIDTH / 2, 100);

      // Get user credentials from NVS
      _prefs.begin("muchrace", false);
      String username = _prefs.getString("username", "");
      String password = _prefs.getString("password", "");
      _prefs.end();

      Serial.print("DEBUG: Checking 'muchrace' namespace. Username: '");
      Serial.print(username);
      Serial.println("'");

      Serial.print("DEBUG: Checking 'muchrace' namespace. Username: '");
      Serial.print(username);
      Serial.println("'");

      if (username.length() == 0) {
        tft->fillRect(0, 80, SCREEN_WIDTH, 80, COLOR_BG);
        tft->setTextColor(TFT_RED, COLOR_BG);
        tft->drawString("No account!", SCREEN_WIDTH / 2, 80);
        tft->drawString("Run Setup again", SCREEN_WIDTH / 2,
                        100); // More helpful message
        delay(2000);
      } else {
        // Use API_URL from config.h
        String apiUrl = API_URL;

        // Perform sync
        bool success = syncManager.syncSettings(
            apiUrl.c_str(), username.c_str(), password.c_str());

        // Show result
        tft->fillRect(0, 80, SCREEN_WIDTH, 100, COLOR_BG);
        if (success) {
          tft->setTextColor(TFT_GREEN, _ui->getBackgroundColor());
          tft->drawString("Sync Success!", SCREEN_WIDTH / 2, 100);
          delay(2000);
        } else {
          tft->setTextColor(TFT_RED, _ui->getBackgroundColor());
          tft->drawString("Sync Failed!", SCREEN_WIDTH / 2, 100);
          delay(2000);
        }
      }

      _currentMode = MODE_MAIN;
      loadSettings();
      // Clear only content area
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true);
      drawList(0, true);
    } else if (item.name == "REMOVE ACCOUNT") {
      // Remove account credentials from storage
      TFT_eSPI *tft = _ui->getTft();
      // Clear only content area
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true);

      // --- 1. PROCESSING CARD ---
      int cardW = 280;
      int cardH = 100;
      int cardX = (SCREEN_WIDTH - cardW) / 2;
      int cardY = (SCREEN_HEIGHT - cardH) / 2;

      tft->fillRoundRect(cardX, cardY, cardW, cardH, 8, 0x18E3); // Charcoal
      tft->drawRoundRect(cardX, cardY, cardW, cardH, 8, TFT_DARKGREY);

      tft->setTextColor(TFT_WHITE, 0x18E3);
      tft->setTextDatum(MC_DATUM);
      tft->setTextFont(2);
      tft->setTextSize(1);
      tft->drawString("Removing Account...", SCREEN_WIDTH / 2, cardY + 50);

      // Clear user credentials from NVS
      _prefs.begin("muchrace", false);
      _prefs.clear(); // Clear all user data (including setup_done)
      _prefs.end();

      // Also clear WiFi credentials
      _prefs.begin("wifi", false);
      _prefs.clear();
      _prefs.end();

      delay(1500);

      // --- 2. SUCCESS CARD ---
      // Redraw refined background or just overdraw card
      tft->fillRoundRect(cardX, cardY, cardW, cardH, 8, 0x18E3); // Re-fill
      tft->drawRoundRect(cardX, cardY, cardW, cardH, 8,
                         TFT_GREEN); // Green Border

      tft->setTextColor(TFT_GREEN, 0x18E3);
      tft->setTextFont(4); // Large Font
      tft->drawString("SUCCESS", SCREEN_WIDTH / 2, cardY + 35);

      tft->setTextColor(TFT_SILVER, 0x18E3);
      tft->setTextFont(2);
      tft->drawString("Restarting Device...", SCREEN_WIDTH / 2, cardY + 70);

      delay(3000);

      // Automatic Restart
      ESP.restart();
    } else if (item.name == "RPM SETTING") {
      _currentMode = MODE_RPM;
      loadSettings();
      // Clear only content area
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true); // Redraw Status Bar
      drawList(0, true);
    } else if (item.name == "GNSS FINE TUNING") {
      _currentMode = MODE_GNSS_CONFIG;
      loadSettings();
      // Clear only content area
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true);
      drawList(0, true);
    } else if (item.name == "UTILITY") {
      _currentMode = MODE_UTILITY;
      _ui->setTitle("UTILITY");
      loadSettings();
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true);
      drawList(0, true);
      drawList(0, true);
    } else if (item.name == "TFT BENCHMARK") {
      _currentMode = MODE_GRAPHIC_TEST;
      startGraphicTest();
      return;
    } else if (item.name == "TOUCH DEBUG") {
      _ui->switchScreen(SCREEN_TOUCH_DEBUG);
      return;
    } else if (item.name == "SD CARD TEST") {
      _currentMode = MODE_SD_TEST;
      _ui->setTitle("SD CARD TEST");
      TFT_eSPI *tft = _ui->getTft();
      // Clear only content area
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true); // Redraw Status Bar

      // Draw "Running..."
      _sdResult = {false, "", "", "", 0, 0}; // Reset
      drawSDTest();

      // Setup static pointer for callback
      static_tft = tft;

      // Run Test (Blocking with Callback)
      _sdResult = sessionManager.runFullTest(sdProgressCallback);

      // Clear static
      static_tft = nullptr;

      // Redraw with results
      // Clear only content area
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true); // Redraw Status Bar
      drawSDTest();

      /* WiFi Handlers removed
      // ...
      */
    } else if (item.key == "reset_ref") {
      _prefs.begin("laptimer", false);
      _prefs.remove("drag_ref"); // Key for reference run time
      _prefs.end();
      /* WiFi Handlers removed
      // ...
      */
    }
  }
}

// ... existing code ...

void SettingsScreen::drawAbout() {
  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(_ui->getBackgroundColor());
  _ui->drawStatusBar(true);

  // Card Background
  int cardX = 20;
  int cardY = 50;
  int cardW = SCREEN_WIDTH - 40;
  int cardH = 160;

  tft->fillRoundRect(cardX, cardY, cardW, cardH, 10, 0x18E3); // Charcoal
  tft->drawRoundRect(cardX, cardY, cardW, cardH, 10, TFT_DARKGREY);

  // Content
  tft->setTextColor(TFT_WHITE, 0x18E3);
  tft->setTextDatum(TC_DATUM);
  tft->setTextFont(4); // Large Font
  tft->drawString("Much Racing", SCREEN_WIDTH / 2, cardY + 20);

  tft->setTextFont(2);
  tft->setTextColor(TFT_CYAN, 0x18E3);
  tft->drawString("Race Computer", SCREEN_WIDTH / 2, cardY + 50);

  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->setTextFont(2); // Increased font
  tft->drawString("Version 1.0 (Beta)", SCREEN_WIDTH / 2, cardY + 85);

  String mac = WiFi.macAddress();
  tft->drawString("Device ID: " + mac, SCREEN_WIDTH / 2, cardY + 110);

  tft->setTextColor(TFT_ORANGE, 0x18E3);
  tft->setTextFont(2);
  tft->drawString("Made by Muchdas", SCREEN_WIDTH / 2, cardY + 140);

  // Back button triangle (Bottom-Left)
  tft->fillTriangle(10, 290, 25, 282, 25, 298, COLOR_ACCENT);
}

void SettingsScreen::drawHeader(String title, uint16_t backColor) {
  TFT_eSPI *tft = _ui->getTft();

  // Draw standardized back button at bottom-left
  tft->fillTriangle(10, 290, 25, 282, 25, 298, COLOR_ACCENT);
}

void SettingsScreen::drawGPSStatus(bool force) {
  TFT_eSPI *tft = _ui->getTft();

  if (force) {
    tft->fillScreen(_ui->getBackgroundColor());
    _ui->drawStatusBar(true);

    // Static Header
    // Standardized back button
    tft->fillTriangle(10, 290, 25, 282, 25, 298, COLOR_ACCENT);

    // Static layout elements
    int yStats = 45; // Shifted up from 62 to prevent overlap
    int hStatsHeader = 18;
    tft->fillRect(10, yStats, 160, hStatsHeader, _ui->getTextColor());
    tft->setTextColor(_ui->getBackgroundColor(), _ui->getTextColor());
    tft->setTextSize(1);
    tft->drawString("GPS STATUS", 15, yStats + 9);

    // List Rect
    int listH = 6 * 15 + 8; // 6 items * 15px + pad
    tft->drawRect(10, yStats + hStatsHeader, 160, listH, _ui->getTextColor());

    // Radar
    int cX = 245, cY = 120, r = 55;

    // Radar
    tft->drawCircle(cX, cY, r * 0.66, COLOR_SECONDARY);
    tft->drawCircle(cX, cY, r * 0.33, COLOR_SECONDARY);
    tft->drawFastHLine(cX - r, cY, 2 * r, COLOR_SECONDARY);
    tft->drawFastVLine(cX, cY - r, 2 * r, COLOR_SECONDARY);

    auto drawCard = [&](String l, int x, int y) {
      tft->fillCircle(x, y, 9, _ui->getTextColor());
      tft->setTextColor(_ui->getBackgroundColor(), _ui->getTextColor());
      tft->setTextDatum(MC_DATUM);
      tft->drawString(l, x, y + 1);
    };
    drawCard("N", cX, cY - r);
    drawCard("S", cX, cY + r);
    drawCard("E", cX + r, cY);
    drawCard("W", cX - r, cY);

    // Reset trackers
    _lastSats = -1;
    _lastHdopValue = -1.0;
    _lastLat = 0;
    _lastLon = 0;
    _lastFixed = false;
  }

  // Rate Limiting
  static unsigned long lastGPSDraw = 0;
  if (!force && millis() - lastGPSDraw < 1000)
    return;
  lastGPSDraw = millis();

  extern GPSManager gpsManager;
  int sats = gpsManager.getSatellites();
  double hdop = gpsManager.getHDOP();
  double lat = gpsManager.getLatitude();
  double lon = gpsManager.getLongitude();
  bool fixed = gpsManager.isFixed();

  int yStats = 45; // Match static layout
  int hStatsHeader = 18;
  int hItem = 15;
  int curY = yStats + hStatsHeader + 4;

  auto drawRowValue = [&](String label, String val, String lastVal, int y) {
    if (force || val != lastVal) {
      tft->setTextDatum(ML_DATUM);
      tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
      tft->setFreeFont(&Org_01);
      tft->setTextSize(1);

      // Draw label and separator only on force
      if (force) {
        tft->drawString(label, 15, y + (hItem / 2));
        tft->drawString(":", 60, y + (hItem / 2));
        tft->drawFastHLine(10, y + hItem, 160, COLOR_SECONDARY);
      }

      // Clear and draw value
      tft->fillRect(75, y, 90, hItem - 1, _ui->getBackgroundColor());
      tft->drawString(val, 75, y + (hItem / 2));
    }
  };

  drawRowValue("GPS", String(sats) + " Sat",
               force ? "" : String(_lastSats) + " Sat", curY);
  curY += hItem;
  drawRowValue("GLO", "- Sat", force ? "" : "- Sat", curY);
  curY += hItem;
  drawRowValue("GAL", "- Sat", force ? "" : "- Sat", curY);
  curY += hItem;
  drawRowValue("BEI", "- Sat", force ? "" : "- Sat", curY);
  curY += hItem;
  drawRowValue("MBN", "A : -", force ? "" : "A : -", curY);
  curY += hItem;
  drawRowValue("HDOP", String(hdop, 2), force ? "" : String(_lastHdopValue, 2),
               curY);

  if (force || lat != _lastLat || lon != _lastLon) {
    int tableBottom = yStats + hStatsHeader + (6 * hItem + 8);
    int yLat = tableBottom + 10;
    tft->fillRect(10, yLat, 200, 40, _ui->getBackgroundColor());
    tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
    tft->setTextDatum(TL_DATUM);
    tft->drawString("LAT : " + String(lat, 6), 15, yLat);
    tft->drawString("LNG : " + String(lon, 6), 15, yLat + 18);
  }

  // Polar Plot Satellites (Simulated for feedback in code)
  if (fixed != _lastFixed || fixed) {
    int cX = 245, cY = 120, r = 55;
    // Only clear plot area if state changed or we need to redraw blinkers
    if (fixed) {
      // Clear old dots (simplest is clear small r+5 area around dots, but radar
      // is fast) Just redraw radar lines to "clean" old dots
      tft->drawCircle(cX, cY, r * 0.66, COLOR_SECONDARY);
      tft->drawCircle(cX, cY, r * 0.33, COLOR_SECONDARY);
      tft->drawFastHLine(cX - r, cY, 2 * r, COLOR_SECONDARY);
      tft->drawFastVLine(cX, cY - r, 2 * r, COLOR_SECONDARY);

      unsigned long t = millis() / 1000;
      float rad = ((t * 5) % 360) * DEG_TO_RAD;
      tft->fillCircle(cX + cos(rad) * (r * 0.5), cY + sin(rad) * (r * 0.5), 3,
                      TFT_GREEN);
      rad = ((t * 2 + 120) % 360) * DEG_TO_RAD;
      tft->fillCircle(cX + cos(rad) * (r * 0.8), cY + sin(rad) * (r * 0.8), 3,
                      TFT_GREEN);
    }
  }

  _lastSats = sats;
  _lastHdopValue = hdop;
  _lastLat = lat;
  _lastLon = lon;
  _lastFixed = fixed;

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
}

void SettingsScreen::drawSDTest() {
  TFT_eSPI *tft = _ui->getTft();

  // Clear Content
  tft->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                SCREEN_HEIGHT - STATUS_BAR_HEIGHT, TFT_BLACK);

  // Header (Premium)
  tft->drawFastHLine(0, 20, SCREEN_WIDTH, COLOR_SECONDARY);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->setTextDatum(TC_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->drawString("SD CARD TEST", SCREEN_WIDTH / 2, 28);

  // Standardized back button
  tft->fillTriangle(10, 290, 25, 282, 25, 298, COLOR_ACCENT);

  int y = 60;

  if (!_sdResult.success && _sdResult.cardType == "") {
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    tft->setTextFont(2);
    tft->drawString("Running SD Test...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    return;
  }

  if (!_sdResult.success && _sdResult.cardType == "NO CARD") {
    // Error Card
    tft->fillRoundRect(20, 80, SCREEN_WIDTH - 40, 100, 8, 0x18E3);
    tft->drawRoundRect(20, 80, SCREEN_WIDTH - 40, 100, 8, TFT_RED);

    tft->setTextColor(TFT_RED, 0x18E3);
    tft->setTextDatum(MC_DATUM);
    tft->setTextFont(4);
    tft->drawString("NO SD CARD", SCREEN_WIDTH / 2, 120);
    tft->setTextFont(2);
    tft->setTextColor(TFT_SILVER, 0x18E3);
    tft->drawString("Please Insert Card", SCREEN_WIDTH / 2, 150);
    tft->setTextColor(TFT_RED, 0x18E3);
    tft->setTextDatum(MC_DATUM);
    tft->setTextFont(4);
    tft->drawString("RaceBox Pro Firmware", SCREEN_WIDTH / 2, 280);
    tft->setFreeFont(NULL); // Reset to default for version
    tft->drawString("v2.1.28-ESP32SL", SCREEN_WIDTH / 2, 305);

    // --- FONT SAFETY ---
    tft->setTextSize(1);
    tft->setFreeFont(NULL);
    tft->setTextFont(1);
    tft->setTextPadding(0);
    return;
  }

  // --- RESULT CARDS ---

  // 1. INFO CARD
  int cardW = SCREEN_WIDTH - 20;
  int cardX = 10;
  int infoH = 80;

  tft->fillRoundRect(cardX, y, cardW, infoH, 6, 0x18E3); // Charcoal

  // Card Title
  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->setTextDatum(TL_DATUM);
  tft->setTextFont(1);
  tft->drawString("CARD INFO", cardX + 10, y + 5);

  // Type / Size / Used Grid
  // Row 1: Type | Size
  tft->setTextDatum(TL_DATUM);
  tft->setTextFont(2);
  tft->setTextColor(TFT_WHITE, 0x18E3);

  tft->drawString("Type:", cardX + 10, y + 25);
  tft->setTextColor(TFT_SKYBLUE, 0x18E3);
  tft->drawString(_sdResult.cardType, cardX + 60, y + 25);

  tft->setTextColor(TFT_WHITE, 0x18E3);
  tft->drawString("Size:", cardX + 160, y + 25);
  tft->setTextColor(TFT_ORANGE, 0x18E3);
  tft->drawString(_sdResult.sizeLabel, cardX + 200, y + 25);

  // Row 2: Used
  tft->setTextColor(TFT_WHITE, 0x18E3);
  tft->drawString("Used:", cardX + 10, y + 50);
  tft->setTextColor(TFT_WHITE, 0x18E3);
  tft->drawString(_sdResult.usedLabel, cardX + 60, y + 50);

  // 2. SPEED CARD
  y += infoH + 10;
  int speedH = 75;

  tft->fillRoundRect(cardX, y, cardW, speedH, 6, 0x10A2); // Slate

  tft->setTextColor(TFT_SILVER, 0x10A2);
  tft->setTextFont(1);
  tft->drawString("BENCHMARK", cardX + 10, y + 5);

  // Speed Grid
  // Read | Write
  // Split card width in 2
  int midX = cardX + cardW / 2;

  // Read
  tft->setTextColor(TFT_WHITE, 0x10A2);
  tft->setTextDatum(TC_DATUM);
  tft->setTextFont(1);
  tft->drawString("READ SPEED", cardX + cardW / 4, y + 20);

  tft->setTextFont(4);
  tft->setTextColor(TFT_GREEN, 0x10A2);
  tft->drawString(String(_sdResult.readSpeedKBps, 0) + " KB/s",
                  cardX + cardW / 4, y + 40);

  // Write
  tft->setTextColor(TFT_WHITE, 0x10A2);
  tft->setTextDatum(TC_DATUM);
  tft->setTextFont(1);
  tft->drawString("WRITE SPEED", midX + cardW / 4, y + 20);

  tft->setTextFont(4);
  tft->setTextColor(TFT_CYAN, 0x10A2);
  tft->drawString(String(_sdResult.writeSpeedKBps, 0) + " KB/s",
                  midX + cardW / 4, y + 40);

  tft->drawLine(midX, y + 20, midX, y + speedH - 10, TFT_SILVER);

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
}

void SettingsScreen::drawList(int scrollOffset, bool force) {
  TFT_eSPI *tft = _ui->getTft();

  // Highlight Back Arrow if selected
  uint16_t backColor =
      (_selectedIdx == -2) ? COLOR_HIGHLIGHT : _ui->getTextColor();

  if (force) {
    _ui->drawStatusBar(true);
    // Draw horizontal line after status bar
    tft->drawFastHLine(0, 20, SCREEN_WIDTH, COLOR_SECONDARY);
  }

  // List (starts at y=40, after header)
  int listY = 40;
  int itemH = 24; // Shrunk for density
  int maxY = 260; // Clean break before footer

  // Clear list area to prevent ghosts when scrolling
  if (force) {
    tft->fillRect(0, listY, SCREEN_WIDTH, maxY - listY,
                  _ui->getBackgroundColor());
  }

  int visibleItems = (maxY - listY) / itemH;

  for (int i = 0; i < visibleItems; i++) {
    int idx = scrollOffset + i;
    if (idx >= _settings.size())
      break;

    SettingItem &item = _settings[idx];
    int y = listY + (i * itemH);

    int sIdx = idx;
    bool stateChanged = (sIdx == _selectedIdx || sIdx == _lastSelectedIdx);

    if (force || stateChanged) {
      // Background
      uint16_t bgColor = (sIdx == _selectedIdx) ? _ui->getTextColor()
                                                : _ui->getBackgroundColor();
      uint16_t txtColor = (sIdx == _selectedIdx) ? _ui->getBackgroundColor()
                                                 : _ui->getTextColor();

      // Explicitly clear background
      tft->fillRect(0, y, SCREEN_WIDTH, itemH, bgColor);
      if (sIdx != _selectedIdx) {
        tft->drawFastHLine(0, y + itemH - 1, SCREEN_WIDTH, COLOR_SECONDARY);
      }

      // Name
      tft->setTextDatum(ML_DATUM); // Middle Left for centering
      tft->setTextFont(2);         // Standard Font
      tft->setTextSize(1);
      tft->setTextColor(txtColor, bgColor);

      String displayName = item.name;
      if (_currentMode == MODE_GNSS_CONFIG) {
        displayName = String(idx + 1) + ". " + item.name;
      }
      tft->drawString(displayName, 15, y + itemH / 2);

      // Value / Toggle / Action
      tft->setTextDatum(MR_DATUM); // Middle Right
      String valText = "";
      if (item.type == TYPE_VALUE) {
        if (item.currentOptionIdx >= 0 &&
            item.currentOptionIdx < item.options.size()) {
          valText = item.options[item.currentOptionIdx];
        }
      } else if (item.type == TYPE_TOGGLE) {
        valText = "";
      } else if (item.type == TYPE_ACTION) {
        valText = ">";
      }
      tft->drawString(valText, SCREEN_WIDTH - 15, y + itemH / 2);

      // Custom Render for Toggle
      if (item.type == TYPE_TOGGLE) {
        int swW = 24;
        int swH = 12;
        int swX = SCREEN_WIDTH - swW - 10;
        int swY = y + (itemH - swH) / 2;
        int r = swH / 2;

        if (item.checkState) {
          tft->fillRoundRect(swX, swY, swW, swH, r, TFT_GREEN);
          tft->fillCircle(swX + swW - r, swY + r, r - 2, TFT_WHITE);
        } else {
          tft->fillRoundRect(swX, swY, swW, swH, r, TFT_RED);
          tft->fillCircle(swX + r, swY + r, r - 2, TFT_WHITE);
        }
      }
    }
  }
  _lastSelectedIdx = _selectedIdx;

  // Draw bottom elements AFTER list
  if (force) {
    // Clear area between list and bottom (260 to 320)
    tft->fillRect(0, maxY, SCREEN_WIDTH, SCREEN_HEIGHT - maxY,
                  _ui->getBackgroundColor());

    // Back Arrow (Standardized Bottom-Left)
    tft->fillTriangle(10, 290, 25, 282, 25, 298, COLOR_ACCENT);

    // Scroll Buttons (Right Edge)
    int scrollX = SCREEN_WIDTH - 100;
    // Up Arrow
    if (scrollOffset > 0) {
      tft->fillTriangle(scrollX, 305, scrollX + 30, 305, scrollX + 15, 275,
                        COLOR_ACCENT);
    }

    // Down Arrow
    if (scrollOffset + visibleItems < _settings.size()) {
      tft->fillTriangle(scrollX + 50, 275, scrollX + 80, 275, scrollX + 65, 305,
                        COLOR_ACCENT);
    }
  }

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
}

// WiFi Functions

void SettingsScreen::drawWiFiList(bool force) {
  TFT_eSPI *tft = _ui->getTft();

  // FIX: Clear screen to prevent ghosting (Double Screen issue)
  tft->fillScreen(_ui->getBackgroundColor());
  _ui->drawStatusBar(true);

  drawHeader("WIFI SETUP");

  // List networks
  int listY = 60;
  int itemH = 24; // Standardized height

  for (int i = 0; i < _scanCount && i < 8;
       i++) { // More items with smaller font
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    bool isSecure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

    int y = listY + (i * itemH);

    // Background
    uint16_t bgColor = (i == _selectedWiFiIdx) ? TFT_BLUE : COLOR_BG;
    uint16_t txtColor = (i == _selectedWiFiIdx) ? TFT_WHITE : COLOR_TEXT;

    tft->fillRect(0, y, SCREEN_WIDTH, itemH, bgColor);

    // SSID
    tft->setTextColor(txtColor, bgColor);
    tft->setTextFont(2);
    tft->setTextDatum(ML_DATUM);
    tft->drawString(ssid, 15, y + itemH / 2);

    // Signal strength
    String signal = String(rssi) + "dBm";
    tft->setTextDatum(MR_DATUM);
    tft->drawString(signal, SCREEN_WIDTH - 40, y + itemH / 2);

    // Lock icon if secured
    if (isSecure) {
      tft->drawString("*", SCREEN_WIDTH - 15, y + itemH / 2);
    }
  }

  if (_scanCount == 0) {
    tft->setTextColor(COLOR_TEXT, COLOR_BG);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("No networks found", SCREEN_WIDTH / 2, 120);
    tft->fillCircle(SCREEN_WIDTH - 20, 290, 5, TFT_RED);
    tft->drawString("Scan to Refresh", SCREEN_WIDTH / 2, 290);
  }

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
}

void SettingsScreen::drawKeyboard(bool fullRedraw) {
  if (fullRedraw) {
    TFT_eSPI *tft = _ui->getTft();

    // FIX: Clear screen to prevent ghosting (Double Screen issue)
    tft->fillScreen(_ui->getBackgroundColor());
    _ui->drawStatusBar(true);

    drawHeader("ENTER PASSWORD");
    // Show SSID (shifted up)
    tft->setTextColor(COLOR_TEXT, COLOR_BG);
    tft->setTextDatum(TC_DATUM);
    tft->drawString(_targetSSID, SCREEN_WIDTH / 2, 45);
  }

  // Redraw password field (shifted up)
  TFT_eSPI *tft = _ui->getTft();
  tft->fillRect(10, 65, SCREEN_WIDTH - 20, 25, TFT_DARKGREY);
  tft->setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft->setTextDatum(TL_DATUM);
  tft->setTextFont(2);

  String displayPass = "";
  if (_showPassword) {
    displayPass = _enteredPass;
  } else {
    for (int i = 0; i < _enteredPass.length(); i++)
      displayPass += "*";
  }
  tft->drawString(displayPass, 15, 70);

  tft->setTextDatum(TR_DATUM);
  tft->setTextColor(COLOR_HIGHLIGHT, TFT_DARKGREY);
  tft->drawString(_showPassword ? "HIDE" : "SHOW", SCREEN_WIDTH - 15, 70);

  _keyboard.draw(tft, 95, _isUppercase);
}

void SettingsScreen::connectWiFi() {
  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(COLOR_BG);
  _ui->drawStatusBar(true);

  tft->setTextColor(TFT_WHITE, COLOR_BG);
  tft->setTextDatum(MC_DATUM);
  tft->drawString("Connecting...", SCREEN_WIDTH / 2, 120);

  // Use core manager for connection
  bool success = wifiManager.connect(_targetSSID.c_str(), _enteredPass.c_str());

  if (success) {
    tft->fillRect(0, 80, SCREEN_WIDTH, 100,
                  COLOR_BG); // Clear "Connecting..." area
    tft->setTextColor(TFT_GREEN, COLOR_BG);
    tft->drawString("Connected!", SCREEN_WIDTH / 2, 120);
    delay(2000);

    // Return to main settings only on success
    _currentMode = MODE_MAIN;
    loadSettings();
    tft->fillScreen(COLOR_BG);
    _ui->drawStatusBar(true);
    drawList(0, true);
  } else {
    tft->fillRect(0, 80, SCREEN_WIDTH, 100,
                  COLOR_BG); // Clear "Connecting..." area
    tft->setTextColor(TFT_RED, COLOR_BG);
    tft->drawString("Failed!", SCREEN_WIDTH / 2, 120);
    delay(2000);

    // Stay in keyboard mode on failure
    tft->fillScreen(COLOR_BG);
    _ui->drawStatusBar(true);
    drawKeyboard(true); // Full redraw to restore keyboard UI
  }
}

void SettingsScreen::startGraphicTest() {
  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(TFT_BLACK);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->setTextDatum(TL_DATUM);
  tft->setTextFont(2);
  tft->drawString("Running Benchmark...", 10, 10);

  runBenchmark();

  // After benchmark, we just show the results (already printed by runBenchmark)

  tft->setTextColor(TFT_GREEN, TFT_BLACK);
  tft->setTextDatum(BL_DATUM);
  tft->setTextFont(2);
  tft->drawString("< Exit", 10, SCREEN_HEIGHT - 10);
}

void SettingsScreen::updateGraphicTest() {
  // Check for any touch to exit
  UIManager::TouchPoint p = _ui->getTouchPoint();
  if (p.x != -1) {
    // Debounce a bit to avoid catching the touch that started the test
    static unsigned long lastTouch = 0;
    if (millis() - lastTouch > 500) {
      endGraphicTest();
      _currentMode = MODE_UTILITY;
      _ui->setTitle("UTILITY");
      loadSettings();
      _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
      _ui->drawStatusBar(true);
      drawList(0, true);
    }
    lastTouch = millis();
  }
}

void SettingsScreen::endGraphicTest() {
  // Cleanup if needed
}

void SettingsScreen::runBenchmark() {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long total = 0;
  int xPos = 5;
  int yPos = 28;
  int yStep = 16; // Tighter spacing to fit 14 items + header

  tft->fillScreen(TFT_BLACK);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->setTextFont(2);

  // Headers
  tft->setTextDatum(TL_DATUM);
  tft->setTextColor(TFT_GREEN, TFT_BLACK);
  tft->drawString("Benchmark", xPos, 5);
  tft->setTextDatum(TR_DATUM);
  tft->drawString("microseconds", SCREEN_WIDTH - 5, 5);
  tft->drawLine(0, 25, SCREEN_WIDTH, 25, TFT_DARKGREY);

  tft->setTextColor(TFT_WHITE, TFT_BLACK);

  // Helper lambda to run test and print result
  auto runAndPrint = [&](const char *name, unsigned long time) {
    tft->setTextDatum(TL_DATUM);
    tft->drawString(name, xPos, yPos);
    tft->setTextDatum(TR_DATUM);
    tft->drawString(String(time), SCREEN_WIDTH - 5, yPos);
    yPos += yStep;
    total += time;
    delay(500); // Slower animation per user request
  };

  runAndPrint("Screen fill", testFillScreen());
  runAndPrint("Text", testText());
  runAndPrint("Lines", testLines(TFT_CYAN));
  runAndPrint("Horiz/Vert Lines", testFastLines(TFT_RED, TFT_BLUE));
  runAndPrint("Rectangles", testRects(TFT_GREEN));
  runAndPrint("Rectangles-filled", testFilledRects(TFT_YELLOW, TFT_MAGENTA));
  runAndPrint("Circles", testCircles(10, TFT_WHITE));
  runAndPrint("Circles-filled", testFilledCircles(10, TFT_MAGENTA));
  runAndPrint("Triangles", testTriangles());
  runAndPrint("Triangles-filled", testFilledTriangles());
  runAndPrint("Rounded rects", testRoundRects());
  runAndPrint("Rounded rects-fill", testFilledRoundRects());

  // Total
  yPos += 5;
  tft->drawLine(0, yPos, SCREEN_WIDTH, yPos, TFT_DARKGREY);
  yPos += 5;
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  tft->setTextDatum(TL_DATUM);
  tft->drawString("Total", xPos, yPos);
  tft->setTextDatum(TR_DATUM);
  tft->drawString(String(total), SCREEN_WIDTH - 5, yPos);
}

unsigned long SettingsScreen::testFillScreen() {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long start = micros();
  tft->fillScreen(TFT_BLACK);
  tft->fillScreen(TFT_RED);
  tft->fillScreen(TFT_GREEN);
  tft->fillScreen(TFT_BLUE);
  tft->fillScreen(TFT_BLACK);
  return micros() - start;
}

unsigned long SettingsScreen::testText() {
  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(TFT_BLACK);
  unsigned long start = micros();
  tft->setTextColor(TFT_WHITE);
  tft->setTextDatum(TL_DATUM);
  tft->setTextFont(2);
  for (int i = 0; i < 500; i++) {
    tft->drawString("Hello World", 0, 0);
    tft->drawNumber(i, 100, 100);
  }
  return micros() - start;
}

unsigned long SettingsScreen::testLines(uint16_t color) {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long start;
  int x1, y1, x2, y2, w = tft->width(), h = tft->height();
  tft->fillScreen(TFT_BLACK);

  x1 = y1 = 0;
  y2 = h - 1;
  start = micros();
  for (x2 = 0; x2 < w; x2 += 6)
    tft->drawLine(x1, y1, x2, y2, color);
  x2 = w - 1;
  for (y2 = 0; y2 < h; y2 += 6)
    tft->drawLine(x1, y1, x2, y2, color);

  return micros() - start;
}

unsigned long SettingsScreen::testFastLines(uint16_t color1, uint16_t color2) {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long start;
  int w = tft->width(), h = tft->height();
  tft->fillScreen(TFT_BLACK);
  start = micros();
  for (int y = 0; y < h; y += 5)
    tft->drawFastHLine(0, y, w, color1);
  for (int x = 0; x < w; x += 5)
    tft->drawFastVLine(x, 0, h, color2);
  return micros() - start;
}

unsigned long SettingsScreen::testRects(uint16_t color) {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long start;
  int w = tft->width(), h = tft->height();
  tft->fillScreen(TFT_BLACK);
  start = micros();
  for (int x = 2; x < w; x += 6) {
    if (x + 2 > h)
      break;
    tft->drawRect(w / 2 - x / 2, h / 2 - x / 2, x, x, color);
  }
  return micros() - start;
}

unsigned long SettingsScreen::testFilledRects(uint16_t color1,
                                              uint16_t color2) {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long start;
  int w = tft->width(), h = tft->height();
  tft->fillScreen(TFT_BLACK);
  start = micros();
  for (int x = w - 1; x > 6; x -= 6) {
    if (x > h)
      continue;
    tft->fillRect(w / 2 - x / 2, h / 2 - x / 2, x, x, color1);
    tft->drawRect(w / 2 - x / 2, h / 2 - x / 2, x, x, color2);
  }
  return micros() - start;
}

unsigned long SettingsScreen::testFilledCircles(uint8_t radius,
                                                uint16_t color) {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long start;
  int w = tft->width(), h = tft->height();
  tft->fillScreen(TFT_BLACK);
  start = micros();
  for (int x = radius; x < w; x += radius * 2) {
    for (int y = radius; y < h; y += radius * 2) {
      tft->fillCircle(x, y, radius, color);
    }
  }
  return micros() - start;
}

unsigned long SettingsScreen::testCircles(uint8_t radius, uint16_t color) {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long start;
  int w = tft->width(), h = tft->height();
  tft->fillScreen(TFT_BLACK);
  start = micros();
  for (int x = 0; x < w + radius; x += radius * 2) {
    for (int y = 0; y < h + radius; y += radius * 2) {
      tft->drawCircle(x, y, radius, color);
    }
  }
  return micros() - start;
}

unsigned long SettingsScreen::testTriangles() {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long start;
  int w = tft->width(), h = tft->height();
  tft->fillScreen(TFT_BLACK);
  start = micros();
  for (int i = 0; i < w / 2; i += 5) {
    tft->drawTriangle(w / 2, h / 2 - i, w / 2 - i, h / 2 + i, w / 2 + i,
                      h / 2 + i, TFT_CYAN);
  }
  return micros() - start;
}

unsigned long SettingsScreen::testFilledTriangles() {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long start;
  int w = tft->width(), h = tft->height();
  tft->fillScreen(TFT_BLACK);
  start = micros();
  for (int i = w / 2; i > 10; i -= 5) {
    tft->fillTriangle(w / 2, h / 2 - i, w / 2 - i, h / 2 + i, w / 2 + i,
                      h / 2 + i, tft->color565(0, i, i));
  }
  return micros() - start;
}

unsigned long SettingsScreen::testRoundRects() {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long start;
  int w = tft->width(), h = tft->height();
  tft->fillScreen(TFT_BLACK);
  start = micros();
  for (int i = 0; i < w / 2 - 10; i += 6) {
    if (i * 2 + 10 > h)
      break;
    tft->drawRoundRect(i, i, w - 2 * i, h - 2 * i, 10, TFT_RED);
  }
  return micros() - start;
}

unsigned long SettingsScreen::testFilledRoundRects() {
  TFT_eSPI *tft = _ui->getTft();
  unsigned long start;
  int w = tft->width(), h = tft->height();
  tft->fillScreen(TFT_BLACK);
  start = micros();
  for (int i = 0; i < w / 2 - 10; i += 6) {
    if (i * 2 + 10 > h)
      break;
    tft->fillRoundRect(i, i, w - 2 * i, h - 2 * i, 10, tft->color565(i, 0, i));
  }
  return micros() - start;
}
