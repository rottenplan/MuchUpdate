#include "DragMeterScreen.h"
#include "../../core/GPSManager.h"
#include "../../core/SessionManager.h"
#include "../fonts/Org_01.h"
#include <Preferences.h>
#include <TFT_eSPI.h>

extern GPSManager gpsManager;
extern SessionManager sessionManager;

// Tentukan subset disiplin untuk dilacak
// 0-60 km/h
// 0-100 km/h
// 100-200 km/h
// 402m (1/4 Mile)

// 0-100 km/h
// 100-200 km/h
// 402m (1/4 Mile)

void DragMeterScreen::onShow() {
  _state = STATE_MENU;
  _displayMode = DISPLAY_NORMAL;
  _selectedBtn = -1;
  _selectedMenuIdx = -1;
  _selectedDragModeIdx = -1;
  _selectedPredictiveIdx = -1;
  _lastTapIdx = -1;
  _lastTapTime = 0;
  _menuItems = {"DRAG MODE", "DRAG SCREEN", "PREDICTIVE", "SUMMARY"};
  _dragModeItems = {"SPEED", "DISTANCE", "CUSTOM"};
  _predictiveItems = {"NORMAL MODE", "PREDICTIVE MODE"};

  // Initialize Default Disciplines (Distance by default or empty?)
  // Let's default to Distance mode
  loadDisciplines(1); // 1 = Distance

  _currentSpeed = 0.0;
  _slope = 0.0;
  _highlightTitle = "400 m";
  _highlightValue = "--.--";

  TFT_eSPI *tft = _ui->getTft();
  // Clear only content area
  tft->fillScreen(COLOR_BG);
  _ui->setTitle("DRAG METER");
  drawDashboardStatic(true);

  // Reset Run State
  _runState = RUN_WAITING;
  _oneFootReached = false;
  _startPosition = 0;

  // Load Settings
  Preferences p;
  p.begin("laptimer", true);
  _rolloutEnabled = p.getBool("rollout", false); // Default false
  int treeIdx = p.getInt("drag_tree_sec", 1);    // Match SettingsScreen key
  if (treeIdx == 0)
    _treeInterval = 3000;
  else if (treeIdx == 2)
    _treeInterval = 7000;
  else
    _treeInterval = 5000; // Default 5s

  int targetIdx = p.getInt("drag_target", 2); // Default index 2 (10.0s)
  float targets[] = {8.0, 9.0, 10.0, 10.5, 11.0, 11.5, 12.0, 13.0, 14.0, 15.0};
  if (targetIdx >= 0 && targetIdx < 10)
    _targetTime = targets[targetIdx];
  else
    _targetTime = 10.0;

  _referenceTime = p.getFloat("drag_ref", 0.0);
  p.end();

  _lastTreeCount = -1;
  _lastTreeIsGo = false;
  _wasOverlayActive = false;
  _lastHighlightBgColor = 0xFFFF; // Force first draw
}

void DragMeterScreen::update() {
  static unsigned long lastDragTouch = 0;
  UIManager::TouchPoint p = _ui->getTouchPoint();

  // 1. Touch Handling (debounced)
  if (p.x != -1 && (millis() - lastDragTouch > 200)) {
    lastDragTouch = millis();

    // 1.1 Global Back Button Check
    if (p.x < 60 && p.y > 275) {
      if (_state == STATE_MENU) {
        _ui->switchScreen(SCREEN_MENU);
      } else {
        _state = STATE_MENU;
        _selectedDragModeIdx = -1;
        _selectedPredictiveIdx = -1;
        _ui->setTitle("DRAG METER");
        _ui->getTft()->fillScreen(COLOR_BG);
        drawDashboardStatic(true);
      }
      return;
    }

    // 1.2 State-specific Touch Logic
    if (_state == STATE_MENU) {
      int startY = 60, btnH = 45, btnW = 360, gap = 8;
      int x = (SCREEN_WIDTH - btnW) / 2;
      if (p.x > x && p.x < x + btnW) {
        int touchedIdx = -1;
        for (int i = 0; i < (int)_menuItems.size(); i++) {
          int btnY = startY + (i * (btnH + gap));
          if (p.y > btnY && p.y < btnY + btnH) {
            touchedIdx = i;
            break;
          }
        }
        if (touchedIdx != -1) {
          unsigned long now = millis();
          if (_lastTapIdx == touchedIdx && (now - _lastTapTime < 500)) {
            _lastTapIdx = -1;
            handleMenuTouch(touchedIdx);
          } else {
            _lastTapIdx = touchedIdx;
            _lastTapTime = now;
            if (_selectedMenuIdx != touchedIdx) {
              _selectedMenuIdx = touchedIdx;
              drawDashboardStatic(false);
            }
          }
        }
      }
    } else if (_state == STATE_DRAG_MODE_MENU) {
      int startY = 60, btnH = 50, btnW = 360, gap = 12;
      int x = (SCREEN_WIDTH - btnW) / 2;
      if (p.x > x && p.x < x + btnW) {
        int touchedIdx = -1;
        for (int i = 0; i < (int)_dragModeItems.size(); i++) {
          int btnY = startY + (i * (btnH + gap));
          if (p.y > btnY && p.y < btnY + btnH) {
            touchedIdx = i;
            break;
          }
        }
        if (touchedIdx != -1) {
          unsigned long now = millis();
          if (_lastTapIdx == touchedIdx && (now - _lastTapTime < 500)) {
            _lastTapIdx = -1;
            handleDragModeTouch(touchedIdx);
          } else {
            _lastTapIdx = touchedIdx;
            _lastTapTime = now;
            if (_selectedDragModeIdx != touchedIdx) {
              _selectedDragModeIdx = touchedIdx;
              drawDashboardStatic(false);
            }
          }
        }
      }
    } else if (_state == STATE_PREDICTIVE_MENU) {
      int startY = 60, btnH = 50, btnW = 360, gap = 12;
      int x = (SCREEN_WIDTH - btnW) / 2;
      for (int i = 0; i < (int)_predictiveItems.size(); i++) {
        int y = startY + (i * (btnH + gap));
        if (p.y >= y && p.y < y + btnH && p.x >= x && p.x < x + btnW) {
          unsigned long now = millis();
          if (_lastTapIdx == i && (now - _lastTapTime < 500)) {
            _lastTapIdx = -1;
            handlePredictiveTouch(i);
          } else {
            _lastTapIdx = i;
            _lastTapTime = now;
            if (_selectedPredictiveIdx != i) {
              _selectedPredictiveIdx = i;
              drawDashboardStatic(false);
            }
          }
          break;
        }
      }
    } else if (_state == STATE_SUMMARY_VIEW) {
      if (p.y < 50 && p.x > 100) {
        _summaryShowBest = !_summaryShowBest;
        _ui->getTft()->fillScreen(COLOR_BG);
        drawDashboardStatic(true);
      }
    }
  }

  // 2. Continuous Logic (Independent of Touch)
  if (_state == STATE_RUNNING) {
    if (_runState == RUN_COUNTDOWN) {
      // Countdown timer
      unsigned long elapsed = millis() - _startTime;
      if (elapsed >= _treeInterval) {
        _runState = RUN_RUNNING;
        _runStartTime = millis();
        sessionManager.startSession();
      }
    } else if (_runState == RUN_RUNNING) {
      checkStopCondition();
      updateDisciplines();
    }

    // Overlay rendering during countdown and GO phase
    bool inGoPhase =
        (_runState == RUN_RUNNING && (millis() - _runStartTime < 1000));
    bool showOverlay = (_runState == RUN_COUNTDOWN || inGoPhase);

    if (showOverlay) {
      drawChristmasTreeOverlay();
    } else {
      drawDashboardDynamic();
    }
    _wasOverlayActive = showOverlay;
  }
}

