#include "DragMeterScreen.h"
#include "../../core/GPSManager.h"
#include "../../core/SessionManager.h"
#include "../fonts/Org_01.h"
#include <Preferences.h>

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
  _selectedBtn = -1;
  _selectedMenuIdx = -1;
  _selectedDragModeIdx = -1;
  _selectedPredictiveIdx = -1;
  _lastTapIdx = -1;
  _lastTapTime = 0;
  _menuItems = {"DRAG MODE", "DRAG SCREEN", "PREDICTIVE", "SUMMARY"};
  _dragModeItems = {"SPEED", "DISTANCE", "CUSTOM"};
  _predictiveItems = {"NORMAL MODE", "PREDICTIVE MODE"};

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
  _ui->drawCarbonBackground(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                            SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
  _ui->setTitle("DRAG METER");
  drawDashboardStatic();

  // Reset Run State
  _runState = RUN_WAITING;
  _oneFootReached = false;
  _startPosition = 0;

  // Load Settings
  Preferences p;
  p.begin("laptimer", true);
  _rolloutEnabled = p.getBool("rollout", false); // Default false
  int treeIdx = p.getInt("tree_time", 0);
  int targetIdx = p.getInt("drag_target", 2); // Default index 2 (10.0s)
  // Options: 8.0, 9.0, 10.0, 10.5, 11.0, 11.5, 12.0, 13.0, 14.0, 15.0
  float targets[] = {8.0, 9.0, 10.0, 10.5, 11.0, 11.5, 12.0, 13.0, 14.0, 15.0};
  if (targetIdx >= 0 && targetIdx < 10)
    _targetTime = targets[targetIdx];
  else
    _targetTime = 10.0;

  _referenceTime = p.getFloat("drag_ref", 0.0);
  p.end();

  if (treeIdx <= 5)
    _treeInterval = (treeIdx + 1) * 500;
  else if (treeIdx == 6)
    _treeInterval = 4000;
  else
    _treeInterval = 5000;

  _displayMode = DISPLAY_NORMAL;
  _predictedFinalTime = 0;

  _summaryShowBest = false;
  _sessionBest.clear();
}

