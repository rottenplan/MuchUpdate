#include "SetupScreen.h"
#include "../../config.h"
#include "../../core/SyncManager.h"
#include "../../core/WiFiManager.h"
#include "../fonts/Org_01.h"

extern WiFiManager wifiManager;

// Update onShow to reset new variables
void SetupScreen::onShow() {
  _currentStep = STEP_WELCOME;
  _username = "";
  _wifiSSID = "";
  _wifiPassword = "";
  _isEditingUsername = false;
  _isEditingSSID = false;
  _isEditingPassword = false;
  _cursorVisible = true;
  _lastTouchTime = 0;
  _lastTapY = -1;
  _isUppercase = true;
  _showPassword = false;
  _scanCount = 0;    // Reset
  _scrollOffset = 0; // Reset
  _hasScanned = false;
  _lastWiFiTapIndex = -1;
  _lastWiFiTapTime = 0;

  drawWelcome();
}

// Update update() to handle new step with Compact Coordinates
void SetupScreen::update() {
  TFT_eSPI *tft = _ui->getTft();
  UIManager::TouchPoint tp = _ui->getTouchPoint();

  // Handle cursor blink
  if (millis() - _lastBlinkTime > 500) {
    _cursorVisible = !_cursorVisible;
    _lastBlinkTime = millis();

    switch (_currentStep) {
    case STEP_ACCOUNT:
      if (_isEditingUsername)
        drawTextField("USERNAME", _username, 40, _isEditingUsername, false);
      else if (_isEditingAccountPassword)
        drawTextField("PASSWORD", _password, 90, _isEditingAccountPassword,
                      true);
      break;
    case STEP_WIFI:
      if (_wifiSSID.length() == 0) {
        if (_isEditingSSID)
          drawTextField("SSID", _wifiSSID, 50, _isEditingSSID, false);
        else if (_isEditingPassword)
          drawTextField("PASSWORD", _wifiPassword, 100, _isEditingPassword,
                        true);
      } else {
        if (_isEditingPassword)
          drawTextField("PASSWORD", _wifiPassword, 80, true, true);
      }
      break;
    default:
      break;
    }
  }

  // Handle touch
  if (tp.x >= 0 && tp.y >= 0) {
    if (millis() - _lastTouchTime < 200)
      return;
    _lastTouchTime = millis();

    switch (_currentStep) {
    case STEP_WELCOME:
      handleWelcomeTouch(tp.x, tp.y);
      break;
    case STEP_ACCOUNT:
      handleAccountTouch(tp.x, tp.y);
      break;
    case STEP_WIFI_SCAN:
      handleWiFiScanTouch(tp.x, tp.y);
      break;
    case STEP_WIFI:
      handleWiFiTouch(tp.x, tp.y);
      break;
    case STEP_COMPLETE:
      handleCompleteTouch(tp.x, tp.y);
      break;
    }
  }
}

// ===== DRAWING METHODS =====

void SetupScreen::drawWelcome() {
  TFT_eSPI *tft = _ui->getTft();
  // Clear entire screen
  tft->fillScreen(_ui->getBackgroundColor());

  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->setTextColor(COLOR_PRIMARY, _ui->getBackgroundColor());
  tft->setTextDatum(MC_DATUM);

  // Title
  tft->setTextSize(2);
  tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
  tft->drawString("WELCOME TO", SCREEN_WIDTH / 2, 75);

  tft->setTextSize(4);
  tft->setTextColor(COLOR_PRIMARY, _ui->getBackgroundColor());
  tft->drawString("MUCH RACING", SCREEN_WIDTH / 2, 105);

  tft->setTextSize(2);
  tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
  tft->drawString("LET'S GET STARTED", SCREEN_WIDTH / 2, 145);

  // Continue button (larger)
  drawButton("TAP TO BEGIN", SCREEN_WIDTH / 2 - 120, 200, 240, 60, false);
}