void DragMeterScreen::checkStartCondition() {
  float speed = gpsManager.getSpeedKmph();
  if (speed > 5.0) { // Moving (> 5 km/h)
    unsigned long now = millis();

    if (_runState == RUN_WAITING) {
      // First motion detection
      _startLat = gpsManager.getLatitude();
      _startLon = gpsManager.getLongitude();
      _startAlt = gpsManager.getAltitude();
      _startPosition = 0;
      _totalRunDistance = 0;
    }

    if (_rolloutEnabled) {
      // For rollout, we track distance from initial movement
      double dist = gpsManager.distanceBetween(_startLat, _startLon,
                                               gpsManager.getLatitude(),
                                               gpsManager.getLongitude());
      _startPosition = dist; // approximate rollout distance
      _lastUpdate = now;

      if (_startPosition >= 0.3048) { // 1 ft
        _runState = RUN_RUNNING;
        _runStartTime = now;
        // Start "official" run from here
        _startLat = gpsManager.getLatitude();
        _startLon = gpsManager.getLongitude();
        _startAlt = gpsManager.getAltitude();

        // Reset disciplines
        for (auto &d : _disciplines) {
          d.completed = false;
          d.resultTime = 0;
        }
      }

    } else {
      // Immediate Start
      _runState = RUN_RUNNING;
      _runStartTime = millis();
      // Reset disciplines
      for (auto &d : _disciplines) {
        d.completed = false;
        d.resultTime = 0;
      }
    }

    if (_runState == RUN_RUNNING) {
      sessionManager.startSession();
    }
  } else {
    _lastUpdate = millis(); // Keep updating time while stationary
    _startPosition = 0;     // Reset rollout if stopped
  }
}

// ... functions ...