void DragMeterScreen::update() {
  static unsigned long lastDragTouch = 0;
  UIManager::TouchPoint p = _ui->getTouchPoint();
  if (p.x != -1) {
    if (millis() - lastDragTouch > 200) {
      lastDragTouch = millis();

      // 1. Tombol Kembali (Top Left)
      if (p.x < 60 && p.y < 60) {
        static unsigned long lastBackTap = 0;
        if (millis() - lastBackTap < 500) {
          lastBackTap = 0;
          if (_state == STATE_MENU) {
            _ui->switchScreen(SCREEN_MENU);
          } else if (_state == STATE_DRAG_MODE_MENU) {
            _state = STATE_MENU;
            _ui->setTitle("DRAG METER");
            _selectedDragModeIdx = -1; // Reset selection
            _ui->getTft()->fillScreen(
                _ui->getBackgroundColor()); // Clear entire screen
            drawMenu();
          } else if (_state == STATE_PREDICTIVE_MENU) {
            _state = STATE_MENU;
            _ui->setTitle("DRAG METER");
            _selectedPredictiveIdx = -1;
            _ui->getTft()->fillScreen(_ui->getBackgroundColor());
            drawMenu();
          } else if (_state == STATE_SUMMARY_VIEW) {
            _state = STATE_MENU;
            _ui->setTitle("DRAG METER");
            _ui->getTft()->fillScreen(_ui->getBackgroundColor());
            drawMenu();
          } else {
            // If in running mode, go back to menu
            _state = STATE_MENU;
            _ui->setTitle("DRAG METER");
            _ui->getTft()->fillScreen(_ui->getBackgroundColor());
            drawDashboardStatic();
          }
        } else {
          lastBackTap = millis();
        }
        return;
      }

      // 2. Menu Logic
      if (_state == STATE_MENU) {
        int startY = 60;
        int btnHeight = 50;
        int btnWidth = 360;
        int gap = 8;
        int x = (SCREEN_WIDTH - btnWidth) / 2;

        // Check if X is within button width (centered)
        if (p.x > x && p.x < x + btnWidth) {
          int touchedIdx = -1;
          // Check Y coordinates
          for (int i = 0; i < _menuItems.size(); i++) {
            int btnY = startY + (i * (btnHeight + gap));
            if (p.y > btnY && p.y < btnY + btnHeight) {
              touchedIdx = i;
              break;
            }
          }

          if (touchedIdx != -1) {
            unsigned long now = millis();
            if (_lastTapIdx == touchedIdx && (now - _lastTapTime < 500)) {
              // Second tap: Execute
              _lastTapIdx = -1;
              handleMenuTouch(touchedIdx);
            } else {
              // First tap: Select/Highlight
              _lastTapIdx = touchedIdx;
              _lastTapTime = now;

              if (_selectedMenuIdx != touchedIdx) {
                _selectedMenuIdx = touchedIdx;
                drawMenu();
              }
            }
          }
        }
      } else if (_state == STATE_DRAG_MODE_MENU) {
        // Drag Mode Menu Logic (Similar to Main Menu)
        int startY = 55;
        int btnHeight = 35;
        int btnWidth = 240;
        int gap = 8;
        int x = (SCREEN_WIDTH - btnWidth) / 2;

        if (p.x > x && p.x < x + btnWidth) {
          int touchedIdx = -1;
          for (int i = 0; i < _dragModeItems.size(); i++) {
            int btnY = startY + (i * (btnHeight + gap));
            if (p.y > btnY && p.y < btnY + btnHeight) {
              touchedIdx = i;
              break;
            }
          }

          if (touchedIdx != -1) {
            unsigned long now = millis();
            if (_lastTapIdx == touchedIdx && (now - _lastTapTime < 500)) {
              // Confirmed
              _lastTapIdx = -1;
              handleDragModeTouch(touchedIdx);
            } else {
              // First tap
              _lastTapIdx = touchedIdx;
              _lastTapTime = now;

              if (_selectedDragModeIdx != touchedIdx) {
                _selectedDragModeIdx = touchedIdx;
                drawDragModeMenu();
              }
            }
          }
        }
      }
    }
  }

  // Handle Summary View Touch
  if (_state == STATE_SUMMARY_VIEW && p.x != -1) {
    if (millis() - lastDragTouch > 200) {
      lastDragTouch = millis();

      // Toggle Area (Header center/right)
      if (p.y < 50 && p.x > 100) {
        _summaryShowBest = !_summaryShowBest;
        _ui->getTft()->fillScreen(_ui->getBackgroundColor());
        drawSummary();
      }
    }
  }

  if (_state == STATE_PREDICTIVE_MENU) {
    int touchedIdx = -1;
    int listY = 60;
    int itemH = 50;
    int gap = 10;

    for (int i = 0; i < _predictiveItems.size(); i++) {
      int y = listY + i * (itemH + gap);
      if (p.y >= y && p.y < y + itemH && p.x >= 10 && p.x < SCREEN_WIDTH - 10) {
        touchedIdx = i;
        break;
      }
    }

    if (touchedIdx != -1) {
      unsigned long now = millis();
      if (_lastTapIdx == touchedIdx && (now - _lastTapTime < 500)) {
        // Confirmed
        _lastTapIdx = -1;
        handlePredictiveTouch(touchedIdx);
      } else {
        // First tap
        _lastTapIdx = touchedIdx;
        _lastTapTime = now;

        if (_selectedPredictiveIdx != touchedIdx) {
          _selectedPredictiveIdx = touchedIdx;
          drawPredictiveMenu();
        }
      }
    }
  }

  if (_state == STATE_RUNNING) {
    if (_runState == RUN_WAITING) {
      checkStartCondition();

      // Mode Toggle Logic
      // Up Arrow (Predicted -> Normal) - Y approx 150
      if (_displayMode == DISPLAY_PREDICTIVE) {
        if (p.x > SCREEN_WIDTH / 2 - 40 && p.x < SCREEN_WIDTH / 2 + 40 &&
            p.y > 110 && p.y < 190) {
          if (millis() - lastDragTouch > 300) {
            lastDragTouch = millis();
            _displayMode = DISPLAY_NORMAL;
            _ui->getTft()->fillScreen(_ui->getBackgroundColor());
            drawDashboardStatic();
          }
        }
      }
      // Down Arrow (Normal -> Predictive) - Y approx 260
      else if (_displayMode == DISPLAY_NORMAL) {
        if (p.x > SCREEN_WIDTH / 2 - 40 && p.x < SCREEN_WIDTH / 2 + 40 &&
            p.y > 220 && p.y < 300) {
          if (millis() - lastDragTouch > 300) {
            lastDragTouch = millis();
            _displayMode = DISPLAY_PREDICTIVE; // Or cycle through modes
            _ui->getTft()->fillScreen(_ui->getBackgroundColor());
            drawDashboardStatic();
          }
        }
      }

      // Check for Tree Button (Bottom Right)
      if (p.x != -1 && p.x > SCREEN_WIDTH - 60 && p.y > SCREEN_HEIGHT - 60) {
        if (millis() - lastDragTouch > 500) { // Debounce
          lastDragTouch = millis();
          startChristmasTree();
        }
      }
    } else if (_runState == RUN_COUNTDOWN) {
      // Christmas Tree Logic
      unsigned long elapsed = millis() - _startTime;
      if (elapsed >= _treeInterval) {
        // GO!
        _runState = RUN_RUNNING;
        _runStartTime = millis();                             // Start timer
        _ui->getTft()->fillScreen(_ui->getBackgroundColor()); // Clear tree
        drawDashboardStatic();
      } else {
        drawChristmasTreeOverlay();
      }
    } else if (_runState == RUN_RUNNING) {
      checkStopCondition();
      updateDisciplines();
    }
    drawDashboardDynamic();
  }
}