void SetupScreen::drawComplete() {
  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(_ui->getBackgroundColor());

  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->setTextColor(TFT_GREEN, _ui->getBackgroundColor());
  tft->setTextDatum(MC_DATUM);

  // Success message
  tft->drawString("SETUP", SCREEN_WIDTH / 2, 80);
  tft->drawString("COMPLETE!", SCREEN_WIDTH / 2, 110);

  tft->setTextSize(1);
  tft->setTextColor(COLOR_TEXT, COLOR_BG);

  if (_username.length() > 0) {
    String msg = "WELCOME " + _username + "!";
    tft->drawString(msg, SCREEN_WIDTH / 2, 165);
  }

  // Continue button (larger and lower)
  drawButton("START RACING", SCREEN_WIDTH / 2 - 100, 210, 200, 50, false);
}

// Updated Account Setup (Compact Layout)
void SetupScreen::drawAccountSetup(bool fullRedraw) {
  TFT_eSPI *tft = _ui->getTft();

  if (fullRedraw) {
    tft->fillScreen(_ui->getBackgroundColor());

    // Header (Use Small Font 1 to avoid overlap with buttons)
    tft->setTextFont(1);
    tft->setTextSize(1);
    tft->setTextColor(COLOR_PRIMARY, _ui->getBackgroundColor());
    tft->setTextDatum(TC_DATUM);
    tft->drawString("ACCOUNT SETUP", SCREEN_WIDTH / 2, 5);

    // Nav Buttons (Compact)
    drawButton("NEXT", SCREEN_WIDTH - 65, 5, 60, 25,
               _username.length() > 0 && _password.length() > 0, 1);
  }

  // Fields (Adjusted for 480x320: Y=40, Y=90)
  drawTextField("USERNAME", _username, 40, _isEditingUsername, false);
  drawTextField("PASSWORD", _password, 90, _isEditingAccountPassword, true);

  // Keyboard (Y=140)
  if (_isEditingUsername || _isEditingAccountPassword) {
    drawKeyboard(140, _isEditingAccountPassword);
  }
}

void SetupScreen::drawTextField(const char *label, String value, int y,
                                bool isActive, bool isPassword) {
  TFT_eSPI *tft = _ui->getTft();

  // Calculate centered box
  int boxW = 260; // Reduced from 300 for even cleaner layout
  if (boxW > SCREEN_WIDTH - 20)
    boxW = SCREEN_WIDTH - 20; // Safety constraint
  int boxX = (SCREEN_WIDTH - boxW) / 2;

  // Use Org_01 (Tiny Font) for the Label
  tft->setFreeFont(&Org_01);
  tft->setTextSize(1);
  tft->setTextDatum(BL_DATUM);
  tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
  tft->drawString(label, boxX, y);

  // Field background (Start box at y+2 to give gap, taller box)
  // Box: y+2 to y+32 (Height 30, increased from 25)
  uint16_t borderColor = isActive ? COLOR_PRIMARY : COLOR_SECONDARY;
  tft->drawRect(boxX, y + 2, boxW, 30, borderColor);
  tft->fillRect(boxX + 1, y + 3, boxW - 2, 28, TFT_DARKGREY);

  // Switch back to Standard Font 1 for Value
  tft->setTextFont(1);
  tft->setTextSize(1);

  // Field value (Masking removed as per user request)
  String displayValue = value;

  // Draw Value centered in box (adjusted for taller box)
  tft->setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft->setTextDatum(ML_DATUM);
  tft->drawString(displayValue, boxX + 5, y + 17);

  // Password Toggle Button for WiFi (Right aligned in box)
  // Password Toggle Button Removed (Always show)

  // Cursor
  if (isActive && _cursorVisible) {
    int cursorX = boxX + 5 + tft->textWidth(displayValue);
    // Cursor Line: y+7 to y+25 (18px tall, adjusted for taller box)
    tft->drawFastVLine(cursorX, y + 8, 18, COLOR_PRIMARY);
  }
}