void DragMeterScreen::drawPredictiveMode() {
  TFT_eSPI *tft = _ui->getTft();

  // 1. Update Speed (Top Area) - Same as Normal but for Predictive
  tft->setTextColor(TFT_WHITE, 0x18E3);
  tft->setTextDatum(TC_DATUM);
  tft->setTextFont(7);
  tft->setTextSize(1);
  tft->setTextPadding(240);
  tft->drawString(String(_currentSpeed, 1), SCREEN_WIDTH / 2, 45);
  tft->setTextPadding(0);

  // 2. Find Longest Distance Discipline for Label
  String discName = "--- m";
  if (!_disciplines.empty()) {
    // Usually the last one is the longest distance
    discName = _disciplines.back().name;
  }

  // 3. Central Predictive Box & Footer (SKIP IF COUNTDOWN OR GO!)
  bool inGoPhase =
      (_runState == RUN_RUNNING && (millis() - _runStartTime < 1000));
  if (_runState == RUN_COUNTDOWN || inGoPhase) {
    // Don't draw predictive elements during countdown/GO to avoid overlap
    _lastHighlightBgColor = 0xFFFF; // Reset to force redraw when countdown ends
  } else {
    int boxY = 125;
    int boxW = SCREEN_WIDTH - 20;
    int boxH = SCREEN_HEIGHT - boxY - 45;
    int boxX = 10;

    calculatePrediction();

    // Determine color based on target
    uint16_t boxColor = TFT_GREEN;
    if (_targetTime > 0 && _predictedFinalTime > 0) {
      float delta = _predictedFinalTime - _targetTime;
      if (abs(delta) > 0.1) {
        boxColor = (delta > 0.1) ? TFT_RED : TFT_GREEN;
      }
    }

    // Draw Box ONLY if color changed OR forced
    if (boxColor != _lastHighlightBgColor) {
      _lastHighlightBgColor = boxColor;
      tft->fillRoundRect(boxX, boxY, boxW, boxH, 8, boxColor);
      tft->drawRoundRect(boxX, boxY, boxW, boxH, 8, TFT_WHITE);

      // Label inside box (static part)
      tft->setTextColor(TFT_BLACK, boxColor);
      tft->setTextDatum(TL_DATUM);
      tft->setFreeFont(&Org_01);
      tft->setTextSize(1);
      tft->drawString(discName + " predictive", boxX + 10, boxY + 10);
    }

    // Big Predicted Time (Always Update with Padding)
    tft->setTextColor(TFT_BLACK, _lastHighlightBgColor);
    tft->setTextDatum(MC_DATUM);
    tft->setTextFont(7);
    tft->setTextSize(2);
    String timeStr =
        (_predictedFinalTime > 0) ? String(_predictedFinalTime, 2) : "--.--";
    tft->setTextPadding(boxW - 20);
    tft->drawString(timeStr, SCREEN_WIDTH / 2, boxY + boxH / 2 + 10);
    tft->setTextPadding(0);

    // 4. Footer Values
    tft->setTextColor(TFT_WHITE, _ui->getBackgroundColor());
    tft->setTextDatum(BL_DATUM);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(1);
    String targetStr =
        "Target: " + (_targetTime > 0 ? String(_targetTime, 2) : "--.--");
    tft->setTextPadding(150);
    tft->drawString(targetStr, 10, SCREEN_HEIGHT - 5);

    // Secondary Info (e.g. 200m time)
    tft->setTextDatum(BR_DATUM);
    String secondaryStr = "-- m: --.--";
    if (_disciplines.size() >= 2) {
      for (const auto &d : _disciplines) {
        if (d.name.indexOf("200") != -1 && d.completed) {
          secondaryStr = d.name + ": " + String(d.resultTime / 1000.0, 2);
          break;
        }
      }
    }
    tft->setTextPadding(150);
    tft->drawString(secondaryStr, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 5);
    tft->setTextPadding(0);
  }

  // Font Safety
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
}

void DragMeterScreen::calculatePrediction() {
  // Basic prediction: Use reference time
  _predictedFinalTime = _referenceTime;
}

void DragMeterScreen::saveReferenceRun() {
  if (_disciplines.empty())
    return;

  // Calculate final time of the longest discipline?
  // We iterate to find the longest target discipline.
  // Simplifying: The last one is usually the longest or the "Result".
  Discipline *best = nullptr;
  for (auto &d : _disciplines) {
    if (d.completed)
      best = &d;
  }

  if (best && best->completed) {
    float runTime = best->resultTime / 1000.0;

    // Save if faster (runTime < _referenceTime) or if no reference exists
    // Note: Faster means LOWER time.
    if (_referenceTime <= 0.0 || runTime < _referenceTime) {
      _referenceTime = runTime;

      Preferences p;
      p.begin("laptimer", false);
      p.putFloat("drag_ref", _referenceTime);
      p.end();
    }
  }
}

void DragMeterScreen::checkStopCondition() {
  // If speed drops to 0 and we have been running for a bit?
  // Or simply if speed < 1.0
  if (gpsManager.getSpeedKmph() < 5.0) {
    // Only stop if we actually started (which we did if we are here)

    saveReferenceRun(); // Save if good run

    // Save to History
    // Use the first discipline as the primary result
    unsigned long resultTime = 0;
    String runName = "Drag Run";
    if (!_disciplines.empty()) {
      runName = _disciplines[0].name;
      if (_disciplines[0].completed) {
        resultTime = _disciplines[0].resultTime;
      }
    }

    sessionManager.stopSession();
    // Use the actual filename if we were logging
    String actualFilename = sessionManager.getCurrentFilename();
    if (actualFilename.length() == 0)
      actualFilename = "DragRun"; // Fallback

    String dateStr =
        gpsManager.getDateString() + " " + gpsManager.getTimeString();
    // Use '1' for run count, and resultTime for bestLap (repurposed field)
    sessionManager.appendToHistoryIndex(actualFilename, dateStr, 1, resultTime,
                                        "DRAG");

    _runState = RUN_FINISHED;

    // Go to Summary
    _state = STATE_SUMMARY_VIEW;
    _ui->getTft()->fillScreen(_ui->getBackgroundColor());
    drawSummary();
  }
}