void DragMeterScreen::checkStartCondition() {
  float speed = gpsManager.getSpeedKmph();
  if (speed > 1.0) { // Moving (> 1 km/h)
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

  // Draw Big Predicted Time
  tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
  tft->setTextDatum(MC_DATUM);
  tft->setTextFont(7); // Big 7-Segment Font
  tft->setTextSize(2); // Massive

  calculatePrediction();

  String timeStr;
  if (_predictedFinalTime > 0) {
    timeStr = String(_predictedFinalTime, 2);
  } else {
    timeStr = "--.--";
  }
  // Centered vertically approx
  tft->drawString(timeStr, SCREEN_WIDTH / 2, 130);

  // Delta to Target
  tft->setTextFont(4); // Medium Font
  tft->setTextSize(1);
  if (_targetTime > 0 && _predictedFinalTime > 0) {
    float delta = _predictedFinalTime - _targetTime;
    uint16_t color = _ui->getTextColor();
    if (abs(delta) <= 0.1)
      color = TFT_GREEN;
    else
      color = TFT_RED;

    tft->setTextColor(color, _ui->getBackgroundColor());
    String deltaStr = "Target: " + String(_targetTime, 1) + "s";
    tft->drawString(deltaStr, SCREEN_WIDTH / 2, 230);
  }
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
  _ui->getTft()->fillScreen(_ui->getBackgroundColor());
  // Draw Tree Background
}

void DragMeterScreen::drawChristmasTreeOverlay() {
  // Simple Lights Logic
  TFT_eSPI *tft = _ui->getTft();
  unsigned long elapsed = millis() - _startTime;
  int phase = elapsed / (_treeInterval / 4); // 4 phases: Yellow 1, 2, 3, Green

  int cx = SCREEN_WIDTH / 2;
  int cy = SCREEN_HEIGHT / 2;

  // Draw Traffic Light
  // Pre-Stage (White)
  tft->fillCircle(cx, cy - 60, 20, _ui->getTextColor());

  // Stage (White)
  tft->fillCircle(cx, cy - 20, 20, _ui->getTextColor());

  // Yellows
  if (phase >= 1)
    tft->fillCircle(cx, cy + 20, 20, TFT_YELLOW);
  if (phase >= 2)
    tft->fillCircle(cx, cy + 60, 20, TFT_YELLOW);
  if (phase >= 3)
    tft->fillCircle(cx, cy + 100, 20, TFT_YELLOW);

  // Green (Handled by state switch, but last frame)
}

void DragMeterScreen::handleMenuTouch(int idx) {
  if (idx < 0 || idx >= _menuItems.size())
    return;

  String &item = _menuItems[idx];
  if (item == "DRAG SCREEN") {
    _state = STATE_RUNNING;
    _ui->setTitle("DRAG METER");
    _ui->getTft()->fillScreen(_ui->getBackgroundColor());
    drawDashboardStatic();
  } else if (item == "DRAG MODE") {
    _state = STATE_DRAG_MODE_MENU;
    _ui->setTitle("DRAG MODE");
    _selectedDragModeIdx = -1;
    _ui->getTft()->fillScreen(_ui->getBackgroundColor());
    drawDragModeMenu();
  } else if (item == "PREDICTIVE") {
    _state = STATE_PREDICTIVE_MENU;
    _ui->setTitle("PREDICTIVE");
    _ui->getTft()->fillScreen(_ui->getBackgroundColor());
    drawPredictiveMenu();
  } else if (item == "SUMMARY") {
    _state = STATE_SUMMARY_VIEW;
    _ui->setTitle("RUN SUMMARY");
    _ui->getTft()->fillScreen(_ui->getBackgroundColor()); // Clear for summary
    drawSummary();
  }
}

void DragMeterScreen::handleDragModeTouch(int idx) {
  if (idx >= 0) {
    _selectedDragModeIdx = idx;
    loadDisciplines(idx);

    // Go to Running View
    _state = STATE_RUNNING;
    _ui->setTitle("DRAG METER");
    _ui->getTft()->fillScreen(_ui->getBackgroundColor());
    drawDashboardStatic();
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

void DragMeterScreen::drawDashboardStatic() {
  TFT_eSPI *tft = _ui->getTft();

  // Common Header (Back Arrow)
  tft->setTextColor(COLOR_HIGHLIGHT, _ui->getBackgroundColor());
  tft->setTextDatum(TL_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->drawString("<", 10, 25);

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
    _ui->setTitle("DRAG METER");
    // Draw Running View (Back arrow already drawn)
    tft->drawFastHLine(0, 45, SCREEN_WIDTH, _ui->getTextColor());

    // Speed Area
    // "KPH" Label
    tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
    tft->setTextDatum(TR_DATUM); // Top Right
    tft->setFreeFont(&Org_01);
    tft->setTextSize(2);
    tft->drawString("KPH", SCREEN_WIDTH - 10, 70);

    // Horizontal Line below Speed
    tft->drawFastHLine(0, 130, SCREEN_WIDTH, _ui->getTextColor());

    // Draw Mode Toggle Arrows
    tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
    tft->setTextDatum(TC_DATUM);
    tft->setTextSize(1);
    if (_displayMode == DISPLAY_NORMAL) {
      tft->drawString("v", SCREEN_WIDTH / 2, 260);
    } else {
      tft->drawString("^", SCREEN_WIDTH / 2, 150);
    }

    // Vertical Line splitting List and Highlight
    tft->drawFastVLine(SCREEN_WIDTH / 2, 130, SCREEN_HEIGHT - 130 - 30,
                       _ui->getTextColor()); // -30 for footer

    // List Area (Left)
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(2);
    int listStartY = 140;
    int gap = 35;
    for (int i = 0; i < _disciplines.size(); i++) {
      tft->drawString(_disciplines[i].name, 10, listStartY + (i * gap));
    }

    // Highlight Box (Right) - Background White
    tft->fillRect(SCREEN_WIDTH / 2 + 1, 131, SCREEN_WIDTH / 2 - 1,
                  SCREEN_HEIGHT - 130 - 31, _ui->getTextColor());

    // Footer Area (Slope)
    tft->fillRect(0, SCREEN_HEIGHT - 30, SCREEN_WIDTH, 30,
                  _ui->getBackgroundColor()); // Clear footer
    tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1); // Smaller font for footer
    tft->drawString("SL:", 10, SCREEN_HEIGHT - 25);
  }
  _ui->drawStatusBar(true);
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
  // 1. Update Speed
  // Big Font
  tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
  tft->setTextDatum(TC_DATUM);
  tft->setTextFont(7); // Big 7-segment like font
  tft->setTextSize(1); // Standard size for font 7
  // We need to verify if Font 7 is available or usage. Org_01 is tiny.
  // Use FreeFont if needed or standard font numbers.
  // Let's try font 6 or 7 if available, otherwise large scaled FreeFont.
  // Assuming Font 6 (large numeric) is available in TFT_eSPI default.
  tft->drawString(String(_currentSpeed, 1), SCREEN_WIDTH / 2 - 20, 50);

  // 2. Update List Values (Left)
  tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
  tft->setTextDatum(TR_DATUM); // Align Right for values
  tft->setFreeFont(&Org_01);   // Back to Org_01
  tft->setTextSize(2);
  int listStartY = 140;
  int gap = 35;
  for (int i = 0; i < _disciplines.size(); i++) {
    String valText = "--.--";
    if (_disciplines[i].completed) {
      valText = String(_disciplines[i].resultTime / 1000.0, 2);
    }
    // Draw value at fixed X (half width - padding)
    tft->drawString(valText, (SCREEN_WIDTH / 2) - 10, listStartY + (i * gap));
  }

  // 3. Highlight Box (Right) - White BG, Black Text
  tft->setTextColor(_ui->getBackgroundColor(), _ui->getTextColor());

  // Title (e.g., "400 m")
  tft->setTextDatum(TC_DATUM);
  tft->setTextSize(2); // Smaller for title
  tft->drawString(_highlightTitle, (SCREEN_WIDTH * 3) / 4, 145);

  // Value (e.g., "11.37")
  tft->setTextFont(6); // Large numeric
  tft->setTextSize(1);
  tft->drawString(_highlightValue, (SCREEN_WIDTH * 3) / 4, 190);

  // 4. Slope (Footer)
  tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
  tft->setFreeFont(&Org_01); // Back to Org_01
  tft->setTextSize(1);
  tft->setTextDatum(TL_DATUM);
  tft->drawString(String(_slope, 1) + "%", 40, SCREEN_HEIGHT - 25);
}

void DragMeterScreen::drawSummary() {
  TFT_eSPI *tft = _ui->getTft();

  // Header
  tft->setTextColor(_ui->getTextColor(), _ui->getBackgroundColor());
  tft->setTextDatum(TC_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  String header = _summaryShowBest ? "SESSION BEST" : "LAST RUN";
  tft->drawString(header, SCREEN_WIDTH / 2, 25);

  // Toggle Arrows
  tft->setTextSize(1);
  if (_summaryShowBest) {
    tft->drawString("v", SCREEN_WIDTH / 2 + 80, 25); // Down to go to Last
  } else {
    tft->drawString("^", SCREEN_WIDTH / 2 + 80, 25); // Up to go to Best
  }

  // Back Button
  tft->setTextColor(COLOR_HIGHLIGHT, COLOR_BG);
  tft->setTextDatum(TL_DATUM);
  tft->setTextSize(2); // Org_01 size 2
  tft->drawString("<", 10, 25);

  tft->drawFastHLine(0, 45, SCREEN_WIDTH, TFT_WHITE);

  // Peak Speed (Top Right)
  // Find peak speed of longest discipline or just max speed seen?
  // We captured peakSpeed in each discipline. Let's show the peak of the
  // last/longest one.
  float peak = 0;
  const std::vector<Discipline> *data =
      _summaryShowBest ? &_sessionBest : &_disciplines;

  if (!data->empty()) {
    peak = data->back().peakSpeed;
  }

  tft->setTextColor(TFT_WHITE, COLOR_BG);
  tft->setTextDatum(TR_DATUM);
  tft->setTextFont(4); // Medium numeric
  tft->setTextSize(1);
  tft->drawString(String(peak, 1), SCREEN_WIDTH - 40, 50);

  tft->setFreeFont(&Org_01);
  tft->setTextSize(1);
  tft->drawString("PK KPH", SCREEN_WIDTH - 5, 65);

  tft->drawFastHLine(0, 85, SCREEN_WIDTH, TFT_WHITE);

  // Table Headers
  int y = 90;
  tft->setTextColor(TFT_LIGHTGREY, COLOR_BG); // Grey headers
  tft->setTextDatum(TL_DATUM);
  tft->drawString("DISC.", 10, y);
  tft->setTextDatum(TC_DATUM);
  tft->drawString("TIME", 180, y);
  tft->setTextDatum(TR_DATUM);
  tft->drawString("@KPH", 340, y);
  tft->drawString("SL%", 450, y);

  tft->drawFastHLine(0, 105, SCREEN_WIDTH, TFT_LIGHTGREY);

  // Rows
  int startY = 110;
  int gap = 35; // compact

  if (data->empty()) {
    tft->setTextColor(TFT_WHITE, COLOR_BG);
    tft->setTextDatum(TC_DATUM);
    tft->drawString("NO DATA", SCREEN_WIDTH / 2, 150);
    return;
  }

  for (int i = 0; i < data->size(); i++) {
    const Discipline &d = (*data)[i];
    int rowY = startY + (i * gap);

    // Color: Red if invalid, Green/White otherwise
    // Use flag d.valid
    uint16_t color = d.valid ? TFT_GREEN : TFT_RED;
    if (!_summaryShowBest && !d.valid) {
      // In last run, if invalid show red.
    }
    // Actually, let's stick to Green for valid, Red for invalid
    tft->setTextColor(color, COLOR_BG);
    tft->setTextSize(2);

    // Name
    tft->setTextDatum(TL_DATUM);
    tft->drawString(d.name, 10, rowY);

    // Time
    tft->setTextDatum(TC_DATUM);
    if (d.completed) {
      tft->drawString(String(d.resultTime / 1000.0, 2), 180, rowY);
    } else {
      tft->drawString("-", 180, rowY);
    }

    // End Speed
    tft->setTextDatum(TR_DATUM);
    if (d.completed) {
      tft->drawString(String(d.endSpeed, 1), 340, rowY);
    } else {
      tft->drawString("-", 340, rowY);
    }

    // Slope
    tft->drawString(String(d.slope, 1), 450, rowY);

    // Invalid Icon? If red, maybe draw a small X?
    // Rely on color for now.
  }

  // Footer: Braking Distance
  tft->drawFastHLine(0, SCREEN_HEIGHT - 25, SCREEN_WIDTH, TFT_WHITE);
  tft->setTextColor(TFT_WHITE, COLOR_BG);
  tft->setTextDatum(TL_DATUM);
  tft->drawString("Brk Dist:", 10, SCREEN_HEIGHT - 20);

  float brakeDist = 0;
  if (!data->empty())
    brakeDist = data->back().brakingDistance;

  tft->setTextDatum(TR_DATUM);
  tft->drawString(String(brakeDist, 1) + " m", SCREEN_WIDTH - 10,
                  SCREEN_HEIGHT - 20);

  _ui->drawStatusBar(true);
}

void DragMeterScreen::drawMenu() {
  TFT_eSPI *tft = _ui->getTft();

  // Back Button
  tft->setTextColor(COLOR_HIGHLIGHT, COLOR_BG);
  tft->setTextDatum(TL_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->drawString("<", 10, 25);

  tft->drawFastHLine(0, 45, SCREEN_WIDTH, COLOR_SECONDARY);

  int startY = 60;
  int btnHeight = 50;
  int btnWidth = 360;
  int gap = 8;
  int x = (SCREEN_WIDTH - btnWidth) / 2;

  for (int i = 0; i < _menuItems.size(); i++) {
    int y = startY + (i * (btnHeight + gap));

    // Determine Color based on selection
    uint16_t btnColor = (i == _selectedMenuIdx) ? TFT_RED : TFT_DARKGREY;

    tft->fillRoundRect(x, y, btnWidth, btnHeight, 5, btnColor);
    tft->setTextColor(TFT_WHITE, btnColor);
    tft->setTextDatum(MC_DATUM);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(2); // Using Org_01 size 2 to match LapTimer
    tft->drawString(_menuItems[i], SCREEN_WIDTH / 2, y + btnHeight / 2 + 2);
  }
  _ui->drawStatusBar(true);
}

void DragMeterScreen::drawDragModeMenu() {
  TFT_eSPI *tft = _ui->getTft();

  // Back Button
  tft->setTextColor(COLOR_HIGHLIGHT, COLOR_BG);
  tft->setTextDatum(TL_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->drawString("<", 10, 25);

  tft->drawFastHLine(0, 45, SCREEN_WIDTH, COLOR_SECONDARY);

  int startY = 60;
  int btnHeight = 50;
  int btnWidth = 360;
  int gap = 8;
  int x = (SCREEN_WIDTH - btnWidth) / 2;

  for (int i = 0; i < _dragModeItems.size(); i++) {
    int y = startY + (i * (btnHeight + gap));

    // Determine Color based on selection
    uint16_t btnColor = (i == _selectedDragModeIdx) ? TFT_RED : TFT_DARKGREY;

    tft->fillRoundRect(x, y, btnWidth, btnHeight, 5, btnColor);
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

  // Back Button
  tft->setTextColor(COLOR_HIGHLIGHT, COLOR_BG);
  tft->setTextDatum(TL_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->drawString("<", 10, 25);

  tft->drawFastHLine(0, 45, SCREEN_WIDTH, COLOR_SECONDARY);

  int startY = 55;
  int btnHeight = 35;
  int btnWidth = 240;
  int gap = 8;
  int x = (SCREEN_WIDTH - btnWidth) / 2;

  for (int i = 0; i < _predictiveItems.size(); i++) {
    int y = startY + (i * (btnHeight + gap));

    // Determine Color based on selection
    uint16_t btnColor = (i == _selectedPredictiveIdx) ? TFT_RED : TFT_DARKGREY;

    tft->fillRoundRect(x, y, btnWidth, btnHeight, 5, btnColor);
    tft->setTextColor(TFT_WHITE, btnColor);
    tft->setTextDatum(MC_DATUM);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(2); // Using Org_01 size 2 to match LapTimer
    tft->drawString(_predictiveItems[i], SCREEN_WIDTH / 2,
                    y + btnHeight / 2 + 2);
  }
  _ui->drawStatusBar(true);
}

void DragMeterScreen::handlePredictiveTouch(int idx) {
  // If Preductive Mode selected (idx 1), maybe we go to Summary View?
  // User didn't specify what Normal Mode does.
  // For now, let's just go to Summary View regardless, or stay in menu with
  // selection indicator? Let's assume selecting it goes to the view for now, as
  // that's typical. Or maybe it toggles a mode and goes back? Given the
  // previous "Predictive" went to "Summary View", let's make "Predictive Mode"
  // go there. "Normal Mode" might just go back to Drag Mode or similar? Let's
  // make "Predictive Mode" go to SummaryView.

  if (idx == 1) { // PREDICTIVE MODE
    _state = STATE_SUMMARY_VIEW;
    _ui->setTitle("RUN SUMMARY");
    _ui->getTft()->fillScreen(COLOR_BG);
    drawSummary();
  } else {
    // Normal Mode - maybe back to drag screen?
    _state = STATE_RUNNING;
    _ui->getTft()->fillScreen(COLOR_BG);
    drawDashboardStatic();
  }
}