void SetupScreen::drawButton(const char *label, int x, int y, int w, int h,
                             bool isHighlighted, int fontSize) {
  TFT_eSPI *tft = _ui->getTft();

  uint16_t bgColor = isHighlighted ? COLOR_PRIMARY : _ui->getBackgroundColor();
  uint16_t borderColor = isHighlighted ? COLOR_PRIMARY : COLOR_SECONDARY;
  uint16_t textColor =
      isHighlighted ? _ui->getBackgroundColor() : _ui->getTextColor();

  tft->fillRect(x, y, w, h, bgColor);
  tft->drawRect(x, y, w, h, borderColor);

  tft->setTextSize(fontSize);
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(textColor, bgColor);
  tft->drawString(label, x + w / 2, y + h / 2);
}

void SetupScreen::drawKeyboard(int y, bool isPassword) {
  _keyboard.draw(_ui->getTft(), y, _isUppercase);
}

// New WiFi Scan Screen
void SetupScreen::drawWiFiScan() {
  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(_ui->getBackgroundColor());

  // Header (Back to Size 1)
  tft->setTextFont(1);
  tft->setTextSize(1);
  tft->setTextColor(COLOR_PRIMARY, _ui->getBackgroundColor());
  tft->setTextDatum(TC_DATUM);
  tft->drawString("SELECT WIFI NETWORK", SCREEN_WIDTH / 2, 5);

  // Nav Buttons (Compact)
  drawButton("SCAN", SCREEN_WIDTH - 65, 5, 60, 25, false, 1);

  if (!_hasScanned) {
    tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
    tft->setTextDatum(MC_DATUM);
    tft->drawString("Scanning...", SCREEN_WIDTH / 2, 120);

    // Perform Scan (Blocking)
    _scanCount = wifiManager.scanNetworks();
    _hasScanned = true;

    // Clear "Scanning..." text area only, don't wipe whole screen (prevents
    // flicker)
    tft->fillRect(0, 50, SCREEN_WIDTH, SCREEN_HEIGHT - 50,
                  _ui->getBackgroundColor());
  }

  // List Networks
  int startY = 50;
  int itemH = 40;
  int limit = 4; // Limit to 4 to keep "Manual" visible
  for (int i = 0; i < _scanCount && i < limit; i++) {
    int y = startY + (i * itemH);
    tft->drawRect(20, y, SCREEN_WIDTH - 40, 30, COLOR_SECONDARY);
    String ssid = wifiManager.getSSID(i);
    if (ssid.length() > 18)
      ssid = ssid.substring(0, 15) + "...";
    // SSID List (Size 1)
    tft->setTextSize(1);
    tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
    tft->setTextDatum(ML_DATUM);
    tft->drawString(ssid, 30, y + 15);
    int rssi = wifiManager.getRSSI(i);
    tft->setTextDatum(MR_DATUM);
    tft->drawString(String(rssi) + "dB", SCREEN_WIDTH - 30, y + 15);
  }

  // Custom Manual Entry Option (Size 1)
  int visibleCount = (_scanCount > limit) ? limit : _scanCount;
  int y = startY + visibleCount * itemH;
  tft->setTextSize(1); // Back to 1
  tft->setTextColor(COLOR_HIGHLIGHT, _ui->getBackgroundColor());
  tft->setTextDatum(MC_DATUM);
  tft->drawString("Manually Enter SSID", SCREEN_WIDTH / 2, y + 15);
}