void DragMeterScreen::updateDisciplines() {
  float speed = gpsManager.getSpeedKmph();
  unsigned long now = millis();
  unsigned long runTime = now - _runStartTime;

  // Geometric Distance
  double currentLat = gpsManager.getLatitude();
  double currentLon = gpsManager.getLongitude();
  double currentAlt = gpsManager.getAltitude();

  // Calculate total run distance from start point
  _totalRunDistance =
      gpsManager.distanceBetween(_startLat, _startLon, currentLat, currentLon);

  _currentSpeed = speed;

  // Calculate Slope: (Rise / Run) * 100
  // Only valid if we have moved enough to reduce GPS altitude noise impact
  if (_totalRunDistance > 50.0) {
    float rise = currentAlt - _startAlt;
    _slope = (rise / _totalRunDistance) * 100.0;
  } else {
    _slope = 0.0;
  }

  // LOG DATA
  if (_runState == RUN_RUNNING && sessionManager.isLogging()) {
    // Time,Lat,Lon,Speed,Sats,Alt,Heading
    String data = String(millis()) + "," + String(currentLat, 6) + "," +
                  String(currentLon, 6) + "," + String(speed, 2) + "," +
                  String(gpsManager.getSatellites()) + "," +
                  String(currentAlt, 2) + "," +
                  String(gpsManager.getHeading(), 2);
    sessionManager.logData(data);
  }

  // Check disciplines
  bool allComplete = true;
  for (auto &d : _disciplines) {
    if (!d.completed) {
      allComplete = false;

      d.slope = _slope; // Capture current slope

      // Peak Speed
      if (speed > d.peakSpeed)
        d.peakSpeed = speed;

      if (d.isDistance) {
        if (_totalRunDistance >= d.target) {
          d.completed = true;
          d.resultTime = runTime;
          d.endSpeed = speed;
          // Validate slope: < -1.0% invalid (NHRA rule approx)
          d.valid = (d.slope >= -1.0);
        }
      } else {
        if (speed >= d.target) {
          d.completed = true;
          d.resultTime = runTime;
          d.valid = (d.slope >= -1.0);
        }
      }
    }
  }

  // Run Finishing Logic
  if (allComplete) {
    if (!_brakingMeasurable) {
      _brakingMeasurable = true; // Mark as finished phase
    }

    // Auto-Stop Condition: Speed drops low OR timeout
    if (speed < 5.0) {
      _oneFootReached = false; // reset flag reuse
      // Call stop/save
      checkStopCondition(); // This function will see speed < 5.0 logic if
                            // updated
    }
  }

  // Update highlight
  if (!_disciplines.empty()) {
    _highlightTitle = _disciplines[0].name;
    if (_disciplines[0].completed) {
      _highlightValue = String(_disciplines[0].resultTime / 1000.0, 2) + "s";
    } else {
      _highlightValue = String(runTime / 1000.0, 1) + "s";
    }
  }
}

void DragMeterScreen::startChristmasTree() {
  _runState = RUN_COUNTDOWN;
  _startTime = millis();
  // _treeInterval is already set in onShow()
}

void DragMeterScreen::drawChristmasTreeOverlay() {
  TFT_eSPI *tft = _ui->getTft();

  bool isGo = false;
  int currentCount = 0;

  if (_runState == RUN_RUNNING) {
    isGo = true; // First second of run is the GO phase
  } else {
    unsigned long elapsed = millis() - _startTime;
    // Calculate digits 3, 2, 1 clearly
    currentCount = ((_treeInterval - elapsed - 1) / 1000) + 1;
    if (currentCount <= 0)
      currentCount = 1; // Safety
    isGo = false;
  }

  // Box Coordinates
  int boxX, boxY, boxW, boxH;

  if (_displayMode == DISPLAY_PREDICTIVE) {
    // Use Large Central Box coordinates in Predictive Mode
    boxY = 125;
    boxW = SCREEN_WIDTH - 20;
    boxH = SCREEN_HEIGHT - boxY - 45;
    boxX = 10;
  } else {
    // Matching drawDashboardStatic Highlight card in Normal Mode
    int splitX = SCREEN_WIDTH / 2;
    int bottomY = 130;
    int bottomH = SCREEN_HEIGHT - bottomY - 50;
    boxX = splitX + 5;
    boxY = bottomY;
    boxW = splitX - 15;
    boxH = bottomH;
  }

  // Background color: Green for GO, Blue for countdown
  uint16_t bgColor = isGo ? TFT_GREEN : 0x18E3; // Blue

  if (_lastTreeIsGo != isGo || _lastTreeCount == -1 ||
      currentCount != _lastTreeCount) {
    _lastTreeIsGo = isGo;
    // Force full redraw of box background
    tft->fillRoundRect(boxX, boxY, boxW, boxH, 8, bgColor);
    tft->drawRoundRect(boxX, boxY, boxW, boxH, 8, TFT_WHITE);

    if (isGo) {
      tft->setTextDatum(MC_DATUM);
      tft->setTextColor(TFT_WHITE, bgColor);
      tft->setTextFont(7);
      tft->setTextSize(1);
      tft->drawString("GO!", boxX + boxW / 2, boxY + boxH / 2);
    }
  }

  if (_lastTreeCount != currentCount || _lastTreeIsGo != isGo) {
    _lastTreeCount = currentCount;

    if (!isGo) {
      tft->setTextDatum(MC_DATUM);
      tft->setTextColor(TFT_WHITE, bgColor);
      tft->setTextFont(7);
      tft->setTextSize(1);
      tft->setTextPadding(boxW - 20); // Use padding to clear previous digit
      tft->drawString(String(currentCount), boxX + boxW / 2, boxY + boxH / 2);

      // Small "READY" text below number (Redraw only if not GO)
      tft->setTextPadding(0);
      tft->setTextFont(2);
      tft->drawString("READY", boxX + boxW / 2, boxY + boxH - 20);
    }
  }

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
}

