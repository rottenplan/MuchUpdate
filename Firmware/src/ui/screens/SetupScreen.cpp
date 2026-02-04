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
      drawTextField("USERNAME", _username, 50, _isEditingUsername, false);
      drawTextField("PASSWORD", _password, 95, _isEditingAccountPassword, true);
      break;
    case STEP_WIFI:
      if (_wifiSSID.length() == 0) {
        drawTextField("SSID", _wifiSSID, 50, _isEditingSSID, false);
        drawTextField("PASSWORD", _wifiPassword, 95, _isEditingPassword, true);
      } else {
        drawTextField("PASSWORD", _wifiPassword, 50, _isEditingPassword, true);
      }
      break;
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

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
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

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
}

// Updated Account Setup (Compact Layout)
void SetupScreen::drawAccountSetup(bool fullRedraw, char highlightChar) {
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

    // SKIP Button (Top Left - Added for Issue #3)
    drawButton("SKIP", 5, 5, 60, 25, false, 1);
  }

  // Ensure one field is active by default
  if (!_isEditingUsername && !_isEditingAccountPassword) {
    _isEditingUsername = true;
  }

  // Dual Field Layout (Fits 480x320: Y=50, Y=95)
  int field1Y = 50;
  int field2Y = 95;
  drawTextField("USERNAME", _username, field1Y, _isEditingUsername, false);
  drawTextField("PASSWORD", _password, field2Y, _isEditingAccountPassword,
                true);

  // Keyboard (Y=140)
  if (_isEditingUsername || _isEditingAccountPassword) {
    drawKeyboard(140, _isEditingAccountPassword, highlightChar);
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

void SetupScreen::drawKeyboard(int y, bool isPassword, char highlightChar) {
  _keyboard.draw(_ui->getTft(), y, _isUppercase, highlightChar);
}

// New WiFi Scan Screen
void SetupScreen::drawWiFiScan() {
  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(_ui->getBackgroundColor());

  // Header
  tft->setTextFont(1);
  tft->setTextSize(1);
  tft->setTextColor(COLOR_PRIMARY, _ui->getBackgroundColor());
  tft->setTextDatum(TC_DATUM);
  tft->drawString("SELECT WIFI NETWORK", SCREEN_WIDTH / 2, 8);

  // Simplified Header: Just SCAN icon/text on right
  drawButton("SCAN", SCREEN_WIDTH - 65, 5, 60, 25, false, 1);

  // SKIP Button (Top Left) - Added per user request
  drawButton("SKIP", 5, 5, 60, 25, false, 1);
  // tft->fillTriangle(15, 25, 30, 15, 30, 35, TFT_BLUE); // Header Triangle
  // REMOVED

  if (!_hasScanned) {
    // Draw Scanning Modal Box as requested
    int modalW = 200;
    int modalH = 80;
    int modalX = (SCREEN_WIDTH - modalW) / 2;
    int modalY = (SCREEN_HEIGHT - modalH) / 2;

    tft->fillRoundRect(modalX, modalY, modalW, modalH, 8,
                       _ui->getBackgroundColor());
    tft->drawRoundRect(modalX, modalY, modalW, modalH, 8, COLOR_PRIMARY);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(TFT_WHITE, _ui->getBackgroundColor());
    tft->setTextFont(2);
    tft->drawString("SCANNING...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

    // Perform Scan (Blocking)
    _scanCount = wifiManager.scanNetworks();
    _hasScanned = true;

    // Redraw screen content after scan
    tft->fillScreen(_ui->getBackgroundColor());
    drawWiFiScan();
    return;
  }

  // List Networks
  int startY = 45;
  int itemH = 36;
  int limit = 6;
  for (int i = 0; i < _scanCount && i < limit; i++) {
    int y = startY + (i * itemH);
    uint16_t color =
        (i == _lastWiFiTapIndex) ? COLOR_HIGHLIGHT : COLOR_SECONDARY;
    tft->drawRoundRect(10, y, SCREEN_WIDTH - 20, 32, 4, color);

    String ssid = wifiManager.getSSID(i);
    if (ssid.length() > 22)
      ssid = ssid.substring(0, 19) + "...";

    tft->setTextFont(2);
    tft->setTextColor(TFT_WHITE, _ui->getBackgroundColor());
    tft->setTextDatum(ML_DATUM);
    tft->drawString(ssid, 20, y + 16);

    int rssi = wifiManager.getRSSI(i);
    tft->setTextDatum(MR_DATUM);
    tft->setTextColor(COLOR_ACCENT, _ui->getBackgroundColor());
    tft->drawString(String(rssi) + " dB", SCREEN_WIDTH - 20, y + 16);
  }

  // Custom Manual Entry Option
  int visibleCount = (_scanCount > limit) ? limit : _scanCount;
  int my = startY + visibleCount * itemH;
  tft->drawRoundRect(10, my, SCREEN_WIDTH - 20, 32, 4, COLOR_PRIMARY);
  tft->setTextFont(2);
  tft->setTextColor(COLOR_PRIMARY, _ui->getBackgroundColor());
  tft->setTextDatum(MC_DATUM);
  tft->drawString("MANUAL SETUP", SCREEN_WIDTH / 2, my + 16);

  // SKIP Button (Bottom Right)
  drawButton("SKIP", SCREEN_WIDTH - 85, SCREEN_HEIGHT - 45, 75, 35, false, 1);
}

void SetupScreen::drawWiFiSetup(bool fullRedraw, char highlightChar) {
  TFT_eSPI *tft = _ui->getTft();

  if (fullRedraw) {
    tft->fillScreen(_ui->getBackgroundColor());
    // Use Font 1 (Standard) to match Account Setup header size
    tft->setTextFont(1);
    tft->setTextSize(1);
    tft->setTextColor(COLOR_PRIMARY,
                      _ui->getBackgroundColor()); // Standardized Header Color

    // Header
    // If selecting a known SSID, show it in header
    if (_wifiSSID.length() > 0) {
      tft->drawString("WIFI: " + _wifiSSID, SCREEN_WIDTH / 2,
                      5); // Y=5 to match Account Setup
    } else {
      tft->drawString("WIFI CONFIGURATION", SCREEN_WIDTH / 2, 5);
    }

    // BACK Button (Top Left) - Changed from SKIP per user request
    drawButton("BACK", 5, 5, 60, 25, false, 1);

    // DONE Button (Top Right)
    drawButton("DONE", SCREEN_WIDTH - 65, 5, 60, 25, true, 1);
  }

  // --- Layout Constants ---
  int field1Y = 50; // Moved down from 40 to prevent header overlap
  int field2Y = 95; // Moved down from 90
  int kbY = 140;    // Standardized Keyboard Y

  if (_wifiSSID.length() == 0) {
    // Manual Entry: Both SSID and Password
    // Highlight active field
    drawTextField("SSID", _wifiSSID, field1Y, _isEditingSSID, false);
    drawTextField("PASSWORD", _wifiPassword, field2Y, _isEditingPassword, true);

    // Draw keyboard if any field is active
    if (_isEditingSSID || _isEditingPassword) {
      drawKeyboard(kbY, _isEditingPassword, highlightChar);
    }
  } else {
    // Selection mode: Single Box (Password only)
    // "jadikan satu box saja" -> Remove SSID label
    // We update the header to show context if fullRedraw, but here we just
    // ensure clean layout

    // Clear the area above the box just in case
    tft->fillRect(0, 30, SCREEN_WIDTH, 20, _ui->getBackgroundColor());

    // Draw Password field at field1Y (50) - SAME as Username
    drawTextField("PASSWORD", _wifiPassword, field1Y, true, true);
    drawKeyboard(kbY, true, highlightChar);
  }

  // Unified Footer Triangle as requested ("keyboard di atas back button")
  // REMOVED per user request
  // tft->fillTriangle(15, SCREEN_HEIGHT - 30, 30, SCREEN_HEIGHT - 40, 30,
  //                   SCREEN_HEIGHT - 20, TFT_BLUE);
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
  // Rescan (Top Right Button: SCAN)
  if (y >= 5 && y <= 80 && x >= SCREEN_WIDTH - 100) {
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
      // Single Tap to select
      _wifiSSID = wifiManager.getSSID(i);
      _wifiPassword = "";
      _currentStep = STEP_WIFI;
      drawWiFiSetup(true); // Ensure clean transition
      return;
    }
  }

  // Manual Entry
  int visibleCount = (_scanCount > limit) ? limit : _scanCount;
  int manY = startY + visibleCount * itemH;
  if (y > manY && y < manY + 40) {
    _wifiSSID = "";
    _wifiPassword = "";
    _currentStep = STEP_WIFI;
    drawWiFiSetup();
    return;
  }

  // SKIP Button (Top Left) - Added per user request
  if (x < 80 && y < 50) {
    _currentStep = STEP_COMPLETE;
    drawComplete();
    return;
  }
}

// Touch Handlers with seasonal Y coordinates
void SetupScreen::handleWiFiTouch(int x, int y) {
  // BACK Button (Top Left) - Changed from SKIP
  if (x < 80 && y < 50) {
    // Return to scanning
    _currentStep = STEP_WIFI_SCAN;
    _hasScanned = false; // Trigger immediate rescan if they go back?
    // Or just show existing list? Let's show existing list if we have it.
    _scanCount = 0; // Force rescan for fresh results
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

  // New Constants (Must match drawWiFiSetup)
  int field1Y = 50;
  int field2Y = 95;
  int kbY = 140;

  if (_wifiSSID.length() == 0) {
    // Manual Entry Mode: (Y=40, Y=90, KB=140)

    // SSID Field
    if (y >= field1Y && y <= field1Y + 32) {
      _isEditingSSID = true;
      _isEditingPassword = false;
      drawWiFiSetup(false);
      return;
    }
    // Password field
    if (y >= field2Y && y <= field2Y + 32) {
      _isEditingPassword = true;
      _isEditingSSID = false;
      drawWiFiSetup(false);
      return;
    }
    // Keyboard at 140
    if (y >= kbY) {
      KeyboardComponent::KeyResult res = _keyboard.handleTouch(x, y, kbY);
      String &target = _isEditingSSID ? _wifiSSID : _wifiPassword;
      // ... same keyboard logic ...
      if (res.type == KeyboardComponent::KEY_CHAR) {
        char c = res.value;
        if (!_isUppercase && c >= 'A' && c <= 'Z')
          c += 32;
        handleKeyboardInput(target, c);
        drawWiFiSetup(false, c);
      } else if (res.type == KeyboardComponent::KEY_SHIFT) {
        _isUppercase = !_isUppercase;
        drawWiFiSetup(false, 1);
      } else if (res.type == KeyboardComponent::KEY_DEL) {
        if (target.length() > 0)
          target.remove(target.length() - 1);
        drawWiFiSetup(false, 2);
      } else if (res.type == KeyboardComponent::KEY_SPACE) {
        target += " ";
        drawWiFiSetup(false, ' ');
      } else if (res.type == KeyboardComponent::KEY_OK) {
        // Move focus or Finish
        if (_isEditingSSID) {
          _isEditingSSID = false;
          _isEditingPassword = true;
          drawWiFiSetup(false);
        } else {
          _isEditingSSID = false;
          _isEditingPassword = false;
          drawWiFiSetup(false, 3);
          delay(100);
          drawWiFiSetup(false);
        }
      }
      return;
    }
  } else {
    // Selection Mode: (PW Field=90, KB=140)
    if (y >= field2Y && y <= field2Y + 32) {
      _isEditingPassword = true;
      _isEditingSSID = false;
      drawWiFiSetup(false);
      return;
    }
    // Keyboard at 140
    if (y >= kbY) {
      KeyboardComponent::KeyResult res = _keyboard.handleTouch(x, y, kbY);
      if (res.type == KeyboardComponent::KEY_CHAR) {
        char c = res.value;
        if (!_isUppercase && c >= 'A' && c <= 'Z')
          c += 32;
        handleKeyboardInput(_wifiPassword, c);
        drawWiFiSetup(false, c);
      } else if (res.type == KeyboardComponent::KEY_SHIFT) {
        _isUppercase = !_isUppercase;
        drawWiFiSetup(false, 1);
      } else if (res.type == KeyboardComponent::KEY_DEL) {
        if (_wifiPassword.length() > 0)
          _wifiPassword.remove(_wifiPassword.length() - 1);
        drawWiFiSetup(false, 2);
      } else if (res.type == KeyboardComponent::KEY_SPACE) {
        _wifiPassword += " ";
        drawWiFiSetup(false, ' ');
      } else if (res.type == KeyboardComponent::KEY_OK) {
        _isEditingPassword = false;
        drawWiFiSetup(false, 3);
        delay(100);
        drawWiFiSetup(false);
      }
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
  // NEXT Button (Top Right: 60x25 at SCREEN_WIDTH-65, 5)
  if (y >= 5 && y <= 35 && x >= SCREEN_WIDTH - 65 && x <= SCREEN_WIDTH - 5) {
    if (_username.length() > 0 && _password.length() > 0) {
      nextStep();
    }
    return;
  }

  // SKIP Button (Top Left: 100x80 area for ease)
  if (x < 100 && y < 80) {
    _currentStep = STEP_COMPLETE;
    drawComplete();
    return;
  }

  // Check field selection
  if (y >= 50 && y <= 82) {
    _isEditingUsername = true;
    _isEditingAccountPassword = false;
    drawAccountSetup(false);
    return;
  }
  if (y >= 95 && y <= 127) {
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
      drawAccountSetup(false, c);
    } else if (res.type == KeyboardComponent::KEY_SHIFT) {
      _isUppercase = !_isUppercase;
      drawAccountSetup(false, 1);
    } else if (res.type == KeyboardComponent::KEY_DEL) {
      if (target.length() > 0)
        target.remove(target.length() - 1);
      drawAccountSetup(false, 2);
    } else if (res.type == KeyboardComponent::KEY_SPACE) {
      target += " ";
      drawAccountSetup(false, ' ');
    } else if (res.type == KeyboardComponent::KEY_OK) {
      if (_isEditingUsername) {
        // Transition to Password field
        _isEditingUsername = false;
        _isEditingAccountPassword = true;
        drawAccountSetup(false);
      } else {
        // Submit if both filled
        if (_username.length() > 0 && _password.length() > 0) {
          nextStep();
        } else {
          // Visual feedback for Enter key
          drawAccountSetup(false, 3);
          delay(100);
          drawAccountSetup(false);
        }
      }
    }
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