// Updated WiFi Setup (Compact Layout)
void SetupScreen::drawWiFiSetup(bool fullRedraw) {
  TFT_eSPI *tft = _ui->getTft();

  if (fullRedraw) {
    tft->fillScreen(_ui->getBackgroundColor());
    // Header (Small Font 1)
    tft->setTextFont(1);
    tft->setTextSize(1);
    tft->setTextColor(COLOR_PRIMARY, _ui->getBackgroundColor());
    tft->setTextDatum(TC_DATUM);
    tft->drawString("ENTER WIFI PASSWORD", SCREEN_WIDTH / 2, 5);

    // Nav Buttons (Compact)
    drawButton("BACK", 5, 5, 60, 25, false, 1);
    drawButton("CONN", SCREEN_WIDTH - 65, 5, 60, 25, _wifiSSID.length() > 0, 1);

    // Status Indicator (Small)
    tft->setTextDatum(TC_DATUM);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(1);
    if (wifiManager.isConnected()) {
      tft->setTextColor(TFT_GREEN, _ui->getBackgroundColor());
      tft->drawString("STATUS: CONNECTED", SCREEN_WIDTH / 2, 28);
    } else {
      tft->setTextColor(TFT_RED, _ui->getBackgroundColor());
      tft->drawString("STATUS: NOT CONNECTED", SCREEN_WIDTH / 2, 28);
    }
  }

  // Fields (Conditional logic to remove redundancy)
  if (_wifiSSID.length() == 0) {
    // Manual Entry Mode: Show both SSID and Password boxes
    drawTextField("SSID", _wifiSSID, 50, _isEditingSSID, false);
    drawTextField("PASSWORD", _wifiPassword, 100, _isEditingPassword, true);

    // Keyboard (Y=150)
    if (_isEditingSSID || _isEditingPassword) {
      drawKeyboard(150, true);
    }
  } else {
    // Selection Mode: SSID is known, just show it as a label and focus on
    // Password
    tft->setTextFont(1);
    tft->setTextSize(1);
    tft->setTextColor(COLOR_HIGHLIGHT, _ui->getBackgroundColor());
    tft->setTextDatum(TC_DATUM);
    tft->drawString("Network: " + _wifiSSID, SCREEN_WIDTH / 2, 40);

    drawTextField("PASSWORD", _wifiPassword, 80, true, true);
    _isEditingPassword = true;
    _isEditingSSID = false;

    // Keyboard (Y=130 - Higher up since there's only one box)
    drawKeyboard(130, true);
  }
}

// ===== TOUCH HANDLERS =====

void SetupScreen::handleWelcomeTouch(int x, int y) {
  // Check if "TAP TO BEGIN" button was pressed
  // Button is 240x60 at position (SCREEN_WIDTH/2 - 120, 200)
  if (y >= 200 && y <= 260 && x >= SCREEN_WIDTH / 2 - 120 &&
      x <= SCREEN_WIDTH / 2 + 120) {
    nextStep();
  }
}

void SetupScreen::handleWiFiScanTouch(int x, int y) {
  // Skip logic removed

  // Rescan (Top Right Button: SCAN)
  // Button is 60x25 at SCREEN_WIDTH-65, 5
  if (y >= 5 && y <= 30 && x >= SCREEN_WIDTH - 65 && x <= SCREEN_WIDTH - 5) {
    _scanCount = 0;
    _hasScanned = false;
    drawWiFiScan();
    return;
  }
  // List Selection
  int startY = 50;
  int itemH = 40;
  int limit = 4;
  for (int i = 0; i < _scanCount && i < limit; i++) {
    int itemY = startY + (i * itemH);
    if (y > itemY && y < itemY + 30) {
      if (_lastWiFiTapIndex == i && (millis() - _lastWiFiTapTime < 500)) {
        // Double Tap confirmed!
        _wifiSSID = wifiManager.getSSID(i);
        _wifiPassword = "";
        _currentStep = STEP_WIFI;
        drawWiFiSetup();
        _lastWiFiTapIndex = -1;
      } else {
        // First Tap - Select/Highlight (Redraw list?)
        // ideally we should highlight it visually. For now just track it.
        _lastWiFiTapIndex = i;
        _lastWiFiTapTime = millis();
        // Force redraw to show selection (if we implement visual selection
        // later) drawWiFiScan();
      }
      return;
    }
  }
  // Manual Entry (Y+20 spacing for MC_DATUM)
  int visibleCount = (_scanCount > limit) ? limit : _scanCount;
  int manY = startY + visibleCount * itemH;
  if (y > manY && y < manY + 40) { // Height ~40px hit area
    _wifiSSID = "";
    _wifiPassword = "";
    _currentStep = STEP_WIFI;
    drawWiFiSetup();
  }
}