void DragMeterScreen::handleMenuTouch(int idx) {
  if (idx < 0 || idx >= _menuItems.size())
    return;

  String &item = _menuItems[idx];
  if (item == "DRAG SCREEN") {
    _state = STATE_RUNNING;
    _displayMode = DISPLAY_NORMAL; // Reset to normal layout when entering
                                   // from main menu
    _ui->setTitle("DRAG METER");
    _ui->getTft()->fillScreen(_ui->getBackgroundColor());
    drawDashboardStatic(true);
    startChristmasTree();
  } else if (item == "DRAG MODE") {
    _state = STATE_DRAG_MODE_MENU;
    _ui->setTitle("DRAG MODE");
    _selectedDragModeIdx = -1;
    _ui->getTft()->fillScreen(COLOR_BG);
    drawDashboardStatic(true);
  } else if (item == "PREDICTIVE") {
    _state = STATE_PREDICTIVE_MENU;
    _ui->setTitle("PREDICTIVE");
    _ui->getTft()->fillScreen(COLOR_BG);
    drawDashboardStatic(true);
  } else if (item == "SUMMARY") {
    _state = STATE_SUMMARY_VIEW;
    _ui->setTitle("RUN SUMMARY");
    _ui->getTft()->fillScreen(COLOR_BG); // Clear for summary
    drawDashboardStatic(true);
  }
}

void DragMeterScreen::handleDragModeTouch(int idx) {
  if (idx >= 0) {
    _selectedDragModeIdx = idx;
    loadDisciplines(idx);
    _displayMode =
        DISPLAY_NORMAL; // Always reset to normal layout when changing mode

    // Go to Running View
    _state = STATE_RUNNING;
    _ui->setTitle("DRAG METER");
    _ui->getTft()->fillScreen(_ui->getBackgroundColor());
    drawDashboardStatic(true);
    startChristmasTree();
  }
}

void DragMeterScreen::loadDisciplines(int modeIdx) {
  _disciplines.clear();

  if (modeIdx == 0) {
    // SPEED MODE
    // 0-60 kph, 0-100 kph, 100-200 kph, 0-200 kph
    // Fields: name, isDist, start, target, resTime, compl, endSpd, slope,
    // peakSpd, brakeDist, valid
    _disciplines.push_back({"0-60", false, 0, 60, 0, false, 0, 0, 0, 0, true});
    _disciplines.push_back(
        {"0-100", false, 0, 100, 0, false, 0, 0, 0, 0, true});
    _disciplines.push_back(
        {"100-200", false, 100, 200, 0, false, 0, 0, 0, 0, true});
    _disciplines.push_back(
        {"0-200", false, 0, 200, 0, false, 0, 0, 0, 0, true});

    _highlightTitle = "0-100";
    _highlightValue = "--.--";

  } else if (modeIdx == 1) {
    // DISTANCE MODE
    // 60 ft, 100 m, 200 m, 400 m
    _disciplines.push_back(
        {"60 ft", true, 0, 18.288, 0, false, 0, 0, 0, 0, true});
    _disciplines.push_back({"100 m", true, 0, 100, 0, false, 0, 0, 0, 0, true});
    _disciplines.push_back({"200 m", true, 0, 200, 0, false, 0, 0, 0, 0, true});
    _disciplines.push_back({"400 m", true, 0, 400, 0, false, 0, 0, 0, 0, true});

    _highlightTitle = "400 m";
    _highlightValue = "--.--";

  } else {
    // CUSTOM (Placeholder)
    _disciplines.push_back(
        {"Custom 1", true, 0, 100, 0, false, 0, 0, 0, 0, true});
    _highlightTitle = "Custom";
    _highlightValue = "--.--";
  }

  // Reset session best if structure changed?
  // Ideally we match by name. For simplicitly, clear session best on mode
  // change.
  _sessionBest.clear();
}

void DragMeterScreen::drawDashboardStatic(bool forceStatusBar) {
  TFT_eSPI *tft = _ui->getTft();

  // 1. Solid Background (Removed fillScreen to match LapTimer - handled by
  // switch)

  if (_state == STATE_MENU) {
    _ui->setTitle("DRAG METER");
    drawMenu();
  } else if (_state == STATE_DRAG_MODE_MENU) {
    _ui->setTitle("DRAG MODE");
    drawDragModeMenu();
  } else if (_state == STATE_PREDICTIVE_MENU) {
    _ui->setTitle("PREDICTIVE");
    drawPredictiveMenu();
  } else if (_state == STATE_SUMMARY_VIEW) {
    _ui->setTitle("RUN SUMMARY");
    drawSummary();
  } else {
    _ui->setTitle(_displayMode == DISPLAY_PREDICTIVE ? "PREDICTIVE"
                                                     : "DRAG METER");

    if (_displayMode == DISPLAY_PREDICTIVE) {
      // --- PREDICTIVE LAYOUT ---
      // 1. Top Speed Area
      int speedCardY = 30;
      int speedCardH = 90;
      tft->fillRoundRect(10, speedCardY, SCREEN_WIDTH - 20, speedCardH, 8,
                         0x18E3);
      tft->drawRoundRect(10, speedCardY, SCREEN_WIDTH - 20, speedCardH, 8,
                         TFT_DARKGREY);
      tft->setTextColor(TFT_SILVER, 0x18E3);
      tft->setTextDatum(TR_DATUM);
      tft->setFreeFont(&Org_01);
      tft->setTextSize(1);
      tft->drawString("KPH", SCREEN_WIDTH - 25, speedCardY + 10);

      // 2. Large Central Box Area (Structure only)
      int boxY = 125;
      int boxH = SCREEN_HEIGHT - boxY - 45;
      // Background will be handled by dynamic draw (Green/Red)

      // 3. Footer Area
      tft->setFreeFont(&Org_01);
      tft->setTextSize(1);
      tft->setTextColor(TFT_WHITE, _ui->getBackgroundColor());
      tft->setTextDatum(BL_DATUM);
      tft->drawString("Target: --.--", 10, SCREEN_HEIGHT - 5);
      tft->setTextDatum(BR_DATUM);
      tft->drawString("-- m: --.--", SCREEN_WIDTH - 10, SCREEN_HEIGHT - 5);

      _lastHighlightBgColor = 0xFFFF; // Force predictive box redraw
    } else {
      // --- NORMAL LAYOUT ---
      // --- TOP CARD (SPEED AREA) ---
      int speedCardY = 30;
      int speedCardH = 90;
      tft->fillRoundRect(10, speedCardY, SCREEN_WIDTH - 20, speedCardH, 8,
                         0x18E3);
      tft->drawRoundRect(10, speedCardY, SCREEN_WIDTH - 20, speedCardH, 8,
                         TFT_DARKGREY);

      // "KPH" Label inside Top Card
      tft->setTextColor(TFT_SILVER, 0x18E3);
      tft->setTextDatum(TR_DATUM);
      tft->setFreeFont(&Org_01);
      tft->setTextSize(1);
      tft->drawString("KPH", SCREEN_WIDTH - 25, speedCardY + 10);

      // --- BOTTOM AREA (Two Cards) ---
      int bottomY = 130;
      int bottomH =
          SCREEN_HEIGHT - bottomY - 50; // Increased margin to 50 (Ends at 270)
      int splitX = SCREEN_WIDTH / 2;

      _lastHighlightBgColor = (_runState == RUN_RUNNING) ? TFT_GREEN : 0x18E3;

      // List Card (Left) - Always Blue
      tft->fillRoundRect(10, bottomY, splitX - 15, bottomH, 8, 0x18E3);
      tft->drawRoundRect(10, bottomY, splitX - 15, bottomH, 8, TFT_WHITE);

      // Highlight/Predictive Card (Right) - Dynamic
      tft->fillRoundRect(splitX + 5, bottomY, splitX - 15, bottomH, 8,
                         _lastHighlightBgColor);
      tft->drawRoundRect(splitX + 5, bottomY, splitX - 15, bottomH, 8,
                         TFT_WHITE);

      // List Headers (Left Side) - Always Silver/White
      tft->setTextDatum(TL_DATUM);
      tft->setTextSize(1);
      tft->setTextColor(TFT_SILVER, 0x18E3);
      int listStartY = bottomY + 10;
      int gap = 30;
      for (int i = 0; i < _disciplines.size(); i++) {
        tft->drawString(_disciplines[i].name, 20, listStartY + (i * gap));
      }

      // Footer Area (Slope)
      tft->setTextColor(TFT_WHITE, _ui->getBackgroundColor());
      tft->setTextDatum(TL_DATUM);
      tft->setFreeFont(&Org_01);
      tft->setTextSize(1);
      tft->drawString("SL:", 40,
                      SCREEN_HEIGHT - 25); // Shifted for back button clearance
    }
    // Force Christmas Tree overlay to redraw because background was
    // overwritten
    _lastTreeCount = -1;
    _lastTreeIsGo = false;
  }

  // Back Button (Blue Triangle) - Draw LAST
  tft->fillTriangle(15, SCREEN_HEIGHT - 30, 30, SCREEN_HEIGHT - 40, 30,
                    SCREEN_HEIGHT - 20, TFT_BLUE);

  _ui->drawStatusBar(forceStatusBar);
}