// Touch Handlers with seasonal Y coordinates
void SetupScreen::handleWiFiTouch(int x, int y) {
  // Nav Buttons (BACK, CONN)
  // BACK Button: 5, 5, 60, 25
  if (y >= 5 && y <= 35 && x >= 5 && x <= 70) {
    _currentStep = STEP_WIFI_SCAN;
    drawWiFiScan();
    return;
  }
  // CONN Button: SCREEN_WIDTH-65, 5, 60, 25
  if (y >= 5 && y <= 35 && x >= SCREEN_WIDTH - 70 && x <= SCREEN_WIDTH - 5) {
    if (_wifiSSID.length() > 0) {
      // Draw Connecting Modal
      TFT_eSPI *tft = _ui->getTft();
      int modalW = 240;
      int modalH = 80;
      int modalX = (SCREEN_WIDTH - modalW) / 2;
      int modalY = (SCREEN_HEIGHT - modalH) / 2;

      tft->fillRoundRect(modalX, modalY, modalW, modalH, 8,
                         _ui->getBackgroundColor());
      tft->drawRoundRect(modalX, modalY, modalW, modalH, 8, COLOR_PRIMARY);
      tft->setTextDatum(MC_DATUM);
      tft->setTextColor(TFT_WHITE, _ui->getBackgroundColor());
      tft->setTextSize(2);
      tft->drawString("CONNECTING...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

      // Perform Connection (Blocking ~10s)
      bool success =
          wifiManager.connect(_wifiSSID.c_str(), _wifiPassword.c_str());

      if (success) {
        tft->fillRect(modalX + 5, modalY + 5, modalW - 10, modalH - 10,
                      _ui->getBackgroundColor());
        tft->setTextColor(TFT_GREEN, _ui->getBackgroundColor());
        tft->drawString("CONNECTED!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
        delay(1000);
        nextStep();
      } else {
        tft->fillRect(modalX + 5, modalY + 5, modalW - 10, modalH - 10,
                      _ui->getBackgroundColor());
        tft->setTextColor(TFT_RED, _ui->getBackgroundColor());
        tft->drawString("FAILED!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
        delay(1500);
        drawWiFiSetup(true); // Redraw to clear modal
      }
    }
    return;
  }

  int boxW = 300;
  int boxX = (SCREEN_WIDTH - boxW) / 2;
  int boxRight = boxX + boxW;

  if (_wifiSSID.length() == 0) {
    // Manual Entry Mode: (Y=50, Y=100, KB=150)
    if (y >= 50 && y <= 82) {
      _isEditingSSID = true;
      _isEditingPassword = false;
      drawWiFiSetup(false);
      return;
    }
    // Password field at 100
    if (y >= 100 && y <= 132) {
      _isEditingPassword = true;
      _isEditingSSID = false;
      drawWiFiSetup(false);
      return;
    }
    // Keyboard at 150
    if (y >= 150) {
      KeyboardComponent::KeyResult res = _keyboard.handleTouch(x, y, 150);
      String &target = _isEditingSSID ? _wifiSSID : _wifiPassword;
      // ... same keyboard logic ...
      if (res.type == KeyboardComponent::KEY_CHAR) {
        char c = res.value;
        if (!_isUppercase && c >= 'A' && c <= 'Z')
          c += 32;
        handleKeyboardInput(target, c);
      } else if (res.type == KeyboardComponent::KEY_SHIFT)
        _isUppercase = !_isUppercase;
      else if (res.type == KeyboardComponent::KEY_DEL) {
        if (target.length() > 0)
          target.remove(target.length() - 1);
      } else if (res.type == KeyboardComponent::KEY_SPACE)
        target += " ";
      else if (res.type == KeyboardComponent::KEY_OK) {
        _isEditingSSID = false;
        _isEditingPassword = false;
      }
      drawWiFiSetup(false);
      return;
    }
  } else {
    // Selection Mode: (PW Field=80, KB=130)
    if (y >= 80 && y <= 112) {
      _isEditingPassword = true;
      _isEditingSSID = false;
      drawWiFiSetup(false);
      return;
    }
    // Keyboard at 130
    if (y >= 130) {
      KeyboardComponent::KeyResult res = _keyboard.handleTouch(x, y, 130);
      if (res.type == KeyboardComponent::KEY_CHAR) {
        char c = res.value;
        if (!_isUppercase && c >= 'A' && c <= 'Z')
          c += 32;
        handleKeyboardInput(_wifiPassword, c);
      } else if (res.type == KeyboardComponent::KEY_SHIFT)
        _isUppercase = !_isUppercase;
      else if (res.type == KeyboardComponent::KEY_DEL) {
        if (_wifiPassword.length() > 0)
          _wifiPassword.remove(_wifiPassword.length() - 1);
      } else if (res.type == KeyboardComponent::KEY_SPACE)
        _wifiPassword += " ";
      else if (res.type == KeyboardComponent::KEY_OK) {
        _isEditingPassword = false;
      }
      drawWiFiSetup(false);
      return;
    }
  }
}

void SetupScreen::handleCompleteTouch(int x, int y) {
  // Button "START RACING" is 200x50 at Y=210
  if (y >= 210 && y <= 260 && x >= SCREEN_WIDTH / 2 - 100 &&
      x <= SCREEN_WIDTH / 2 + 100) {
    saveSetupComplete();
    _ui->switchScreen(SCREEN_MENU);
  }
}

void SetupScreen::handleKeyboardInput(String &target, char key) {
  if (target.length() < 30)
    target += key;
}

void SetupScreen::handleAccountTouch(int x, int y) {
  // NEXT Button (Compact Top Right: 60x25 at SCREEN_WIDTH-65, 5)
  if (y >= 5 && y <= 35 && x >= SCREEN_WIDTH - 65 && x <= SCREEN_WIDTH - 5) {
    if (_username.length() > 0 && _password.length() > 0) {
      nextStep();
    }
    return;
  }

  // Check Username field (Y=40)
  if (y >= 40 && y <= 72) {
    _isEditingUsername = true;
    _isEditingAccountPassword = false;
    _isEditingSSID = false;
    _isEditingPassword = false;
    drawAccountSetup(false);
    return;
  }

  // Calculate text field position (boxW=260)
  int boxW = 260;
  if (boxW > SCREEN_WIDTH - 20)
    boxW = SCREEN_WIDTH - 20;
  int boxX = (SCREEN_WIDTH - boxW) / 2;
  int boxRight = boxX + boxW;

  // Password field (Y=90)
  if (y >= 90 && y <= 122) {
    _isEditingUsername = false;
    _isEditingAccountPassword = true;
    drawAccountSetup(false);
    return;
  }

  // Keyboard (Y=140)
  if ((_isEditingUsername || _isEditingAccountPassword) && y >= 140) {
    KeyboardComponent::KeyResult res = _keyboard.handleTouch(x, y, 140);
    String &target = _isEditingUsername ? _username : _password;

    if (res.type == KeyboardComponent::KEY_CHAR) {
      char c = res.value;
      if (!_isUppercase && c >= 'A' && c <= 'Z')
        c += 32; // To Lowercase
      handleKeyboardInput(target, c);
    } else if (res.type == KeyboardComponent::KEY_SHIFT)
      _isUppercase = !_isUppercase;
    else if (res.type == KeyboardComponent::KEY_DEL) {
      if (target.length() > 0)
        target.remove(target.length() - 1);
    } else if (res.type == KeyboardComponent::KEY_SPACE)
      target += " ";
    else if (res.type == KeyboardComponent::KEY_OK) {
      _isEditingUsername = false;
      _isEditingAccountPassword = false;
    }

    drawAccountSetup(false);
    return;
  }
}

// Updated nextStep routing
void SetupScreen::nextStep() {
  switch (_currentStep) {
  case STEP_WELCOME:
    _currentStep = STEP_WIFI_SCAN;
    drawWiFiScan();
    break;
  case STEP_ACCOUNT:
    // Enhanced Synchronizing UI (Modal Style)
    {
      TFT_eSPI *tft = _ui->getTft();
      int modalW = 280;
      int modalH = 100;
      int modalX = (SCREEN_WIDTH - modalW) / 2;
      int modalY = (SCREEN_HEIGHT - modalH) / 2;

      // Draw Modal Shadow/Dimming (simple fill)
      tft->fillRoundRect(modalX + 4, modalY + 4, modalW, modalH, 8, TFT_BLACK);
      // Main Box
      tft->fillRoundRect(modalX, modalY, modalW, modalH, 8,
                         _ui->getBackgroundColor());
      tft->drawRoundRect(modalX, modalY, modalW, modalH, 8, COLOR_PRIMARY);

      // Header
      tft->setFreeFont(&Org_01);
      tft->setTextSize(1);
      tft->setTextColor(COLOR_ACCENT, _ui->getBackgroundColor());
      tft->setTextDatum(TC_DATUM);
      tft->drawString("CLOUD SYNC", SCREEN_WIDTH / 2, modalY + 10);

      // Message
      tft->setTextFont(1);
      tft->setTextSize(2);
      tft->setTextColor(TFT_WHITE, _ui->getBackgroundColor());
      tft->setTextDatum(MC_DATUM);
      tft->drawString("Synchronizing...", SCREEN_WIDTH / 2, modalY + 45);

      // Stylized Progress Bar (Static but looks active)
      int barW = 200;
      int barH = 8;
      int barX = (SCREEN_WIDTH - barW) / 2;
      int barY = modalY + 75;
      tft->drawRect(barX, barY, barW, barH, COLOR_SECONDARY);
      tft->fillRect(barX + 2, barY + 2, barW / 2, barH - 4,
                    COLOR_ACCENT); // 50% "fake" progress

      extern SyncManager syncManager;
      // Perform First Sync
      bool syncSuccess = syncManager.performFirstSync(
          API_URL, _username.c_str(), _password.c_str());

      // Update Result in Modal
      tft->fillRect(modalX + 5, modalY + 35, modalW - 10, 30,
                    _ui->getBackgroundColor());
      if (syncSuccess) {
        tft->setTextColor(TFT_GREEN, _ui->getBackgroundColor());
        tft->drawString("SYNC SUCCESS!", SCREEN_WIDTH / 2, modalY + 45);
        tft->fillRect(barX + 2, barY + 2, barW - 4, barH - 4,
                      TFT_GREEN); // Full Green Bar
      } else {
        tft->setTextColor(TFT_RED, _ui->getBackgroundColor());
        tft->drawString("SYNC FAILED", SCREEN_WIDTH / 2, modalY + 45);
      }
      delay(1500);
    }

    // Save Account locally as fallback/cache
    if (_username.length() > 0) {
      Preferences prefs;
      prefs.begin("muchrace", false);
      prefs.putString("username", _username);
      if (_password.length() > 0)
        prefs.putString("password", _password);
      prefs.end();
    }
    // Go to Complete
    _currentStep = STEP_COMPLETE;
    drawComplete();
    break;
  case STEP_WIFI_SCAN:
    // Handled in touch, but logic flow: Scan -> Wifi Setup
    break;
  case STEP_WIFI:
    _currentStep = STEP_ACCOUNT;
    drawAccountSetup();
    break;
  default:
    break;
  }
}

void SetupScreen::saveSetupComplete() {
  Preferences prefs;
  prefs.begin("muchrace", false);
  prefs.putBool("setup_done", true);
  prefs.end();

  Serial.println("Setup marked as complete!");
}