void DragMeterScreen::drawDashboardDynamic() {
  if (_state != STATE_RUNNING)
    return;

  TFT_eSPI *tft = _ui->getTft();

  if (_displayMode == DISPLAY_PREDICTIVE) {
    drawPredictiveMode();
    return;
  }

  // NORMAL MODE DRAWING
  // 1. Update Speed (Top Card)
  tft->setTextColor(TFT_WHITE, 0x18E3);
  tft->setTextDatum(TC_DATUM);
  tft->setTextFont(7);
  tft->setTextSize(1);
  tft->setTextPadding(240); // Standardize area for speed digits
  tft->drawString(String(_currentSpeed, 1), SCREEN_WIDTH / 2,
                  45); // Centered (offset removed)
  tft->setTextPadding(0);

  // 2. Update List Values (Bottom Left Card) - Always White on Blue
  tft->setTextColor(TFT_WHITE, 0x18E3);
  tft->setTextDatum(TR_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  int bottomY = 130;
  int listStartY = bottomY + 10;
  int gap = 30;
  for (int i = 0; i < _disciplines.size(); i++) {
    String valText = "--.--";
    if (_disciplines[i].completed) {
      valText = String(_disciplines[i].resultTime / 1000.0, 2);
    }
    // Align with names in static draw
    tft->setTextPadding(100);
    tft->drawString(valText, (SCREEN_WIDTH / 2) - 25, listStartY + (i * gap));
    tft->setTextPadding(0);
  }

  // 3. Highlight Card (Bottom Right Card) - SKIP IF COUNTDOWN OR GO!
  bool inGoPhase =
      (_runState == RUN_RUNNING && (millis() - _runStartTime < 1000));
  if (_runState == RUN_COUNTDOWN || inGoPhase) {
    // Skip to keep Christmas Tree / GO! visible
  } else {
    // Determine background and text color
    uint16_t bgColor = (_runState == RUN_RUNNING) ? TFT_GREEN : 0x18E3;
    uint16_t txtColor = (_runState == RUN_RUNNING) ? TFT_BLACK : TFT_CYAN;

    // Redraw box background ONLY if color changed (e.g. from Blue to Green
    // at start)
    if (bgColor != _lastHighlightBgColor) {
      drawDashboardStatic(false);
    }

    tft->setTextColor(txtColor, bgColor);

    // Title (e.g., "400 m")
    tft->setTextDatum(TC_DATUM);
    tft->setTextSize(2);
    tft->drawString(_highlightTitle, (SCREEN_WIDTH * 3) / 4 - 5, bottomY + 15);

    // Value (e.g., "11.37")
    tft->setTextFont(6);
    tft->setTextSize(1);
    tft->setTextPadding(140);
    tft->drawString(_highlightValue, (SCREEN_WIDTH * 3) / 4 - 5, bottomY + 50);
    tft->setTextPadding(0);
  }

  // 4. Slope (Footer)
  tft->setTextColor(TFT_WHITE, _ui->getBackgroundColor());
  tft->setFreeFont(&Org_01);
  tft->setTextSize(1);
  tft->setTextDatum(TL_DATUM);
  tft->setTextPadding(80);
  tft->drawString(String(_slope, 1) + "%", 70, SCREEN_HEIGHT - 25);
  tft->setTextPadding(0);

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
}

void DragMeterScreen::drawSummary() {
  TFT_eSPI *tft = _ui->getTft();

  // Removed fillScreen - handled by transition

  // Header Title
  tft->setTextColor(TFT_WHITE, _ui->getBackgroundColor());
  tft->setTextDatum(TC_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  String header = _summaryShowBest ? "SESSION BEST" : "LAST RUN";
  tft->drawString(header, SCREEN_WIDTH / 2, 28);

  // Toggle Arrows (Subtle)
  tft->setTextSize(1);
  tft->setTextColor(TFT_SILVER, _ui->getBackgroundColor());
  if (_summaryShowBest) {
    tft->drawString("v", SCREEN_WIDTH / 2 + 85, 28);
  } else {
    tft->drawString("^", SCREEN_WIDTH / 2 + 85, 28);
  }

  // --- TOP STAT CARD ---
  int topStatY = 55;
  int topStatH = 40;
  tft->fillRoundRect(10, topStatY, SCREEN_WIDTH - 20, topStatH, 6, 0x18E3);
  tft->drawRoundRect(10, topStatY, SCREEN_WIDTH - 20, topStatH, 6,
                     TFT_DARKGREY);

  float peak = 0;
  const std::vector<Discipline> *data =
      _summaryShowBest ? &_sessionBest : &_disciplines;
  if (!data->empty()) {
    peak = data->back().peakSpeed;
  }

  tft->setTextColor(TFT_CYAN, 0x18E3);
  tft->setTextDatum(ML_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(1);
  tft->drawString("PEAK SPEED:", 20, topStatY + topStatH / 2);

  tft->setTextDatum(MR_DATUM);
  tft->setTextFont(4);
  tft->drawString(String(peak, 1) + " KPH", SCREEN_WIDTH - 20,
                  topStatY + topStatH / 2);

  // --- TABLE AREA ---
  int tableY = 105;
  tft->setFreeFont(&Org_01);
  tft->setTextSize(1);
  tft->setTextColor(TFT_SILVER, _ui->getBackgroundColor());
  tft->setTextDatum(TL_DATUM);
  tft->drawString("DISC.", 20, tableY);
  tft->setTextDatum(TC_DATUM);
  tft->drawString("TIME", 210, tableY); // Adjusted column
  tft->setTextDatum(TR_DATUM);
  tft->drawString("@KPH", 350, tableY); // Adjusted column
  tft->drawString("SL%", 460, tableY);  // Adjusted column

  tft->drawFastHLine(0, tableY + 15, SCREEN_WIDTH, 0x18E3);

  int startY = tableY + 22;
  int gap = 30;

  if (data->empty()) {
    tft->setTextColor(TFT_DARKGREY, _ui->getBackgroundColor());
    tft->setTextDatum(TC_DATUM);
    tft->drawString("NO DATA AVAILABLE", SCREEN_WIDTH / 2, 180);
  } else {
    for (int i = 0; i < data->size(); i++) {
      const Discipline &d = (*data)[i];
      int rowY = startY + (i * gap);
      uint16_t color = d.valid ? TFT_GREEN : TFT_RED;

      tft->setTextColor(color, _ui->getBackgroundColor());
      tft->setTextSize(2);

      tft->setTextDatum(TL_DATUM);
      tft->drawString(d.name, 20, rowY);

      tft->setTextDatum(TC_DATUM);
      tft->drawString(d.completed ? String(d.resultTime / 1000.0, 2) : "-", 210,
                      rowY);

      tft->setTextDatum(TR_DATUM);
      tft->drawString(d.completed ? String(d.endSpeed, 1) : "-", 350, rowY);
      tft->drawString(String(d.slope, 1), 460, rowY);
    }
  }

  // Footer: Braking
  float brakeDist = data->empty() ? 0 : data->back().brakingDistance;
  tft->setTextColor(TFT_SILVER, _ui->getBackgroundColor());
  tft->setTextSize(1);
  tft->setTextDatum(TL_DATUM);
  tft->drawString("BRAKING DISTANCE:", 40, SCREEN_HEIGHT - 25);
  tft->setTextDatum(TR_DATUM);
  tft->setTextColor(TFT_WHITE, _ui->getBackgroundColor());
  tft->drawString(String(brakeDist, 1) + " m", SCREEN_WIDTH - 10,
                  SCREEN_HEIGHT - 25);

  _ui->drawStatusBar(true);

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
}

void DragMeterScreen::drawMenu() {
  TFT_eSPI *tft = _ui->getTft();

  // Removed fillScreen - handled by transition

  int startY = 60;
  int btnHeight = 45; // Reduced from 50
  int btnWidth = 360;
  int gap = 8; // Reduced from 12
  int x = (SCREEN_WIDTH - btnWidth) / 2;

  for (int i = 0; i < _menuItems.size(); i++) {
    int y = startY + (i * (btnHeight + gap));

    uint16_t btnColor = (i == _selectedMenuIdx) ? COLOR_PRIMARY : 0x18E3;
    uint16_t borderColor = (i == _selectedMenuIdx) ? TFT_CYAN : TFT_DARKGREY;

    tft->fillRoundRect(x, y, btnWidth, btnHeight, 6, btnColor);
    tft->drawRoundRect(x, y, btnWidth, btnHeight, 6, borderColor);

    tft->setTextColor(TFT_WHITE, btnColor);
    tft->setTextDatum(MC_DATUM);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(2);
    tft->drawString(_menuItems[i], SCREEN_WIDTH / 2, y + btnHeight / 2 + 2);
  }
  _ui->drawStatusBar(true);

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextFont(1);
  tft->setTextPadding(0);
}

void DragMeterScreen::drawDragModeMenu() {
  TFT_eSPI *tft = _ui->getTft();

  // Removed fillScreen - handled by transition

  int startY = 60;
  int btnHeight = 50;
  int btnWidth = 360;
  int gap = 12;
  int x = (SCREEN_WIDTH - btnWidth) / 2;

  for (int i = 0; i < _dragModeItems.size(); i++) {
    int y = startY + (i * (btnHeight + gap));

    uint16_t btnColor = (i == _selectedDragModeIdx) ? COLOR_PRIMARY : 0x18E3;
    uint16_t borderColor =
        (i == _selectedDragModeIdx) ? TFT_CYAN : TFT_DARKGREY;

    tft->fillRoundRect(x, y, btnWidth, btnHeight, 6, btnColor);
    tft->drawRoundRect(x, y, btnWidth, btnHeight, 6, borderColor);

    tft->setTextColor(TFT_WHITE, btnColor);
    tft->setTextDatum(MC_DATUM);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(2);
    tft->drawString(_dragModeItems[i], SCREEN_WIDTH / 2, y + btnHeight / 2 + 2);
  }
  _ui->drawStatusBar(true);
}

void DragMeterScreen::drawPredictiveMenu() {
  TFT_eSPI *tft = _ui->getTft();

  // Removed fillScreen - handled by transition

  int startY = 60;
  int btnHeight = 50;
  int btnWidth = 360;
  int gap = 12;
  int x = (SCREEN_WIDTH - btnWidth) / 2;

  for (int i = 0; i < _predictiveItems.size(); i++) {
    int y = startY + (i * (btnHeight + gap));

    uint16_t btnColor = (i == _selectedPredictiveIdx) ? COLOR_PRIMARY : 0x18E3;
    uint16_t borderColor =
        (i == _selectedPredictiveIdx) ? TFT_CYAN : TFT_DARKGREY;

    tft->fillRoundRect(x, y, btnWidth, btnHeight, 6, btnColor);
    tft->drawRoundRect(x, y, btnWidth, btnHeight, 6, borderColor);

    tft->setTextColor(TFT_WHITE, btnColor);
    tft->setTextDatum(MC_DATUM);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(2);
    tft->drawString(_predictiveItems[i], SCREEN_WIDTH / 2,
                    y + btnHeight / 2 + 2);
  }
  _ui->drawStatusBar(true);
}

void DragMeterScreen::handlePredictiveTouch(int idx) {
  if (idx == 1) { // PREDICTIVE MODE
    _displayMode = DISPLAY_PREDICTIVE;
    _state = STATE_RUNNING;
    _lastHighlightBgColor = 0xFFFF; // Reset to force redraw
    _ui->getTft()->fillScreen(COLOR_BG);
    drawDashboardStatic(true);
  } else {
    // Normal Mode
    _displayMode = DISPLAY_NORMAL;
    _state = STATE_RUNNING;
    _lastHighlightBgColor = 0xFFFF; // Reset to force redraw
    _ui->getTft()->fillScreen(COLOR_BG);
    drawDashboardStatic(true);
  }
}
