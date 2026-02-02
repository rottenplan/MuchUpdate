#include "LapTimerScreen.h"
#include "../../core/GPSManager.h"
#include "../../core/SessionManager.h"
#include "../fonts/Org_01.h"
#include <Preferences.h>
#include <algorithm> // Untuk min_element

extern GPSManager gpsManager;
extern SessionManager sessionManager;

// Konstanta untuk Tata Letak UI
#define STATUS_BAR_HEIGHT 20
#define LIST_ITEM_HEIGHT 30

// Tentukan Area Tombol
#define STOP_BTN_Y 255 // Dipindahkan KE BAWAH untuk jarak yang lebih baik
#define STOP_BTN_H 55

void LapTimerScreen::onShow() {
  _lastUpdate = 0;
  _lastTouchTime = millis(); // Prevent ghost touch on entry
  _lastBackTapTime = 0;
  _isRecording = false; // Mulai tidak merekam
  _finishSet = false;
  _lapCount = 0;
  _state = STATE_TRACK_LIST; // Mulai di Pilihan Track
  _raceMode = MODE_BEST;     // Default mode
  _bestLapTime = 0;
  _bestLapTime = 0;
  _lapTimes.clear();
  _listScroll = 0;
  _menuSelectionIdx = -1;
  _maxRpmSession = 0; // Reset Max RPM

  // Reset Flicker Tracking
  _lastSpeed = -999.0;
  _lastSats = -1;
  _lastRpmRender = -1;
  _lastMaxRpmRender = 0;
  _lastLapCountRender = -1;
  _lastRecordedStateRender = (RecordingState)-1;
  _lastLastLapTimeRender = -1;
  _lastBestLapTimeRender = -1;
  _maxSpeedSession = 0.0;
  _maxSpeedSessionRender = -1.0;
  _maxRpmSessionRender = 0;
  _finishLineInside = false;
  _lastFinishCross = 0;

  // Initialize GPS recording state
  _recordingState = RECORD_IDLE;
  _recordedPoints.clear();
  _recordStartLat = 0;
  _recordStartLon = 0;
  _recordingStartTime = 0;
  _lastPointTime = 0;
  _totalDistance = 0;

  loadTracks();

  // Start in Sub-Menu
  _state = STATE_MENU;
  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(_ui->getBackgroundColor()); // Ensure clean background
  gpsManager.setRawDataCallback(nullptr);     // Ensure no log overlay
  _needsStaticRedraw = true;
  drawMenu();

  // Try to load reference lap from History (Naive scan for now)
  // Logic: Find file with best time matching current track?
  // Limitation: We don't know "current track" filename history.
  // Workaround: Use best lap ever.
  // Ignoring for now to keep startup fast. Future: User selects "Load
  // Reference".
}

#include <ArduinoJson.h>

#include <ArduinoJson.h>

void LapTimerScreen::loadTracks() {
  _tracks.clear();

  double curLat = gpsManager.getLatitude();
  double curLon = gpsManager.getLongitude();

  // Load from SD Card if available
  if (SD.exists("/tracks.json")) {
    File file = SD.open("/tracks.json", FILE_READ);
    if (file) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, file);
      file.close();

      if (!error && doc["tracks"].is<JsonArray>()) {
        JsonArray trackArray = doc["tracks"];
        for (JsonVariant t : trackArray) {
          double tLat = t["lat"].as<double>();
          double tLon = t["lon"].as<double>();

          // Filter 50km Radius
          double dist = gpsManager.distanceBetween(curLat, curLon, tLat, tLon);
          if (dist > 50000)
            continue;

          Track newTrack;
          newTrack.name = t["name"].as<String>();
          newTrack.lat = tLat;
          newTrack.lon = tLon;
          newTrack.isCustom = true; // Loaded from SD

          // Configs
          JsonArray configs = t["configs"];
          if (configs.size() > 0) {
            for (JsonVariant c : configs) {
              newTrack.configs.push_back({c.as<String>()});
            }
          } else {
            newTrack.configs.push_back({"Default"});
          }

          if (t.containsKey("path")) {
            newTrack.pathFile = t["path"].as<String>();
          }
          if (t.containsKey("best_lap")) {
            newTrack.bestLap = t["best_lap"].as<unsigned long>();
          }

          _tracks.push_back(newTrack);
        }
        Serial.println("Tracks loaded from SD");
      }
    }
  }

  // Factory Tracks (Hardcoded)
  // Check dist for them too
  Track sonoma;
  sonoma.name = "Test Track (Bordeaux)"; // Renamed to match img
  sonoma.lat = 44.8378;                  // Bordeaux approx
  sonoma.lon = -0.5792;
  sonoma.isCustom = false; // Factory
  sonoma.isCustom = false; // Factory
  sonoma.configs.push_back({"Default"});
  sonoma.pathFile = ""; // No file for factory (or hardcode points later)

  // Always add test track for DEBUG/UI TESTING
  _tracks.push_back(sonoma);

  if (gpsManager.isFixed()) {
    double d =
        gpsManager.distanceBetween(curLat, curLon, sonoma.lat, sonoma.lon);
    // if (d < 50000)
    //   _tracks.push_back(sonoma);
  } else {
    // For Debug/Sim without GPS, maybe add it?
    // User said "If GPS data is unavailable... cannot enter the menu".
    // So logic in `update` prevents us getting here without GPS.
    // So here safely assume GPS is fixed.
    // I'll leave the check enabled.
    double d =
        gpsManager.distanceBetween(curLat, curLon, sonoma.lat, sonoma.lon);
    // if (d < 50000)
    //   _tracks.push_back(sonoma);
  }

  // Add a fake "Nearby" one for testing if list is empty?
  // _tracks.push_back(sonoma); // FORCE ADD FOR UI TESTING (Remove later)
}

void LapTimerScreen::loadTrackPath(String filename) {
  _recordedPoints.clear();

  if (!SD.exists(filename)) {
    Serial.println("Track path file not found: " + filename);
    return;
  }

  File file = SD.open(filename, FILE_READ);
  if (!file)
    return;

  // Read CSV: lat,lon (one per line)
  // Limit points to save RAM (e.g. max 1000)
  int count = 0;
  while (file.available() && count < 1000) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      int commaIndex = line.indexOf(',');
      if (commaIndex > 0) {
        String latStr = line.substring(0, commaIndex);
        String lonStr = line.substring(commaIndex + 1);

        GPSPoint p;
        p.lat = latStr.toDouble();
        p.lon = lonStr.toDouble();
        p.timestamp = 0; // Static path
        _recordedPoints.push_back(p);
        count++;
      }
    }
  }
  file.close();
  Serial.println("Loaded " + String(count) + " points from " + filename);
}

void LapTimerScreen::saveTrackToGPX(String filename) {
  if (_recordedPoints.empty()) {
    Serial.println("No points to save!");
    return;
  }

  // Ensure directory exists
  if (!SD.exists("/tracks")) {
    SD.mkdir("/tracks");
  }

  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing: " + filename);
    return;
  }

  Serial.println("Saving GPX to: " + filename);

  // 1. Header
  file.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  file.println("<gpx version=\"1.1\" creator=\"MuchRacing\" "
               "xmlns=\"http://www.topografix.com/GPX/1/1\">");

  // 2. Metadata / Track Info
  file.println("  <trk>");
  // Use current track name or generic
  String trackName =
      (_currentTrackName.length() > 0) ? _currentTrackName : " Recorded Track";
  file.println("    <name>" + trackName + "</name>");
  file.println("    <trkseg>");

  // 3. Points
  for (const auto &p : _recordedPoints) {
    file.printf("      <trkpt lat=\"%.7f\" lon=\"%.7f\">\n", p.lat, p.lon);

    // Optional: Add Elevation or Time if available in GPSPoint struct
    // Standard timestamp format: 2023-10-25T14:30:00Z
    // Currently we store raw millis() or similar in timestamp, need real time?
    // If GPSManager has real UTC time, ideally we'd use that.
    // For now, no time tag to avoid confusing parsers with bad data.

    file.println("      </trkpt>");
  }

  // 4. Footer
  file.println("    </trkseg>");
  file.println("  </trk>");
  file.println("</gpx>");

  file.close();
  Serial.println("GPX Saved Successfully.");
}

void LapTimerScreen::update() {
  UIManager::TouchPoint p = _ui->getTouchPoint();
  bool touched = (p.x != -1);

  if (_state == STATE_MENU) {
    if (touched) {
      // Global Debounce for Menu
      if (millis() - _lastTouchTime < 200)
        return;
      _lastTouchTime = millis();

      // 1. Back/Home (< 60x60)
      if (p.x < 60 && p.y < 60) {
        if (_menuSelectionIdx == -2) {
          if (millis() - _lastBackTapTime < 500) {
            _ui->switchScreen(SCREEN_MENU);
            _lastBackTapTime = 0;
          } else {
            _lastBackTapTime = millis();
          }
        } else {
          _menuSelectionIdx = -2;
          drawMenu();
          _lastBackTapTime = millis();
        }
        return;
      }

      // 2. Button Logic
      // startY = 60, btnHeight = 50, gap = 8
      int startY = 60;
      int btnHeight = 50;
      int gap = 8;
      int x = (SCREEN_WIDTH - 360) / 2;
      int btnWidth = 360;

      // Check if X is within button width (centered)
      if (p.x > x && p.x < x + btnWidth) {
        int touchedIdx = -1;

        // Check Y coordinates for each button
        for (int i = 0; i < 4; i++) {
          int btnY = startY + (i * (btnHeight + gap));
          if (p.y > btnY && p.y < btnY + btnHeight) {
            touchedIdx = i;
            break;
          }
        }

        if (touchedIdx != -1) {
          // Debounce handled above

          // Double Tap Logic
          if (_menuSelectionIdx == touchedIdx) {
            // Second tap on SAME button -> Execute Action

            // Execute Action
            if (touchedIdx == 0) {         // Select Track
              if (!gpsManager.isFixed()) { // GPS CHECK ENABLED
                _state = STATE_NO_GPS;
                _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                        SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                        _ui->getBackgroundColor());
                drawNoGPS();
              } else {
                // Go to Searching Screen first
                _state = STATE_SEARCHING;
                _searchStartTime = millis();
                _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                        SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                        _ui->getBackgroundColor());
                drawSearching();
              }
            } else if (touchedIdx == 1) { // Race Screen
              _state = STATE_RACING;
              // fillRect removed, handled by drawRacingStatic's fillScreen

              drawRacingStatic();
              drawRacing();
            } else if (touchedIdx == 2) { // Session Summary
              _state = STATE_SUMMARY;
              _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                      SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                      _ui->getBackgroundColor());
              drawSummary();
            } else if (touchedIdx == 3) { // Record Track
              _recordingState = RECORD_IDLE;
              _recordedPoints.clear();
              _state = STATE_RECORD_TRACK;
              _lastRecordedStateRender = (RecordingState)-1; // Force Redraw
              _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                      SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                      _ui->getBackgroundColor());
              drawRecordTrack();
            }
          } else {
            // First tap (or different button) -> Highlight Only
            _menuSelectionIdx = touchedIdx;
            drawMenu();
          }

          return;
        }
      }
    }
  } else if (_state == STATE_CREATE_TRACK) {
    extern GPSManager gpsManager;
    if (touched) {
      if (millis() - _lastTouchTime < 300)
        return;
      _lastTouchTime = millis();

      // Back Button (< 60x60)
      if (p.x < 60 && p.y < 60) {
        _state = STATE_TRACK_LIST;
        _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                _ui->getBackgroundColor());
        drawTrackList();
        return;
      }

      if (_createStep == 0) {
        // SET START Button (Centered, y=200, w=220, h=50)
        int btnW = 220;
        int btnH = 50;
        int btnX = (SCREEN_WIDTH - btnW) / 2;
        int btnY = 200;

        if (p.x > btnX && p.x < btnX + btnW && p.y > btnY &&
            p.y < btnY + btnH) {
          // Capture GPS
          if (gpsManager.isFixed()) { // GPS CHECK RESTORED
            _createStartLat = gpsManager.getLatitude();
            _createStartLon = gpsManager.getLongitude();
            _createStep = 1;
            _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                    SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                    _ui->getBackgroundColor()); // Clear
            drawCreateTrack();
          } else {
            _ui->showToast("No GPS Fix!", 2000);
          }
        }
      } else if (_createStep == 1) {
        int btnW = 200;
        int btnH = 50;
        int btnY = 200;

        // Button 1: SAME AS START (Left)
        int btn1X = 10;
        if (p.x > btn1X && p.x < btn1X + btnW && p.y > btnY &&
            p.y < btnY + btnH) {
          _createFinishLat = _createStartLat;
          _createFinishLon = _createStartLon;
          // SAVE
          _createStep = 2; // Show "Saving"
          drawCreateTrack();

          // Generate Name (e.g. "Track [Time]")
          String name = "Track " + String(millis() / 1000);
          saveNewTrack(name, _createStartLat, _createStartLon, _createFinishLat,
                       _createFinishLon);

          _ui->showToast("Track Saved!", 2000);
          loadTracks(); // Reload to see it

          // Exit
          _state = STATE_TRACK_LIST;
          _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                  _ui->getBackgroundColor());
          drawTrackList();
          return;
        }

        // Button 2: SET FINISH (Right)
        int btn2X = SCREEN_WIDTH - 10 - btnW;
        if (p.x > btn2X && p.x < btn2X + btnW && p.y > btnY &&
            p.y < btnY + btnH) {
          if (gpsManager.isFixed()) {
            _createFinishLat = gpsManager.getLatitude();
            _createFinishLon = gpsManager.getLongitude();
            // SAVE
            _createStep = 2;
            drawCreateTrack();

            String name = "Track " + String(millis() / 1000);
            saveNewTrack(name, _createStartLat, _createStartLon,
                         _createFinishLat, _createFinishLon);

            _ui->showToast("Track Saved!", 2000);
            loadTracks();

            _state = STATE_TRACK_LIST;
            _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                    SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                    _ui->getBackgroundColor());
            drawTrackList();
            return;
          } else {
            _ui->showToast("No GPS Fix!", 2000);
          }
        }
      }
    }
  } else if (_state == STATE_NO_GPS) {
    if (touched) {
      if (millis() - _lastTouchTime < 200)
        return;
      _lastTouchTime = millis();

      int btnY = SCREEN_HEIGHT - 60;
      // Retry (Left) x=20, w=130
      if (p.y > btnY && p.y < btnY + 40) {
        if (p.x > 20 && p.x < 150) {
          // Retry Logic DISABLED
          /*
          if (gpsManager.isFixed()) {
            _state = STATE_SEARCHING;
            _searchStartTime = millis();
            _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                    SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                    COLOR_BG);
            drawSearching();
          } else {
            // Feedback (Redraw to blink)
            drawNoGPS();
          }
          */
        }
        // Continue (Right) x=170, w=130 -> Back to Menu
        else if (p.x > 170 && p.x < 300) {
          _state = STATE_MENU;
          _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                  _ui->getBackgroundColor());
          drawMenu();
          _ui->drawStatusBar();
        }
      }
    }
  } else if (_state == STATE_SEARCHING) {
    // Auto-transition after delay
    if (millis() - _searchStartTime > 2000) {
      loadTracks();
      // If no tracks loaded, maybe stay in searching or show "No Tracks"?
      // For now, go to List, list handles empty state.
      _selectedTrackIdx = -1;
      _state = STATE_TRACK_LIST;
      _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                              SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                              _ui->getBackgroundColor());
      drawTrackList();
    }
  } else if (_state == STATE_TRACK_LIST) {
    if (touched) {
      if (millis() - _lastTouchTime < 200)
        return;
      _lastTouchTime = millis();

      // 1. Back Arrow (if clicked top left still works, though text changed)
      // Title is "Nearby Tracks" at x=10. Back behavior?
      // User didn't specify Back on List, but implied menu access.
      // Let's keep Back check on left just in case < 60x60
      if (p.x < 60 && p.y < 60) {
        if (millis() - _lastBackTapTime < 500) {
          _state = STATE_MENU;
          _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT, COLOR_BG);
          drawMenu();
          _ui->drawStatusBar();
          _lastBackTapTime = 0;
        } else {
          _lastBackTapTime = millis();
        }
        return;
      }

      // 2. New Track Button (Top Right)
      // btnX = SCREEN_WIDTH - 110, Y=22, W=100, H=20
      if (p.x > SCREEN_WIDTH - 110 && p.y < 50) {
        // Go to Create Track Wizard
        _state = STATE_CREATE_TRACK;
        _createStep = 0;
        _createStartLat = 0;
        _createStartLon = 0;
        _createFinishLat = 0;
        _createFinishLon = 0;

        _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                _ui->getBackgroundColor());
        drawCreateTrack();
        return;
      }

      // 3. Track Selection (Open Popup)
      int startY = 60;
      int itemH = 55;
      int gap = 8;
      if (p.y > startY) {
        int idx = (p.y - startY) / (itemH + gap);
        if (idx >= 0 && idx < _tracks.size()) {
          _selectedTrackIdx = idx;
          _state = STATE_TRACK_MENU;
          drawTrackOptionsPopup();
        }
      }
    }
  } else if (_state == STATE_TRACK_MENU) {
    if (touched) {
      if (millis() - _lastTouchTime < 200)
        return;
      _lastTouchTime = millis();

      // Popup Coords calculation again
      int w = 220;
      int h = 150;
      int x = (SCREEN_WIDTH - w) / 2;
      int y = (SCREEN_HEIGHT - h) / 2 + 10;

      // Check if touch inside popup
      if (p.x > x && p.x < x + w && p.y > y && p.y < y + h) {
        // Row Check
        int itemH = 25;
        int relY = p.y - (y + 10);
        int idx = relY / itemH;

        if (idx == 0) { // Select
          Track &t = _tracks[_selectedTrackIdx];
          _currentTrackName = t.name;
          _selectedConfigIdx = 0;

          // Load Track Path if available
          if (t.pathFile.length() > 0) {
            loadTrackPath(t.pathFile);
          } else {
            _recordedPoints.clear(); // Clear any previous points
          }

          _state = STATE_RACING;

          // LOAD REFERENCE LAP FOR PREDICTIVE TIMING
          if (t.bestLap > 0) {
            // Try to find the best lap file in history or current
            // For now, let's assume we load the most recent session of this
            // track or the one that provided the best lap. Logic simplified:
            // Use the track's pathFile if it's a log format
            sessionManager.loadBestLapAsReference(t.pathFile);
          }

          drawRacingStatic();
          drawRacing();
        } else if (idx == 1) { // Select & Edit
          // Go to Details Screen
          _state = STATE_TRACK_DETAILS;
          _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                  _ui->getBackgroundColor());
          drawTrackDetails();
        } else if (idx == 2) { // Invert
                               // TODO: Logic
        } else if (idx == 3) { // Reinit Best Lap
          if (_selectedTrackIdx >= 0 && _selectedTrackIdx < _tracks.size()) {
            _tracks[_selectedTrackIdx].bestLap = 0;
          }
          // Close Popup
          _state = STATE_TRACK_LIST;
          _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                  _ui->getBackgroundColor());
          drawTrackList();
        } else if (idx == 4) { // Remove
          if (_selectedTrackIdx >= 0 && _selectedTrackIdx < _tracks.size()) {
            if (_tracks[_selectedTrackIdx].isCustom) {
              _tracks.erase(_tracks.begin() + _selectedTrackIdx);
            }
          }
          _state = STATE_TRACK_LIST;
          _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT, COLOR_BG);
          drawTrackList();
        }
      } else {
        // Click Outside -> Close Popup
        _state = STATE_TRACK_LIST;
        _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                _ui->getBackgroundColor());
        drawTrackList();
      }
    }
  } else if (_state == STATE_TRACK_DETAILS) {
    if (touched) {
      if (millis() - _lastTouchTime < 200)
        return;
      _lastTouchTime = millis();

      // 1. Back Button (Bottom-Left)
      if (p.x < 100 && p.y > 240) {
        if (millis() - _lastBackTapTime < 500) {
          _state = STATE_TRACK_LIST;
          _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                  _ui->getBackgroundColor());
          drawTrackList();
          _lastBackTapTime = 0;
        } else {
          _lastBackTapTime = millis();
        }
        return;
      }

      // 2. Select Button
      int btnW = 180;
      int btnX = (SCREEN_WIDTH - btnW) / 2;
      int btnY = 240; // Match drawTrackDetails
      int btnH = 45;

      if (p.x > btnX && p.x < btnX + btnW && p.y > btnY - 10 &&
          p.y < btnY + btnH + 10) {
        Track &t = _tracks[_selectedTrackIdx];
        _currentTrackName = t.name;
        _state = STATE_RACING;
        if (t.pathFile.length() > 0)
          loadTrackPath(t.pathFile);
        if (t.bestLap > 0)
          sessionManager.loadBestLapAsReference(t.pathFile);
        drawRacingStatic();
        drawRacing();
      }

      // 3. Edit Name Button
      int renameX = 420;
      int renameY = 80 + 5;
      if (p.x > renameX && p.x < renameX + 45 && p.y > renameY &&
          p.y < renameY + 18) {
        _state = STATE_RENAME_TRACK;
        _renamingName = _tracks[_selectedTrackIdx].name;
        drawRenameTrack(true);
      }
    }
  } else if (_state == STATE_SAVE_TRACK) {
    if (touched) {
      if (millis() - _lastTouchTime < 150)
        return;
      _lastTouchTime = millis();
      KeyboardComponent::KeyResult key = _keyboard.handleTouch(p.x, p.y, 90);
      if (key.type != KeyboardComponent::KEY_NONE) {
        if (key.type == KeyboardComponent::KEY_CHAR) {
          if (_renamingName.length() < 15) {
            _renamingName += key.value;
            drawSaveTrackName(false);
          }
        } else if (key.type == KeyboardComponent::KEY_DEL) {
          if (_renamingName.length() > 0) {
            _renamingName.remove(_renamingName.length() - 1);
            drawSaveTrackName(false);
          }
        } else if (key.type == KeyboardComponent::KEY_OK) {
          String fn = _renamingName;
          fn.trim();
          if (fn.length() == 0)
            fn = "Track_" + String(millis());
          saveTrackToGPX("/tracks/" + fn + ".gpx");
          _ui->showToast("Saved!", 2000);
          _state = STATE_TRACK_LIST;
          loadTracks();
          drawTrackList();
        } else if (key.type == KeyboardComponent::KEY_SHIFT) {
          _keyboardShift = !_keyboardShift;
          drawSaveTrackName(true);
        }
      }
      // Cancel Button (Bottom Left)
      if (p.x < 100 && p.y > 240) {
        _state = STATE_RECORD_TRACK;
        _recordingState = RECORD_COMPLETE;
        _ui->getTft()->fillScreen(_ui->getBackgroundColor());
        drawRecordTrack();
      }
    }
  } else if (_state == STATE_RENAME_TRACK) {
    if (touched) {
      if (millis() - _lastTouchTime < 250)
        return;
      _lastTouchTime = millis();
      KeyboardComponent::KeyResult res = _keyboard.handleTouch(p.x, p.y, 110);
      if (res.type == KeyboardComponent::KEY_OK) {
        renameTrack(_selectedTrackIdx, _renamingName);
        _state = STATE_TRACK_DETAILS;
        _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                _ui->getBackgroundColor());
        drawTrackDetails();
      } else if (res.type != KeyboardComponent::KEY_NONE) {
        // Handle other keys...
        if (res.type == KeyboardComponent::KEY_CHAR) {
          _renamingName += res.value;
          drawRenameTrack();
        } else if (res.type == KeyboardComponent::KEY_DEL) {
          if (_renamingName.length() > 0)
            _renamingName.remove(_renamingName.length() - 1);
          drawRenameTrack();
        }
      }
    }
  } else if (_state == STATE_SUMMARY) {

    // --- LOGIKA STATUS RINGKASAN ---
    if (touched) {
      if (millis() - _lastTouchTime < 200)
        return;
      _lastTouchTime = millis();

      // 1. Tombol Kembali (Bottom Left area)
      if (p.x < 100 && p.y > 240) {
        if (millis() - _lastBackTapTime < 500) {
          _state = STATE_TRACK_LIST;
          _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                  _ui->getBackgroundColor());
          drawTrackList();
          _ui->drawStatusBar();
          _lastBackTapTime = 0;
        } else {
          _lastBackTapTime = millis();
        }
        return;
      }

      // 2. Restart / New Session (Tap anywhere else?)
      // For now, let's keep it simple: Tap bottom right to go to Track Select?
      if (p.x > 200 && p.y > 200) {
        _state = STATE_TRACK_LIST;
        _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                _ui->getBackgroundColor());
        drawTrackList();
        _ui->drawStatusBar();
        return;
      }
    }
  } else if (_state == STATE_RECORD_TRACK) {
    extern GPSManager gpsManager;

    if (touched) {
      // Back button (Bottom Left area)
      if (p.x < 100 && p.y > 240) {
        if (millis() - _lastBackTapTime < 500) {
          _state = STATE_MENU;
          _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT, COLOR_BG);
          drawMenu();
          _ui->drawStatusBar();
          _lastBackTapTime = 0;
        } else {
          _lastBackTapTime = millis();
        }
        return;
      }

      // Constants for Touch Logic (Must match drawRecordTrack)
      int cardY = 55;
      int cardH = 40;
      int clearY = cardY + cardH + 5;
      int gridY = clearY + 10;
      int boxH = 70;
      int btnY = 240;
      int btnH = 55;

      if (_recordingState == RECORD_IDLE) {
        // START button (Centered)
        // Hit Area: X 150-330
        if (p.x > 150 && p.x < 330 && p.y > btnY - 10 &&
            p.y < btnY + btnH + 10) {

          if (true) {
            // BYPASS GPS CHECK for Recording
            // Logic... same as before
            if (_menuSelectionIdx == 10) {
              _menuSelectionIdx = -1;
              _recordingState = RECORD_ACTIVE;
              _recordStartLat = gpsManager.getLatitude();
              _recordStartLon = gpsManager.getLongitude();
              _recordingStartTime = millis();
              _lastPointTime = millis();
              _recordedPoints.clear();

              GPSPoint firstPoint;
              firstPoint.lat = _recordStartLat;
              firstPoint.lon = _recordStartLon;
              firstPoint.timestamp = millis();
              _recordedPoints.push_back(firstPoint);

              drawRecordTrack();
            } else {
              _menuSelectionIdx = 10;
              drawRecordTrack();
            }
          }
        }
      } else if (_recordingState == RECORD_ACTIVE) {
        // STOP button (Centered)
        if (p.x > 150 && p.x < 330 && p.y > btnY - 10 &&
            p.y < btnY + btnH + 10) {

          if (_menuSelectionIdx == 11) {
            _menuSelectionIdx = -1;
            _recordingState = RECORD_COMPLETE;
            drawRecordTrack();
          } else {
            _menuSelectionIdx = 11;
            drawRecordTrack();
          }
        }
      } else if (_recordingState == RECORD_COMPLETE) {
        // SAVE | DISCARD Buttons
        // btnW=100, gap=20.
        // StartX = (480 - 220)/2 = 130.
        // SaveX: 130 to 230
        // DelX: 250 to 350
        // Y position = gridY + boxH + 20 = 110 + 70 + 20 = 200.

        int saveY = 200;

        // SAVE (Left)
        // SAVE (Left)
        if (p.x > 130 && p.x < 230 && p.y > saveY && p.y < saveY + btnH) {
          if (_menuSelectionIdx == 12) {
            // ACTION: SAVE -> GO TO NAMING
            _state = STATE_SAVE_TRACK;
            _renamingName = "";
            _keyboardShift = true;
            drawSaveTrackName(true);
            _menuSelectionIdx = -1;
          } else {
            _menuSelectionIdx = 12;
            drawRecordTrack();
          }
        }
        // DISCARD (Right)
        else if (p.x > 250 && p.x < 350 && p.y > saveY && p.y < saveY + btnH) {
          if (_menuSelectionIdx == 13) {
            // ACTION: DISCARD
            _recordingState = RECORD_IDLE;
            _recordedPoints.clear();
            drawRecordTrack();
            _menuSelectionIdx = -1;
          } else {
            _menuSelectionIdx = 13;
            drawRecordTrack();
          }
        }
      }
    }

    // GPS Recording Loop (when ACTIVE)
    if (_recordingState == RECORD_ACTIVE) {
      unsigned long now = millis();
      if (now - _lastPointTime > 2000) {
        if (gpsManager.isFixed()) {
          double currentLat = gpsManager.getLatitude();
          double currentLon = gpsManager.getLongitude();
          if (_recordedPoints.size() > 0) {
            GPSPoint &lastPoint = _recordedPoints.back();
            double dist = gpsManager.distanceBetween(
                lastPoint.lat, lastPoint.lon, currentLat, currentLon);
            if (dist > 5) {
              GPSPoint newPoint;
              newPoint.lat = currentLat;
              newPoint.lon = currentLon;
              newPoint.timestamp = now;
              _recordedPoints.push_back(newPoint);
              double distToStart = gpsManager.distanceBetween(
                  _recordStartLat, _recordStartLon, currentLat, currentLon);
              if (distToStart < 15 && _recordedPoints.size() > 20) {
                _recordingState = RECORD_COMPLETE;
              }
            }
          }
        }
        _lastPointTime = now;
      }
    }

    // UI Refresh
    unsigned long now = millis();
    if (_needsStaticRedraw) {
      drawRecordTrackStatic();
      _needsStaticRedraw = false;
      _lastUpdate = now;
    }
    if (now - _lastUpdate > 500) {
      drawRecordTrack();
      _lastUpdate = now;
    }
  } else if (_state == STATE_NO_GPS) {
    if (touched) {
      // BACK Button (Bottom-Left)
      if (p.x < 100 && p.y > 240) {
        _state = STATE_MENU;
        _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                                SCREEN_HEIGHT - STATUS_BAR_HEIGHT,
                                _ui->getBackgroundColor());
        drawMenu();
        _ui->drawStatusBar();
      }
    }
  } else if (_state == STATE_RACING) {
    if (touched) {
      if (p.x < 50 && p.y > 200) {
        if (millis() - _lastTouchTime < 200)
          return;
        _lastTouchTime = millis();
        finalizeRaceSession();
        return;
      }
      if (p.x > 150 && p.x < 330 && p.y > STOP_BTN_Y) {
        if (millis() - _lastTouchTime < 200)
          return;
        _lastTouchTime = millis();
        finalizeRaceSession();
        return;
      }
    }

    if (_finishSet)
      checkFinishLine();

    // Update Max Stats
    float curSpeed = gpsManager.getSpeedKmph();
    if (curSpeed > _maxSpeedSession)
      _maxSpeedSession = curSpeed;
    int curRpm = gpsManager.getRPM();
    if (curRpm > _maxRpmSession)
      _maxRpmSession = (unsigned long)curRpm;

    if (sessionManager.isLogging() && (millis() - _lastUpdate > 100)) {
      String data = String(millis()) + "," +
                    String(gpsManager.getLatitude(), 6) + "," +
                    String(gpsManager.getLongitude(), 6) + "," +
                    String(gpsManager.getSpeedKmph()) + "," +
                    String(gpsManager.getSatellites()) + "," +
                    String(gpsManager.getAltitude(), 2) + "," +
                    String(gpsManager.getHeading(), 2);
      sessionManager.logData(data);
    }

    if (_needsStaticRedraw) {
      drawRacingStatic();
      _lastUpdate = 0;
      _needsStaticRedraw = false;
    }

    if (millis() - _lastUpdate > 100) {
      drawRacing();
      _lastUpdate = millis();
    }

    // --- Predictive Timing Logic ---
    static double lastLatRef = 0, lastLonRef = 0;
    if (_isRecording) {
      double curLat = gpsManager.getLatitude();
      double curLon = gpsManager.getLongitude();
      // Init last pos if just started
      if (lastLatRef == 0 &&
          _recordedPoints.size() > 0) { // Or use start coords
        lastLatRef = curLat;
        lastLonRef = curLon;
      }

      if (lastLatRef != 0) {
        double d =
            gpsManager.distanceBetween(lastLatRef, lastLonRef, curLat, curLon);
        if (d > 0.5)
          _currentLapDist += d;

        // --- Delta Calculation ---
        if (!sessionManager.referenceLap.empty()) {
          float refTime = sessionManager.getReferenceTime(_currentLapDist);
          if (refTime > 0) {
            unsigned long lapTime = millis() - _currentLapStart;
            _currentDelta = (float)(lapTime - refTime) / 1000.0f;
          }
        } else {
          _currentDelta = 0.0f;
        }
      }
      lastLatRef = curLat;
      lastLonRef = curLon;

      // Calculate Delta
      float refTime = sessionManager.getReferenceTime(_currentLapDist);
      if (refTime > 0) {
        unsigned long curTime = millis() - _currentLapStart;
        // Delta = Actual - Reference
        // Negative = FASTER (Green)
        // Positive = SLOWER (Red)
        _currentDelta = (float)curTime - refTime;
      }
    } else {
      _currentLapDist = 0;
      lastLatRef = 0;
    }
  }
}

// --- PEMBANTU PENGGAMBARAN ---

void LapTimerScreen::drawMenu() {
  TFT_eSPI *tft = _ui->getTft();

  // Header
  tft->drawFastHLine(0, 20, SCREEN_WIDTH, COLOR_SECONDARY);
  tft->setTextDatum(TC_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->setTextColor(COLOR_TEXT, COLOR_BG);
  tft->drawString("LAP TIMER", SCREEN_WIDTH / 2, 25);

  // Back Arrow
  tft->setTextDatum(TL_DATUM);
  tft->drawString("<", 10, 25);

  // Buttons
  int startY = 60;
  int btnHeight = 50;
  int btnWidth = 360;
  int gap = 8;
  int x = (SCREEN_WIDTH - btnWidth) / 2;

  const char *menuItems[] = {"SELECT TRACK", "RACE SCREEN", "SESSION SUMMARY",
                             "RECORD TRACK"};

  for (int i = 0; i < 4; i++) {
    int y = startY + (i * (btnHeight + gap));

    // Determine Color based on selection
    uint16_t btnColor = (i == _menuSelectionIdx) ? TFT_RED : TFT_DARKGREY;

    tft->fillRoundRect(x, y, btnWidth, btnHeight, 5, btnColor);
    tft->setTextColor(TFT_WHITE, btnColor);
    tft->setTextDatum(MC_DATUM);
    tft->drawString(menuItems[i], SCREEN_WIDTH / 2, y + btnHeight / 2 + 2);
  }
  _ui->drawStatusBar();
}

void LapTimerScreen::drawSearching() {
  TFT_eSPI *tft = _ui->getTft();
  int cx = SCREEN_WIDTH / 2;
  int cy = SCREEN_HEIGHT / 2 - 20;

  // --- Draw Icon ---
  tft->setTextColor(TFT_WHITE, COLOR_BG);

  // 1. Map (Trapezoid-like)
  int mapY = cy + 10;
  tft->drawLine(cx - 20, mapY, cx + 20, mapY, TFT_WHITE); // Top
  tft->drawLine(cx + 20, mapY, cx + 30, mapY + 25,
                TFT_WHITE); // Right Slope
  tft->drawLine(cx + 30, mapY + 25, cx - 30, mapY + 25,
                TFT_WHITE);                                    // Bottom
  tft->drawLine(cx - 30, mapY + 25, cx - 20, mapY, TFT_WHITE); // Left Slope

  // Dotted Path inside (Mock)
  for (int i = 0; i < 3; i++) {
    tft->fillCircle(cx - 15 + (i * 15), mapY + 12, 2, TFT_WHITE);
  }

  // 2. Pin (Above Map)
  int pinY = cy - 10;
  tft->fillCircle(cx, pinY, 8, TFT_WHITE); // Head
  tft->fillTriangle(cx - 8, pinY, cx + 8, pinY, cx, pinY + 15,
                    TFT_WHITE);            // Point
  tft->fillCircle(cx, pinY, 3, TFT_BLACK); // Hole

  // --- Draw Text ---
  tft->setTextDatum(TC_DATUM);
  tft->setFreeFont(&Org_01); // Standard font
  tft->setTextSize(1);
  tft->drawString("Searching nearby Tracks", cx, cy + 50);

  _ui->drawStatusBar();
}

void LapTimerScreen::drawTrackList() {
  TFT_eSPI *tft = _ui->getTft();

  // Clear Screen (Below StatusBar)
  tft->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                SCREEN_HEIGHT - STATUS_BAR_HEIGHT, TFT_BLACK);

  // --- 1. HEADER ---
  int headY = 20; // Y-coord for line
  tft->drawFastHLine(0, headY, SCREEN_WIDTH, COLOR_SECONDARY);

  // Title "Nearby Tracks" (Moved to Center for consistency)
  tft->setTextDatum(TC_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->setTextColor(TFT_WHITE, TFT_BLACK); // Global White Text
  tft->drawString("SELECT TRACK", SCREEN_WIDTH / 2, headY + 8);

  // Back Button (<) Left
  tft->setTextDatum(TL_DATUM);
  tft->setTextSize(1);
  tft->drawString("<", 10, 25);

  // "New Track" Button (Top Right) -> "+" Icon style
  int btnX = SCREEN_WIDTH - 40;
  int btnY = 25;
  // Draw Circle Button
  // tft->fillCircle(btnX + 10, btnY + 10, 15, 0x10A2); // Slate Circle
  // tft->drawCircle(btnX + 10, btnY + 10, 15, TFT_WHITE);
  // tft->drawString("+", btnX + 5, btnY + 2);

  // Or "NEW" Text Button
  int newW = 50;
  int newH = 20;
  int newX = SCREEN_WIDTH - newW - 10;
  tft->fillRoundRect(newX, 25, newW, newH, 4, 0x10A2);
  tft->drawRoundRect(newX, 25, newW, newH, 4, TFT_WHITE);
  tft->setTextDatum(MC_DATUM);
  tft->setTextSize(1);
  tft->drawString("NEW", newX + newW / 2, 25 + newH / 2 + 1);

  // --- 2. LIST ---
  int startY = 60;
  int itemH = 55; // Taller for card style
  int itemW = SCREEN_WIDTH - 20;
  int itemX = 10;
  int gap = 8;

  tft->setTextDatum(TL_DATUM);

  if (_tracks.empty()) {
    tft->setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("No tracks found.", SCREEN_WIDTH / 2, 100);
    tft->drawString("Enable GPS or Create New.", SCREEN_WIDTH / 2, 125);
    _ui->drawStatusBar();
    return;
  }

  // Draw Items
  for (size_t i = 0; i < _tracks.size(); i++) {
    int y = startY + (i * (itemH + gap));
    if (y + itemH > SCREEN_HEIGHT)
      break; // Pagination limit

    // Card BG
    tft->fillRoundRect(itemX, y, itemW, itemH, 6, 0x18E3); // Charcoal
    tft->drawRoundRect(itemX, y, itemW, itemH, 6, TFT_DARKGREY);

    // Track Icon (Left)
    // tft->fillCircle(itemX + 20, y + itemH / 2, 10, TFT_BLACK);
    // Draw Flag or Dot
    tft->fillCircle(itemX + 20, y + itemH / 2, 4,
                    _tracks[i].isCustom ? TFT_CYAN : TFT_GOLD);

    // Name
    tft->setTextColor(TFT_WHITE, 0x18E3);
    tft->setTextFont(2); // Mid size
    tft->setTextDatum(ML_DATUM);
    tft->drawString(_tracks[i].name, itemX + 40, y + itemH / 2 - 5);

    // Detail (Lat/Lon or Configs)
    tft->setTextColor(TFT_SILVER, 0x18E3);
    tft->setTextFont(1);
    char buf[32];
    sprintf(buf, "%d Configs", _tracks[i].configs.size());
    tft->drawString(buf, itemX + 40, y + itemH / 2 + 10);

    // Arrow Right
    tft->setTextColor(TFT_DARKGREY, 0x18E3);
    tft->drawString(">", itemX + itemW - 15, y + itemH / 2);
  }

  _ui->drawStatusBar();
}

void LapTimerScreen::drawTrackOptionsPopup() {
  TFT_eSPI *tft = _ui->getTft();

  // Popup Dimensions
  int w = 220;
  int h = 150;
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2 + 10;

  // Draw Box (Black with White Border)
  tft->fillRoundRect(x, y, w, h, 5, TFT_BLACK);
  tft->drawRoundRect(x, y, w, h, 5, TFT_WHITE);

  // Options
  const char *options[] = {"Select", "Select & Edit", "Invert",
                           "Reinit best Lap", "Remove"};
  int startY = y + 10;
  int itemH = 25;

  tft->setTextDatum(TL_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(1);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);

  for (int i = 0; i < 5; i++) {
    tft->drawString(options[i], x + 15, startY + (i * itemH));
  }
}

void LapTimerScreen::drawTrackDetails() {
  TFT_eSPI *tft = _ui->getTft();
  if (_selectedTrackIdx < 0 || _selectedTrackIdx >= _tracks.size())
    return;
  Track &t = _tracks[_selectedTrackIdx];

  tft->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                SCREEN_HEIGHT - STATUS_BAR_HEIGHT, TFT_BLACK);

  // Divider
  tft->drawFastHLine(0, 20, SCREEN_WIDTH, COLOR_SECONDARY);

  // Title (Centered Below Header)
  tft->setTextDatum(MC_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->drawString("TRACK DETAILS", SCREEN_WIDTH / 2, 45);

  // Back Arrow
  tft->setTextDatum(TL_DATUM);
  tft->setTextSize(1);
  tft->drawString("<", 10, 25);

  // --- LAYOUT ---
  // --- LAYOUT ---
  int mapX = 10;
  int mapY = 80; // Shifted Down (was 60)
  int mapW = 230;
  int mapH = 150; // Reduced height (was 160)

  int infoX = 250;
  int infoY = 80; // Shifted Down
  int infoW = 220;
  int infoH = 150;

  // 1. MAP CARD
  tft->fillRoundRect(mapX, mapY, mapW, mapH, 8, 0x18E3); // Charcoal
  tft->drawRoundRect(mapX, mapY, mapW, mapH, 8, TFT_DARKGREY);

  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->setTextDatum(MC_DATUM);
  tft->drawString("Map Preview", mapX + mapW / 2, mapY + mapH / 2);

  // 2. INFO CARD
  tft->fillRoundRect(infoX, infoY, infoW, infoH, 8, 0x18E3);
  tft->drawRoundRect(infoX, infoY, infoW, infoH, 8, TFT_DARKGREY);

  tft->setTextDatum(TL_DATUM);
  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->drawString("NAME:", infoX + 12, infoY + 12);

  // Rename Icon/Button (Top Right of Info Card)
  tft->fillRoundRect(infoX + infoW - 50, infoY + 5, 45, 18, 4, 0x10A2);
  tft->drawRoundRect(infoX + infoW - 50, infoY + 5, 45, 18, 4, TFT_SILVER);
  tft->setTextColor(TFT_WHITE, 0x10A2);
  tft->setTextDatum(MC_DATUM);
  tft->setTextFont(1);
  tft->drawString("EDIT", infoX + infoW - 27, infoY + 14);

  tft->setTextColor(TFT_WHITE, 0x18E3);
  tft->setTextFont(2);
  tft->setTextDatum(TL_DATUM); // RESET DATUM after EDIT button
  // Truncate name to clear the EDIT button
  String dispName = t.name;
  int maxW = infoW - 60; // Increased margin to avoid EDIT button
  if (tft->textWidth(dispName) > maxW) {
    while (tft->textWidth(dispName + "...") > maxW && dispName.length() > 0) {
      dispName = dispName.substring(0, dispName.length() - 1);
    }
    dispName += "...";
  }
  tft->drawString(dispName, infoX + 12, infoY + 28);

  tft->setTextFont(1);
  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->setTextDatum(TL_DATUM);
  tft->drawString("BEST LAP:", infoX + 12, infoY + 58);

  tft->setTextColor(TFT_GOLD, 0x18E3);
  tft->setTextFont(2);
  tft->drawString("01:10.5", infoX + 12, infoY + 74); // Placeholder

  tft->setTextFont(1);
  tft->setTextColor(TFT_SILVER, 0x18E3);
  char confBuf[32];
  sprintf(confBuf, "Configs: %d", t.configs.size());
  tft->drawString(confBuf, infoX + 12, infoY + 100);

  // 3. ACTION BUTTONS
  int btnY = 240;
  int btnH = 45;
  int btnW = 180;

  // SELECT Button (Green/Slate) -> centered? Or split?
  // Let's put SELECT center-right, BACK center-left?
  // User asked for "Select & Edit". Maybe "EDIT" button?
  // We'll implemented "SELECT" for now.

  int selX = (SCREEN_WIDTH - btnW) / 2;
  tft->fillRoundRect(selX, btnY, btnW, btnH, 6, 0x05E0); // Greenish
  tft->drawRoundRect(selX, btnY, btnW, btnH, 6, TFT_WHITE);

  // BACK Button  // Back Triangle (Standardized Bottom-Left)
  tft->fillTriangle(10, 290, 25, 282, 25, 298, TFT_BLUE);

  tft->setTextColor(TFT_WHITE, 0x05E0);
  tft->setTextDatum(MC_DATUM);
  tft->setTextFont(2);
  tft->drawString("SELECT TRACK", selX + btnW / 2, btnY + btnH / 2 + 1);

  _ui->drawStatusBar();
}

void LapTimerScreen::drawRecordTrackStatic() {
  TFT_eSPI *tft = _ui->getTft();

  // Background
  tft->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                SCREEN_HEIGHT - STATUS_BAR_HEIGHT, TFT_BLACK);

  // --- 1. HEADER ---
  // --- 1. HEADER ---
  int headY = 20;
  tft->drawFastHLine(0, headY, SCREEN_WIDTH, COLOR_SECONDARY);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->setTextDatum(TC_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->drawString("TRACK RECORDER", SCREEN_WIDTH / 2, headY + 8);

  // Back Button (<)
  tft->setTextDatum(TL_DATUM);
  tft->setTextSize(1);
  tft->drawString("<", 10, 25);

  _ui->drawStatusBar();

  // Reset trackers to force dynamic redraw
  _lastRecordGpsFixed = false;
  _lastRecordSats = -1;
}

void LapTimerScreen::drawRecordTrack() {
  TFT_eSPI *tft = _ui->getTft();
  extern GPSManager gpsManager;

  // --- LAYOUT CONSTANTS ---
  // --- LAYOUT CONSTANTS ---
  const int H_BTN = 42;
  const int Y_BTN = STATUS_BAR_HEIGHT + 5; // Top, under Status Bar (25)

  const int Y_HEADER = Y_BTN + H_BTN + 10; // ~77
  const int H_HEADER = 30;

  const int Y_METRICS = Y_HEADER + H_HEADER + 10; // ~117
  const int Y_METRICS_VAL = Y_METRICS + 22;

  const int Y_FEEDBACK = Y_METRICS_VAL + 50; // ~190

  // --- 1. STATE CHANGE DETECTION ---
  bool stateChanged = (_recordingState != _lastRecordedStateRender);

  if (stateChanged) {
    // Clear Button Area (to prevent overlap when switching states)
    tft->fillRect(0, Y_BTN, SCREEN_WIDTH, H_BTN + 5, TFT_BLACK);

    // --- 1. HEADER (Premium Style) ---
    int headY = 20;
    tft->drawFastHLine(0, headY, SCREEN_WIDTH, COLOR_SECONDARY);

    // Title
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextDatum(TC_DATUM);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(2);
    tft->drawString("TRACK RECORDER", SCREEN_WIDTH / 2, headY + 8);

    // Back button (<)
    tft->setTextDatum(TL_DATUM);
    tft->setTextSize(1);
    tft->drawString("<", 10, 25);
  }

  // --- 2. GPS STATUS (Premium Card Style) ---
  int cardX = 10;
  int cardY = 55;
  int cardW = SCREEN_WIDTH - 20;
  int cardH = 40; // Compact height

  bool gpsFixed = gpsManager.isFixed();
  int sats = gpsManager.getSatellites();

  if (gpsFixed != _lastRecordGpsFixed || sats != _lastRecordSats ||
      stateChanged) {
    if (stateChanged) {
      // Card Background
      tft->fillRoundRect(cardX, cardY, cardW, cardH, 8, 0x18E3); // Charcoal
      tft->drawRoundRect(cardX, cardY, cardW, cardH, 8, TFT_DARKGREY);

      // Label
      tft->setTextColor(TFT_SILVER, 0x18E3);
      tft->setTextDatum(ML_DATUM);
      tft->setTextFont(2);
      tft->drawString("GPS STATUS", cardX + 10, cardY + cardH / 2);
    }

    // Status Text
    uint16_t statusColor = TFT_RED;
    String statusText = "NO FIX";
    if (gpsFixed) {
      if (sats >= 6) {
        statusColor = TFT_GREEN;
        statusText = "READY";
      } else {
        statusColor = TFT_YELLOW;
        statusText = "WEAK";
      }
    }

    tft->setTextColor(statusColor, 0x18E3);
    tft->setTextDatum(MR_DATUM);
    tft->setTextPadding(120);
    tft->setTextFont(2);
    tft->setTextSize(1); // Ensure size is 1 to prevent glitch
    tft->drawString(statusText + " (" + String(sats) + ")", cardX + cardW - 10,
                    cardY + cardH / 2);
    tft->setTextPadding(0);

    _lastRecordGpsFixed = gpsFixed;
    _lastRecordSats = sats;
  }

  // --- 3. MAIN CONTENT DRAWING ---
  if (stateChanged) {
    // Clear Content Area (Below GPS Card)
    int clearY = cardY + cardH + 5;
    tft->fillRect(0, clearY, SCREEN_WIDTH, SCREEN_HEIGHT - clearY, TFT_BLACK);

    // Grid Layout for Metrics (Slate Boxes)
    int gridY = clearY + 10;
    int boxW = (SCREEN_WIDTH - 25) / 2;
    int boxH = 70;
    int box1X = 10;
    int box2X = 15 + boxW;

    // --- STATIC ELEMENTS PER STATE ---
    if (_recordingState == RECORD_IDLE) {
      // Instructions
      tft->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft->setTextFont(2);
      tft->setTextDatum(MC_DATUM);
      tft->drawString("Go to Start Line", SCREEN_WIDTH / 2, gridY + 15);
      tft->drawString("& Tap Start", SCREEN_WIDTH / 2, gridY + 35);

    } else if (_recordingState == RECORD_ACTIVE) {
      // DRAW GRID BOXES
      // Box 1: Points
      tft->fillRoundRect(box1X, gridY, boxW, boxH, 6, 0x10A2); // Slate
      tft->setTextColor(TFT_SILVER, 0x10A2);
      tft->setTextFont(1);
      tft->setTextDatum(TL_DATUM);
      tft->drawString("POINTS", box1X + 8, gridY + 5);

      // Box 2: Time
      tft->fillRoundRect(box2X, gridY, boxW, boxH, 6, 0x10A2); // Slate
      tft->setTextColor(TFT_SILVER, 0x10A2);
      tft->setTextFont(1);
      tft->setTextDatum(TL_DATUM);
      tft->drawString("TIME", box2X + 8, gridY + 5);

      // STOP Button below grid
      int btnY = 240;

      // STOP Button
      int btnW = 180;
      int btnX = (SCREEN_WIDTH - btnW) / 2;
      uint16_t btnColor = (_menuSelectionIdx == 11) ? 0x8000 : TFT_RED;
      uint16_t txtColor = TFT_WHITE;

      tft->fillRoundRect(btnX, btnY, btnW, H_BTN, 8, btnColor);
      if (_menuSelectionIdx == 11)
        tft->drawRoundRect(btnX, btnY, btnW, H_BTN, 8, TFT_WHITE);

      tft->setTextColor(txtColor, btnColor);
      tft->setTextFont(4);
      tft->setTextDatum(MC_DATUM);
      tft->drawString("STOP", SCREEN_WIDTH / 2, btnY + H_BTN / 2 + 2);

    } else if (_recordingState == RECORD_COMPLETE) {

      // Success Message in Grid area
      tft->setTextColor(TFT_GREEN, TFT_BLACK);
      tft->setTextFont(4);
      tft->setTextDatum(MC_DATUM);
      // FIX: Move UP (was gridY + 25)
      tft->drawString("DONE!", SCREEN_WIDTH / 2, gridY + 10);

      int btnY = gridY + boxH + 20;

      // Buttons: SAVE | DISCARD
      int btnW = 100;
      int gap = 20;
      int startX = (SCREEN_WIDTH - (btnW * 2 + gap)) / 2;

      uint16_t saveColor = (_menuSelectionIdx == 12) ? 0x03E0 : TFT_GREEN;
      uint16_t saveTxt = (_menuSelectionIdx == 12) ? TFT_WHITE : TFT_BLACK;

      tft->fillRoundRect(startX, btnY, btnW, H_BTN, 6, saveColor);
      if (_menuSelectionIdx == 12)
        tft->drawRoundRect(startX, btnY, btnW, H_BTN, 6, TFT_WHITE);

      tft->setTextColor(saveTxt, saveColor);
      tft->setTextDatum(MC_DATUM);
      tft->drawString("SAVE", startX + btnW / 2, btnY + H_BTN / 2);

      uint16_t delColor = (_menuSelectionIdx == 13) ? 0x8000 : TFT_RED;

      tft->fillRoundRect(startX + btnW + gap, btnY, btnW, H_BTN, 6, delColor);
      if (_menuSelectionIdx == 13)
        tft->drawRoundRect(startX + btnW + gap, btnY, btnW, H_BTN, 6,
                           TFT_WHITE);

      tft->setTextColor(TFT_WHITE, delColor);
      tft->drawString("DEL", startX + btnW + gap + btnW / 2, btnY + H_BTN / 2);
    }
    _lastRecordedStateRender = _recordingState;
  }

  // --- 4. DYNAMIC UPDATES ---
  int clearY = cardY + cardH + 5;
  int gridY = clearY + 10;
  int boxW = (SCREEN_WIDTH - 25) / 2;
  int box1X = 10;
  int box2X = 15 + boxW;

  if (_recordingState == RECORD_IDLE) {
    static bool lastReady = false;
    bool ready = true; // BYPASS

    if (ready != lastReady || stateChanged) {
      int btnW = 180;
      int btnX = (SCREEN_WIDTH - btnW) / 2;
      int btnY = 240;

      if (ready) {
        uint16_t btnColor = (_menuSelectionIdx == 10) ? 0x05E0 : TFT_GREEN;
        uint16_t txtColor = (_menuSelectionIdx == 10) ? TFT_WHITE : TFT_BLACK;

        tft->fillRoundRect(btnX, btnY, btnW, H_BTN, 8, btnColor);
        if (_menuSelectionIdx == 10)
          tft->drawRoundRect(btnX, btnY, btnW, H_BTN, 8, TFT_WHITE);

        tft->setTextColor(txtColor, btnColor);
        tft->setTextFont(4);
        tft->setTextDatum(MC_DATUM);
        tft->drawString("START", SCREEN_WIDTH / 2, btnY + H_BTN / 2 + 2);

        // Clear Msg
        tft->fillRect(0, btnY - 30, SCREEN_WIDTH, 25, TFT_BLACK);

      } else {
        tft->fillRect(btnX, btnY, btnW, H_BTN, TFT_BLACK);
        tft->setTextColor(TFT_ORANGE, TFT_BLACK);
        tft->setTextFont(2);
        tft->setTextDatum(MC_DATUM);
        tft->drawString("WAITING FOR GPS...", SCREEN_WIDTH / 2, btnY + 20);
      }
      lastReady = ready;
    }
  } else if (_recordingState == RECORD_ACTIVE) {
    // Dynamic Values inside Boxes

    // Points Value (Left Box)
    tft->setTextColor(TFT_SKYBLUE, 0x10A2);
    tft->setTextFont(4);
    tft->setTextSize(1); // Ensure size 1
    tft->setTextDatum(MC_DATUM);
    tft->setTextPadding(boxW - 10);
    tft->drawNumber(_recordedPoints.size(), box1X + boxW / 2, gridY + 28);

    // Time Value (Right Box)
    tft->setTextColor(TFT_WHITE, 0x10A2);
    unsigned long elapsed = (millis() - _recordingStartTime) / 1000;
    tft->setTextPadding(boxW - 10);
    tft->drawString(String(elapsed) + "s", box2X + boxW / 2, gridY + 28);
    tft->setTextPadding(0);

    // Feedback
    int feedY = 230;
    double currentLat = gpsManager.getLatitude();
    double currentLon = gpsManager.getLongitude();
    double distToStart = gpsManager.distanceBetween(
        _recordStartLat, _recordStartLon, currentLat, currentLon);

    tft->setTextFont(2);
    tft->setTextDatum(MC_DATUM);
    tft->setTextPadding(200);

    if (distToStart < 20 && _recordedPoints.size() > 10) {
      tft->setTextColor(TFT_GREEN, TFT_BLACK);
      tft->drawString("FINISH DETECTED!", SCREEN_WIDTH / 2, feedY);
    } else {
      tft->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft->drawString("Dist: " + String(distToStart, 0) + "m", SCREEN_WIDTH / 2,
                      feedY);
    }
    tft->setTextPadding(0);

  } else if (_recordingState == RECORD_COMPLETE) {
    if (stateChanged) {
      tft->setTextColor(TFT_WHITE, TFT_BLACK);
      tft->setTextFont(2);
      tft->setTextDatum(MC_DATUM);

      unsigned long elapsed =
          (_recordedPoints.back().timestamp - _recordingStartTime) / 1000;
      String stats = String(_recordedPoints.size()) + " Pts  |  " +
                     String(elapsed) + " Sec";

      // FIX: Move text UP to avoid overlap with buttons at gridY + 70
      // Previous: gridY + 70 (Overlapped)
      // New: Stats at gridY + 35
      tft->drawString(stats, SCREEN_WIDTH / 2, gridY + 35);
    }
  }
}

void LapTimerScreen::drawNoGPS() {
  TFT_eSPI *tft = _ui->getTft();

  // Colors (Renamed to avoid config.h macro conflicts)
  uint16_t L_COLOR_BG = TFT_BLACK;
  uint16_t L_COLOR_CARD = 0x18E3; // Charcoal
  uint16_t L_COLOR_BTN = 0x10A2;  // Slate
  uint16_t L_COLOR_TEXT = TFT_WHITE;
  uint16_t L_COLOR_LABEL = TFT_SILVER;

  tft->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                SCREEN_HEIGHT - STATUS_BAR_HEIGHT, L_COLOR_BG);

  // --- HEADER ---
  int headY = 20;
  tft->drawFastHLine(0, headY, SCREEN_WIDTH, COLOR_SECONDARY);

  tft->setTextColor(TFT_WHITE, L_COLOR_BG);
  tft->setTextDatum(TC_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->drawString("GPS STATUS", SCREEN_WIDTH / 2, headY + 8);

  // --- MESSAGE CARD ---
  int cardW = 260;
  int cardH = 100;
  int cardX = (SCREEN_WIDTH - cardW) / 2;
  int cardY = (SCREEN_HEIGHT - cardH) / 2;

  tft->fillRoundRect(cardX, cardY, cardW, cardH, 8, L_COLOR_CARD);
  tft->drawRoundRect(cardX, cardY, cardW, cardH, 8, TFT_DARKGREY);

  // Message Text
  tft->setTextColor(TFT_RED, L_COLOR_CARD);
  tft->setTextSize(1);
  tft->setTextDatum(MC_DATUM);
  tft->drawString("NO SATELLITES FIX", SCREEN_WIDTH / 2, cardY + 30);

  tft->setTextColor(L_COLOR_LABEL, L_COLOR_CARD);
  tft->drawString("Cannot record track.", SCREEN_WIDTH / 2, cardY + 55);
  tft->drawString("Please check GPS antenna.", SCREEN_WIDTH / 2, cardY + 75);

  // 3. START BUTTON (Bottom Center)
  int btnW = 180;
  int btnH = 45;
  int btnX = (SCREEN_WIDTH - btnW) / 2;
  int btnY = 255; // Shifted Down (was 240)

  tft->fillRoundRect(btnX, btnY, btnW, btnH, 6, L_COLOR_BTN);
  tft->drawRoundRect(btnX, btnY, btnW, btnH, 6, TFT_WHITE);

  tft->setTextColor(TFT_WHITE, L_COLOR_BTN);
  tft->setTextSize(1);
  tft->drawString("CONTINUE", SCREEN_WIDTH / 2, btnY + (btnH / 2) - 2);

  _ui->drawStatusBar();
}

void LapTimerScreen::drawSummary() {
  TFT_eSPI *tft = _ui->getTft();
  tft->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                SCREEN_HEIGHT - STATUS_BAR_HEIGHT, TFT_BLACK);

  // --- 1. HEADER ---
  tft->drawFastHLine(0, 20, SCREEN_WIDTH, COLOR_SECONDARY);

  // Title
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->setTextDatum(TC_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->drawString("SESSION SUMMARY", SCREEN_WIDTH / 2, 28);

  // Back Arrow (Standardized Bottom-Left)
  tft->fillTriangle(10, 290, 25, 282, 25, 298, COLOR_ACCENT);

  // --- DATA PROCESSING ---
  int bestIdx = -1;
  unsigned long bestTime = 0;
  unsigned long totalTime = 0;

  if (!_lapTimes.empty()) {
    bestTime = _lapTimes[0];
    bestIdx = 0;
    for (int i = 0; i < _lapTimes.size(); i++) {
      totalTime += _lapTimes[i];
      if (_lapTimes[i] < bestTime) {
        bestTime = _lapTimes[i];
        bestIdx = i;
      }
    }
  }

  // --- 2. BEST LAP CARD (Premium Look) ---
  int cardX = 10;
  int cardY = 55;
  int cardW = SCREEN_WIDTH - 20;
  int cardH = 65;

  tft->fillRoundRect(cardX, cardY, cardW, cardH, 8,
                     0x18E3); // Dark Grey/Charcoalish
  tft->drawRoundRect(cardX, cardY, cardW, cardH, 8, TFT_DARKGREY);

  // Label
  tft->setTextSize(1);
  tft->setTextFont(2);
  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->setTextDatum(TL_DATUM);
  tft->drawString("BEST LAP", cardX + 10, cardY + 8);

  if (bestIdx != -1) {
    // Lap Number Tag
    String lapTag = "LAP " + String(bestIdx + 1);
    int tagW = tft->textWidth(lapTag);
    tft->fillRoundRect(cardX + cardW - tagW - 15, cardY + 8, tagW + 10, 16, 4,
                       TFT_GOLD);
    tft->setTextColor(TFT_BLACK, TFT_GOLD);
    tft->setTextDatum(MC_DATUM);
    tft->drawString(lapTag, cardX + cardW - 10 - tagW / 2, cardY + 16);

    // Time Value (Big & Center)
    int ms = bestTime % 1000;
    int s = (bestTime / 1000) % 60;
    int m = (bestTime / 60000);
    char buf[16];
    sprintf(buf, "%d:%02d.%02d", m, s, ms / 10);

    tft->setTextColor(TFT_WHITE, 0x18E3);
    tft->setTextFont(6); // Large Font
    tft->setTextDatum(MC_DATUM);
    tft->drawString(buf, cardX + cardW / 2, cardY + cardH / 2 + 8);
  } else {
    tft->setTextColor(TFT_DARKGREY, 0x18E3);
    tft->setTextFont(4);
    tft->setTextDatum(MC_DATUM);
    // --- 3. STATS GRID (Rounded Boxes) ---
    int gridY = cardY + cardH + 10;
    int boxW = (SCREEN_WIDTH - 25) / 2;
    int boxH = 45;

    // Box 1: Total Laps
    tft->fillRoundRect(10, gridY, boxW, boxH, 6, 0x10A2); // Darker Slate
    tft->setTextColor(TFT_SILVER, 0x10A2);
    tft->setTextFont(1);
    tft->setTextDatum(TL_DATUM);
    tft->drawString("TOTAL LAPS", 18, gridY + 5);

    tft->setTextFont(4);
    tft->setTextColor(TFT_SKYBLUE, 0x10A2);
    tft->setTextDatum(MC_DATUM);
    tft->drawNumber(_lapCount, 10 + boxW / 2, gridY + 25);

    // Box 2: Max RPM or Theoretical
    tft->fillRoundRect(15 + boxW, gridY, boxW, boxH, 6, 0x10A2);
    tft->setTextColor(TFT_SILVER, 0x10A2);
    tft->setTextFont(1);
    tft->setTextDatum(TL_DATUM);
    tft->drawString("MAX RPM", 23 + boxW, gridY + 5);

    tft->setTextFont(4);
    tft->setTextColor(TFT_ORANGE, 0x10A2);
    tft->setTextDatum(MC_DATUM);
    tft->drawNumber(_maxRpmSession, 15 + boxW + boxW / 2, gridY + 25);

    // --- 4. DATA LIST (Top Laps) ---
    int listY = gridY + boxH + 10;
    tft->drawFastHLine(20, listY, SCREEN_WIDTH - 40, TFT_DARKGREY);

    // Header tiny
    tft->setTextFont(1);
    tft->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft->setTextDatum(TL_DATUM);
    tft->drawString("RECENT LAPS", 20, listY + 5);

    int itemsToShow = 3;
    int startIdx =
        (_lapTimes.size() > itemsToShow) ? _lapTimes.size() - itemsToShow : 0;

    int rowY = listY + 20;
    for (int i = _lapTimes.size() - 1; i >= startIdx; i--) {
      if (i < 0)
        break;

      unsigned long t = _lapTimes[i];
      int ms = t % 1000;
      int s = (t / 1000) % 60;
      int m = (t / 60000);
      char buf[32];

      uint16_t color = (i == bestIdx) ? TFT_GREEN : TFT_WHITE;
      sprintf(buf, "%d:%02d.%02d", m, s, ms / 10);

      tft->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      tft->drawString(String(i + 1) + ".", 20, rowY);

      tft->setTextColor(color, TFT_BLACK);
      tft->setTextFont(2);
      tft->drawString(buf, 60, rowY);

      if (bestIdx != -1 && i != bestIdx) {
        long delta = (long)t - (long)bestTime;
        int d_ms = abs(delta) % 1000;
        int d_s = abs(delta) / 1000;
        sprintf(buf, "%s%d.%02d", (delta > 0) ? "+" : "-", d_s, d_ms / 10);
        tft->setTextColor((delta > 0) ? TFT_RED : TFT_GREEN, TFT_BLACK);
        tft->setTextDatum(TR_DATUM);
        tft->drawString(buf, SCREEN_WIDTH - 20, rowY);
        tft->setTextDatum(TL_DATUM);
      }
      rowY += 20;
    }
    _ui->drawStatusBar();
  }
}

void LapTimerScreen::drawRacingStatic() {
  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(TFT_BLACK);

  // --- FONT SAFETY ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextPadding(0);

  // --- FORCE DYNAMIC REDRAW ---
  // Reset trackers so drawRacing() repopulates everything
  _lastSpeed = -999.0;
  _lastSats = -1;
  _lastRpmRender = -1;
  _lastMaxRpmRender = 0;
  _lastLapCountRender = -1;
  _lastRecordedStateRender = (RecordingState)-1;
  _lastLastLapTimeRender = -1;
  _lastBestLapTimeRender = -1;
  _maxSpeedSessionRender = -1.0;
  _maxRpmSessionRender = 0;

  // --- LAYOUT DEFINITIONS (480x320) ---
  int rpmY = STATUS_BAR_HEIGHT + 2;
  int rpmH = 22;

  // MAIN DASHBOARD SPLIT (Upper half: 24-210)
  int midY = rpmY + rpmH + 6;
  int dashH = 180;

  // THREE COLUMN SYSTEM
  int colW = SCREEN_WIDTH / 3; // 160px each

  // 1. RPM BAR (PRO LOOK)
  tft->drawRoundRect(5, rpmY, SCREEN_WIDTH - 10, rpmH, 5, TFT_DARKGREY);
  // RPM Labels (0-10k) - Tiny dots
  for (int i = 1; i < 10; i++) {
    int dx = 5 + (i * (SCREEN_WIDTH - 10) / 10);
    tft->drawFastVLine(dx, rpmY + rpmH - 4, 3, TFT_SILVER);
  }

  // 2. LEFT COLUMN: TRACK INFO & STATS
  int leftX = 5;
  int leftW = colW - 10;
  tft->fillRoundRect(leftX, midY, leftW, dashH, 8, 0x18E3); // Charcoal

  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->setTextFont(1);
  tft->setTextDatum(TC_DATUM);
  tft->drawString("TRACK INFO", leftX + leftW / 2, midY + 5);
  tft->drawFastHLine(leftX + 10, midY + 16, leftW - 20, TFT_DARKGREY);

  // Labels for left column
  tft->setTextDatum(TL_DATUM);
  tft->drawString("MAX SPD", leftX + 10, midY + 25);
  tft->drawString("G-FORCE", leftX + 10, midY + 75);
  tft->drawString("SATS", leftX + 10, midY + 125);

  // 3. CENTER COLUMN: SPEED & DELTA
  int centerX = colW + 5;
  int centerW = colW - 10;
  // Speed Zone (Huge)
  tft->setTextColor(TFT_SILVER, TFT_BLACK);
  tft->setTextFont(2);
  tft->setTextDatum(BC_DATUM);
  tft->drawString("KM/H", SCREEN_WIDTH / 2, midY + dashH - 10);

  // 4. RIGHT COLUMN: LAP TIMES
  int rightX = (colW * 2) + 5;
  int rightW = colW - 10;
  tft->fillRoundRect(rightX, midY, rightW, dashH, 8, 0x18E3); // Charcoal

  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->setTextDatum(TC_DATUM);
  tft->drawString("LAP TIMES", rightX + rightW / 2, midY + 5);
  tft->drawFastHLine(rightX + 10, midY + 16, rightW - 20, TFT_DARKGREY);

  // Labels for right column
  tft->setTextDatum(TL_DATUM);
  tft->drawString("LAST", rightX + 10, midY + 28);  // Lowered
  tft->drawString("BEST", rightX + 10, midY + 108); // Lowered

  // --- 5. BOTTOM AREA: CONTROL & PREDICTIVE ---
  int footerY = midY + dashH + 5;
  // Background Box (Only draw border, text is dynamic)
  tft->drawRoundRect(5, footerY, SCREEN_WIDTH - 10, 80, 8, TFT_DARKGREY);

  _ui->drawStatusBar(true);
}

void LapTimerScreen::drawRPMBar(int rpm, int maxRpm) {
  TFT_eSPI *tft = _ui->getTft();
  int xStart = 7;
  int y = STATUS_BAR_HEIGHT + 7;
  int w = SCREEN_WIDTH - 14;
  int h = 16;

  if (maxRpm < 5000)
    maxRpm = 10000;

  int segments = 20;
  int segW = (w / segments) - 2;
  int activeSegs = map(constrain(rpm, 0, maxRpm), 0, maxRpm, 0, segments);

  for (int i = 0; i < segments; i++) {
    int sx = xStart + (i * (segW + 2));
    uint16_t color = 0x10A2; // Dark background for inactive

    if (i < activeSegs) {
      if (i < segments * 0.6)
        color = TFT_GREEN;
      else if (i < segments * 0.8)
        color = TFT_ORANGE;
      else
        color = TFT_RED;
    }

    tft->fillRect(sx, y, segW, h, color);
  }
}

void LapTimerScreen::drawRacing() {
  TFT_eSPI *tft = _ui->getTft();

  // --- FONT SAFETY (Prevent leaks from Font 6/7) ---
  tft->setTextSize(1);
  tft->setFreeFont(NULL);
  tft->setTextPadding(0);

  // --- RE-DEFINE LAYOUT CONSTANTS (Match drawRacingStatic) ---
  int rpmY = STATUS_BAR_HEIGHT + 2;
  int rpmH = 22;
  int midY = rpmY + rpmH + 6;
  int dashH = 180;
  int colW = SCREEN_WIDTH / 3;
  int footerY = midY + dashH + 5;

  uint16_t cardBg = 0x18E3; // Charcoal

  // --- 1. RPM BAR (Segmented Pro Look) ---
  int currentRpm = gpsManager.getRPM();
  if (currentRpm > _maxRpmSession)
    _maxRpmSession = currentRpm;
  if (abs(currentRpm - _lastRpmRender) > 50) {
    drawRPMBar(currentRpm, 10000); // Now uses segmented logic
    _lastRpmRender = currentRpm;
  }

  // --- 2. MAIN SPEED (Huge Center Value) ---
  float speed = gpsManager.getSpeedKmph();
  // Check units (km/h vs mph)
  Preferences prefs;
  prefs.begin("laptimer", true);
  bool useMph = prefs.getInt("units", 0) == 1;
  prefs.end();
  if (useMph)
    speed *= 0.621371;

  if (abs(speed - _lastSpeed) > 0.1) { // Lower threshold
    tft->setTextSize(1);
    tft->setTextFont(7); // Massive Digital
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->setTextDatum(MC_DATUM);
    // Explicitly clear speed area to prevent ghosting
    tft->fillRect(colW + 10, midY + 30, colW - 20, 110,
                  TFT_BLACK); // Larger clear area
    tft->drawString(String((int)speed), SCREEN_WIDTH / 2, midY + 85);
    _lastSpeed = speed;

    tft->setTextFont(1); // Safety Reset
  }

  // --- 3. SESSION INFO (Left Column) ---
  int sidePad = colW - 20;
  // Max Speed update
  if (abs(_maxSpeedSessionRender - _maxSpeedSession) > 0.1) {
    tft->setTextColor(TFT_WHITE, cardBg);
    tft->setTextFont(4);
    tft->setTextDatum(TL_DATUM);
    tft->setTextPadding(sidePad);
    tft->drawString(
        String((int)(useMph ? _maxSpeedSession * 0.621 : _maxSpeedSession)),
        5 + 10, midY + 42);
    _maxSpeedSessionRender = _maxSpeedSession;
  }
  // Sats update
  int sats = gpsManager.getSatellites();
  if (sats != _lastSats) {
    tft->setTextColor(TFT_WHITE, cardBg);
    tft->setTextFont(4);
    tft->setTextDatum(TL_DATUM);
    tft->setTextPadding(sidePad);
    tft->drawString(String(sats), 5 + 10, midY + 142);
    _lastSats = sats;
  }

  // --- 4. LAP TIMES (Right Column) ---
  // LAST LAP
  if (_lastLapTime != _lastLastLapTimeRender) {
    String text = "--:--.---";
    if (_lastLapTime > 0) {
      int ms = _lastLapTime % 1000;
      int s = (_lastLapTime / 1000) % 60;
      int m = (_lastLapTime / 60000);
      char b[16];
      sprintf(b, "%01d:%02d.%03d", m, s, ms);
      text = String(b);
    }
    tft->setTextColor(TFT_WHITE, cardBg);
    tft->setTextFont(2);
    tft->setTextDatum(TL_DATUM);
    tft->setTextPadding(colW - 25);
    tft->drawString(text, (colW * 2) + 15, midY + 54); // More space from label
    _lastLastLapTimeRender = _lastLapTime;
  }
  // BEST LAP
  if (_bestLapTime != _lastBestLapTimeRender) {
    String text = "--:--.---";
    if (_bestLapTime > 0) {
      int ms = _bestLapTime % 1000;
      int s = (_bestLapTime / 1000) % 60;
      int m = (_bestLapTime / 60000);
      char b[16];
      sprintf(b, "%01d:%02d.%03d", m, s, ms);
      text = String(b);
    }
    tft->setTextColor(TFT_GOLD, cardBg); // Gold for Best
    tft->setTextFont(2);
    tft->setTextDatum(TL_DATUM);
    tft->setTextPadding(colW - 25);
    tft->drawString(text, (colW * 2) + 15, midY + 134); // More space from label
    _lastBestLapTimeRender = _bestLapTime;
  }

  // --- 5. PREDICTIVE FOOTER ---
  // Background Box based on delta
  uint16_t deltaColor = COLOR_SECONDARY;
  if (_isRecording && !sessionManager.referenceLap.empty()) {
    deltaColor = (_currentDelta <= 0) ? TFT_GREEN : TFT_RED;
    // Draw "Glow" box
    tft->fillRoundRect(10, footerY + 5, SCREEN_WIDTH - 20, 70, 6, deltaColor);

    // Delta Text
    tft->setTextColor(TFT_WHITE, deltaColor);
    tft->setTextFont(6);
    tft->setTextDatum(MC_DATUM);
    char dBuf[16];
    snprintf(dBuf, sizeof(dBuf), "%s%.2f", (_currentDelta >= 0 ? "+" : ""),
             _currentDelta);
    tft->drawString(dBuf, SCREEN_WIDTH / 2, footerY + 32);

    // Mini Live Timer
    unsigned long currentLap = millis() - _currentLapStart;
    int ms = (currentLap % 1000);
    int s = (currentLap / 1000) % 60;
    int m = (currentLap / 60000);
    char tBuf[16];
    sprintf(tBuf, "%02d:%02d.%01d", m, s, ms / 100);
    tft->setTextSize(1);
    tft->setTextFont(2);
    tft->setTextPadding(SCREEN_WIDTH - 60);
    tft->drawString(tBuf, SCREEN_WIDTH / 2, footerY + 68);
  } else {
    // Idle Predictive Area - CLEAR AREA FIRST to prevent duplicates
    tft->fillRect(10, footerY + 10, SCREEN_WIDTH - 20, 60, TFT_BLACK);

    tft->setTextColor(COLOR_SECONDARY, TFT_BLACK);
    tft->setTextFont(2);
    tft->setTextSize(1);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("WAITING FOR LAP 2...", SCREEN_WIDTH / 2, footerY + 40);
  }

  // --- FINAL CLEANUP (Prevent font leaks) ---
  tft->setTextPadding(0);
  tft->setTextSize(1);
  tft->setTextFont(1);
  tft->setFreeFont(NULL);
}

void LapTimerScreen::drawTrackMap(int x, int y, int w, int h) {
  TFT_eSPI *tft = _ui->getTft();

  // Logic to draw map from points
  // If no recorded points (or loaded track points), show
  // "No Map" or simple loop

  // For now, if we have _recordedPoints, we draw them
  // scaled
  if (_recordedPoints.empty()) {
    // Scaling mock track to fit 80% of the container
    int cx = x + w / 2;
    int cy = y + h / 2;

    // Scale factor: Fit '80px' nominal shape into min(w, h)
    // * 0.8
    float scale = (min(w, h) * 0.8) / 80.0;

    // Original coordinates scaled
    // (-20, -40) to (20, -40) -> Top Horizontal
    tft->drawLine(cx - 20 * scale, cy - 40 * scale, cx + 20 * scale,
                  cy - 40 * scale, TFT_WHITE);
    // (20, -40) to (30, -10) -> Top Right
    tft->drawLine(cx + 20 * scale, cy - 40 * scale, cx + 30 * scale,
                  cy - 10 * scale, TFT_WHITE);
    // (30, -10) to (10, 40) -> Bottom Right
    tft->drawLine(cx + 30 * scale, cy - 10 * scale, cx + 10 * scale,
                  cy + 40 * scale, TFT_WHITE);
    // (10, 40) to (-20, 30) -> Bottom Left
    tft->drawLine(cx + 10 * scale, cy + 40 * scale, cx - 20 * scale,
                  cy + 30 * scale, TFT_WHITE);
    // (-20, 30) to (-20, -40) -> Left Vertical
    tft->drawLine(cx - 20 * scale, cy + 30 * scale, cx - 20 * scale,
                  cy - 40 * scale, TFT_WHITE);

    // Finish line dot
    tft->fillCircle(cx - 20 * scale, cy - 40 * scale, 4 * scale, TFT_RED);
    return;
  }

  // Auto-scale logic
  double minLat = 90.0, maxLat = -90.0;
  double minLon = 180.0, maxLon = -180.0;

  // Find bounds
  // Use a sampling if too many points to be fast
  int step = 1;
  if (_recordedPoints.size() > 500)
    step = _recordedPoints.size() / 500;

  for (size_t i = 0; i < _recordedPoints.size(); i += step) {
    if (_recordedPoints[i].lat < minLat)
      minLat = _recordedPoints[i].lat;
    if (_recordedPoints[i].lat > maxLat)
      maxLat = _recordedPoints[i].lat;
    if (_recordedPoints[i].lon < minLon)
      minLon = _recordedPoints[i].lon;
    if (_recordedPoints[i].lon > maxLon)
      maxLon = _recordedPoints[i].lon;
  }

  if (minLat == maxLat || minLon == maxLon)
    return;

  // Scale factors
  // Expand bounds slightly (5%)
  double latRange = maxLat - minLat;
  double lonRange = maxLon - minLon;

  // Aspect ratio correction (basic equirectangular
  // approximation) Lat degrees are constant distance, Lon
  // degrees shrink by cos(lat)
  double avgLatRad = (minLat + maxLat) / 2.0 * DEG_TO_RAD;
  double lonScale = cos(avgLatRad);

  double aspect = (lonRange * lonScale) / latRange;

  int drawW = w - 20;
  int drawH = h - 20;
  double screenAspect = (double)drawW / drawH;

  double scaleX, scaleY;
  int offsetX = x + 10;
  int offsetY = y + 10;

  if (aspect > screenAspect) {
    // Wider than screen: fit to width
    scaleX = drawW / (lonRange * lonScale);
    scaleY = scaleX; // Keep aspect
    // Center Y
    double contentH = latRange * scaleY;
    offsetY += (drawH - contentH) / 2;
  } else {
    // Taller than screen: fit to height
    scaleY = drawH / latRange;
    scaleX = scaleY; // Keep aspect
    // Center X
    double contentW = (lonRange * lonScale) * scaleX;
    offsetX += (drawW - contentW) / 2;
  }

  // Draw points
  for (size_t i = 0; i < _recordedPoints.size() - step; i += step) {
    GPSPoint &p1 = _recordedPoints[i];
    GPSPoint &p2 = _recordedPoints[i + step];

    // Map to screen
    // Note: Y is inverted (Lat increases up, Screen Y
    // increases down)
    int x1 = offsetX + (int)((p1.lon - minLon) * lonScale * scaleX);
    int y1 = offsetY + (int)((maxLat - p1.lat) * scaleY);

    int x2 = offsetX + (int)((p2.lon - minLon) * lonScale * scaleX);
    int y2 = offsetY + (int)((maxLat - p2.lat) * scaleY);

    tft->drawLine(x1, y1, x2, y2, TFT_WHITE);
  }

  // Draw current position cursor?
  // Locate current pos
  double currLat = gpsManager.getLatitude();
  double currLon = gpsManager.getLongitude();
  int cx = offsetX + (int)((currLon - minLon) * lonScale * scaleX);
  int cy = offsetY + (int)((maxLat - currLat) * scaleY);

  // Verify bounds before drawing cursor
  if (cx >= x && cx < x + w && cy >= y && cy < y + h) {
    tft->fillCircle(cx, cy, 4, TFT_RED);
  }
}

void LapTimerScreen::checkFinishLine() {
  double dist = gpsManager.distanceBetween(gpsManager.getLatitude(),
                                           gpsManager.getLongitude(),
                                           _finishLat, _finishLon);

  // Logika Deteksi Mulai/Lap
  // Members: _finishLineInside, _lastFinishCross

  if (dist < 20) { // Radius 20m
    if (!_finishLineInside &&
        (millis() - _lastFinishCross > 10000)) { // Debounce 10s
      // Lap Baru / Mulai
      if (!_isRecording) {
        _isRecording = true;
        sessionManager.startSession();
        _lapCount = 1;
      } else {
        unsigned long lapTime = millis() - _currentLapStart;
        _lastLapTime = lapTime;
        _lapTimes.push_back(lapTime); // Tambahkan ke riwayat
        if (_bestLapTime == 0 || lapTime < _bestLapTime) {
          _bestLapTime = lapTime;
          // Reload reference lap from CURRENT session file
          if (sessionManager.isLogging()) {
            String curFile = sessionManager.getCurrentFilename();
            Serial.print("New Best Lap! Loading reference: ");
            Serial.println(curFile);
            sessionManager.loadBestLapAsReference(curFile);
          }
        }
        _lapCount++;

        // Log Lap Event
        // Format: LAP, LapNumber, LapTimeMs
        String lapLog = "LAP," + String(_lapCount) + "," + String(lapTime);
        sessionManager.logData(lapLog);

        // Reset max RPM for new lap if desired, or keep
        // session max? Usually Session Max is what you want
        // to see overall. But "RPM max" on screen might
        // imply "This Lap". Design usually shows Session or
        // Lap Max. Let's keep Session Max for now as it's
        // easier. If "This Lap", we reset here:
        // _maxRpmSession = 0;
      }
      _currentLapStart = millis();
      _lastFinishCross = millis();
      _finishLineInside = true;
      _currentLapDist = 0; // Reset Distance for Predictive

      // Final Sector (S3) for previous lap
      if (_lapCount > 1 && _lastSector == 2) {
        unsigned long sTime = millis() - _sectorStartTime;
        String log = "SECTOR," + String(_lapCount - 1) + ",3," + String(sTime);
        sessionManager.logData(log);
      }

      _lastSector = 0;
      _sectorStartTime = millis();
    }
  } else {
    if (dist > 25)
      _finishLineInside = false;

    // --- SECTOR ANALYSIS ---
    if (_isRecording && !sessionManager.referenceLap.empty()) {
      float totalLapDist = sessionManager.referenceLap.back().distance;
      if (totalLapDist > 100) { // Valid lap length
        float s1Threshold = totalLapDist * 0.333f;
        float s2Threshold = totalLapDist * 0.666f;

        if (_currentLapDist >= s1Threshold && _lastSector == 0) {
          unsigned long sTime = millis() - _sectorStartTime;
          String log = "SECTOR," + String(_lapCount) + ",1," + String(sTime);
          sessionManager.logData(log);
          _lastSector = 1;
          _sectorStartTime = millis(); // Reset for S2
          _ui->showToast("SECTOR 1", 1000);
        } else if (_currentLapDist >= s2Threshold && _lastSector == 1) {
          unsigned long sTime = millis() - _sectorStartTime;
          String log = "SECTOR," + String(_lapCount) + ",2," + String(sTime);
          sessionManager.logData(log);
          _lastSector = 2;
          _sectorStartTime = millis(); // Reset for S3
          _ui->showToast("SECTOR 2", 1000);
        }
      }
    }
  }
}

// --- TRACK CREATOR WIZARD ---
void LapTimerScreen::drawCreateTrack() {
  TFT_eSPI *tft = _ui->getTft();
  extern GPSManager gpsManager;

  // 1. Header (Standardized)
  tft->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                SCREEN_HEIGHT - STATUS_BAR_HEIGHT, TFT_BLACK);

  // Divider - REMOVED per user request
  // tft->drawFastHLine(0, 50, SCREEN_WIDTH, COLOR_SECONDARY);

  // Status Bar Separator Line (Restored)
  tft->drawFastHLine(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH, COLOR_SECONDARY);

  // Title Box
  int headW = 200;
  int headH = 35;
  int headX = (SCREEN_WIDTH - headW) / 2;
  int headY = STATUS_BAR_HEIGHT + 15; // Below Status Bar (20 + 15 = 35)
  tft->fillRoundRect(headX, headY, headW, headH, 6, 0x10A2); // Slate
  tft->drawRoundRect(headX, headY, headW, headH, 6, TFT_SILVER);

  tft->setTextDatum(MC_DATUM);
  tft->setFreeFont(&Org_01);
  tft->setTextSize(2);
  tft->setTextColor(TFT_WHITE, 0x10A2);
  tft->drawString("NEW TRACK", SCREEN_WIDTH / 2, headY + headH / 2 + 1);

  // Back Button
  tft->setTextDatum(TL_DATUM);
  tft->setFreeFont(NULL); // Reset to default
  tft->setTextSize(1);
  tft->setTextColor(TFT_WHITE, TFT_BLACK);
  tft->drawString("<", 10, 25);

  // 2. Content (Shifted down)
  int midY = 120;
  tft->setTextDatum(MC_DATUM);
  tft->setTextFont(1);
  tft->setTextColor(TFT_SILVER, TFT_BLACK);

  double lat = gpsManager.getLatitude();
  double lon = gpsManager.getLongitude();
  bool fixed = gpsManager.isFixed();

  if (_createStep == 0) {
    tft->drawString("STEP 1: START LINE", SCREEN_WIDTH / 2,
                    90); // Moved down from 50
    tft->setTextFont(2);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString("Go to Start Line", SCREEN_WIDTH / 2, midY);

    // Coordinates
    tft->setTextFont(1);
    tft->setTextColor(fixed ? TFT_GREEN : TFT_RED, TFT_BLACK);
    String coord = String(lat, 6) + ", " + String(lon, 6);
    tft->drawString(coord, SCREEN_WIDTH / 2, midY + 30);

    // Button: SET START
    int btnW = 220; // Bigger
    int btnH = 50;
    int btnX = (SCREEN_WIDTH - btnW) / 2;
    int btnY = 200; // Lower

    uint16_t btnColor = fixed ? TFT_GREEN : TFT_DARKGREY;
    tft->fillRoundRect(btnX, btnY, btnW, btnH, 6, btnColor);
    tft->setTextColor(TFT_BLACK, btnColor);
    tft->setTextFont(2);
    tft->drawString("SET START", btnX + btnW / 2, btnY + btnH / 2 + 1);

  } else if (_createStep == 1) {
    tft->drawString("STEP 2: FINISH LINE", SCREEN_WIDTH / 2,
                    90); // Moved down from 50
    tft->setTextFont(2);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString("Go to Finish Line", SCREEN_WIDTH / 2, midY);

    // Coordinates
    tft->setTextFont(1);
    tft->setTextColor(fixed ? TFT_GREEN : TFT_RED, TFT_BLACK);
    String coord = String(lat, 6) + ", " + String(lon, 6);
    tft->drawString(coord, SCREEN_WIDTH / 2, midY + 30);

    int btnW = 200; // Bigger buttons
    int btnH = 50;

    // Button 1: SAME AS START
    int btn1X = 20;
    int btnY = 200; // Lower
    tft->fillRoundRect(btn1X, btnY, btnW, btnH, 6, TFT_CYAN);
    tft->setTextColor(TFT_BLACK, TFT_CYAN);
    tft->setTextFont(2);
    tft->drawString("SAME AS START", btn1X + btnW / 2, btnY + btnH / 2 + 1);

    // Button 2: SET FINISH
    int btn2X = SCREEN_WIDTH - 20 - btnW;
    uint16_t btnColor = fixed ? TFT_GREEN : TFT_DARKGREY;
    tft->fillRoundRect(btn2X, btnY, btnW, btnH, 6, btnColor);
    tft->setTextColor(TFT_BLACK, btnColor);
    tft->drawString("SET FINISH", btn2X + btnW / 2, btnY + btnH / 2 + 1);
  } else if (_createStep == 2) {
    // Saving state
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->setTextFont(2);
    tft->drawString("SAVING TRACK...", SCREEN_WIDTH / 2, 120);
  }

  _ui->drawStatusBar();
}

void LapTimerScreen::saveNewTrack(String name, double sLat, double sLon,
                                  double fLat, double fLon) {
  if (!SD.exists("/tracks.json")) {
    File f = SD.open("/tracks.json", FILE_WRITE);
    if (f) {
      f.print("{\"tracks\":[]}");
      f.close();
    }
  }

  File file = SD.open("/tracks.json", FILE_READ);
  if (!file)
    return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    doc.clear();
    doc["tracks"].to<JsonArray>();
  }

  JsonArray tracks = doc["tracks"];
  JsonObject t = tracks.add<JsonObject>();
  t["name"] = name;
  t["lat"] = sLat;
  t["lon"] = sLon;
  // We save standard structure.

  JsonArray cfgs = t["configs"].to<JsonArray>();
  cfgs.add("Default");

  File wFile = SD.open("/tracks.json", FILE_WRITE);
  if (wFile) {
    serializeJson(doc, wFile);
    wFile.close();
  }
}

void LapTimerScreen::finalizeRaceSession() {
  _state = STATE_SUMMARY;
  if (sessionManager.isLogging()) {
    String dateStr =
        gpsManager.getDateString() + " " + gpsManager.getTimeString();
    sessionManager.appendToHistoryIndex("Track Session", dateStr, _lapCount,
                                        _bestLapTime, "TRACK");

    // PERSIST BEST LAP TO tracks.json
    if (_selectedTrackIdx >= 0 && _selectedTrackIdx < (int)_tracks.size()) {
      Track &t = _tracks[_selectedTrackIdx];
      if (_bestLapTime > 0 && (_bestLapTime < t.bestLap || t.bestLap == 0)) {
        t.bestLap = _bestLapTime;
        t.pathFile = sessionManager.getCurrentFilename();

        // Update JSON file
        File file = SD.open("/tracks.json", FILE_READ);
        if (file) {
          JsonDocument doc;
          deserializeJson(doc, file);
          file.close();

          JsonArray tracks = doc["tracks"];
          for (JsonObject track : tracks) {
            if (track["name"] == t.name) {
              track["best_lap"] = t.bestLap;
              track["path"] = t.pathFile;
              break;
            }
          }

          File wFile = SD.open("/tracks.json", FILE_WRITE);
          if (wFile) {
            serializeJson(doc, wFile);
            wFile.close();
          }
        }
      }
    }
  }
  sessionManager.stopSession();
  _isRecording = false;
  _ui->getTft()->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                          SCREEN_HEIGHT - STATUS_BAR_HEIGHT, COLOR_BG);
  drawSummary();
}

void LapTimerScreen::drawRenameTrack(bool force) {
  TFT_eSPI *tft = _ui->getTft();
  static String lastRenamingName = "";
  static bool lastShift = !_keyboardShift;

  if (force) {
    lastRenamingName = ""; // Reset history
  }

  bool fullRedraw = force || (lastRenamingName == "");

  if (fullRedraw) {
    tft->fillRect(0, STATUS_BAR_HEIGHT, SCREEN_WIDTH,
                  SCREEN_HEIGHT - STATUS_BAR_HEIGHT, TFT_BLACK);

    // Header
    tft->drawFastHLine(0, 20, SCREEN_WIDTH, COLOR_SECONDARY);
    tft->setTextDatum(TC_DATUM);
    tft->setFreeFont(&Org_01);
    tft->setTextSize(2);
    tft->setTextColor(TFT_WHITE, TFT_BLACK);
    tft->drawString("RENAME TRACK", SCREEN_WIDTH / 2, 28);
  }

  // Name Box (Partial update)
  int boxY = 60;
  int boxH = 40;
  if (fullRedraw || lastRenamingName != _renamingName) {
    tft->fillRoundRect(20, boxY, SCREEN_WIDTH - 40, boxH, 8, 0x18E3);
    tft->drawRoundRect(20, boxY, SCREEN_WIDTH - 40, boxH, 8, TFT_GOLD);

    tft->setTextColor(TFT_WHITE, 0x18E3);
    tft->setTextFont(2);
    tft->setTextSize(1); // Force standard size
    tft->setTextDatum(MC_DATUM);
    tft->drawString(_renamingName + "|", SCREEN_WIDTH / 2, boxY + boxH / 2 + 1);
    lastRenamingName = _renamingName;
  }

  // Keyboard (Partial update if shift changed)
  if (fullRedraw || lastShift != _keyboardShift) {
    _keyboard.draw(tft, 110, _keyboardShift);
    lastShift = _keyboardShift;
  }

  if (fullRedraw) {
    _ui->drawStatusBar();
  }
}

void LapTimerScreen::renameTrack(int index, String newName) {
  if (index < 0 || index >= _tracks.size() || newName.length() == 0)
    return;

  _tracks[index].name = newName;

  if (!SD.exists("/tracks.json"))
    return;

  File file = SD.open("/tracks.json", FILE_READ);
  if (!file)
    return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (!error) {
    JsonArray tracks = doc["tracks"];
    if (index < tracks.size()) {
      tracks[index]["name"] = newName;

      File wFile = SD.open("/tracks.json", FILE_WRITE);
      if (wFile) {
        serializeJson(doc, wFile);
        wFile.close();
      }
    }
  }
}
void LapTimerScreen::drawSaveTrackName(bool force) {
  TFT_eSPI *tft = _ui->getTft();

  if (force) {
    tft->fillScreen(_ui->getBackgroundColor());

    // Title
    tft->setTextFont(1);
    tft->setTextSize(1);
    tft->setTextColor(COLOR_PRIMARY, _ui->getBackgroundColor());
    tft->setTextDatum(TC_DATUM);
    tft->drawString("NAME YOUR TRACK", SCREEN_WIDTH / 2, 5);

    // Input Box
    int boxW = 300;
    int boxH = 40;
    int boxX = (SCREEN_WIDTH - boxW) / 2;
    int boxY = 40;

    tft->drawRect(boxX, boxY, boxW, boxH, COLOR_SECONDARY);
    tft->fillRect(boxX + 1, boxY + 1, boxW - 2, boxH - 2, TFT_DARKGREY);

    // Draw Buttons (SAVE / CANCEL) at Bottom
    int btnY = 275; // Moved down to avoid keyboard overlap
    int btnW = 100;
    int gap = 20;
    int startX = (SCREEN_WIDTH - (btnW * 2 + gap)) / 2;

    // CANCEL
    int cancelX = startX;
    tft->fillRect(cancelX, btnY, btnW, 40, TFT_RED);
    tft->drawRect(cancelX, btnY, btnW, 40, TFT_WHITE);
    tft->setTextColor(TFT_WHITE, TFT_RED);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(1);
    tft->drawString("CANCEL", cancelX + btnW / 2, btnY + 20);

    // SAVE
    int saveX = startX + btnW + gap;
    tft->fillRect(saveX, btnY, btnW, 40, COLOR_PRIMARY);
    tft->drawRect(saveX, btnY, btnW, 40, TFT_BLACK);
    tft->setTextColor(TFT_BLACK, COLOR_PRIMARY);
    tft->drawString("SAVE", saveX + btnW / 2, btnY + 20);
  }

  // Draw Current Text
  int boxW = 300;
  int boxX = (SCREEN_WIDTH - boxW) / 2;
  int boxY = 40;

  tft->setTextFont(1);
  tft->setTextSize(2);
  tft->setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft->setTextDatum(ML_DATUM);
  tft->setTextPadding(boxW - 10);
  tft->drawString(_renamingName, boxX + 10, boxY + 20);
  tft->setTextPadding(0);

  // Draw Keyboard (Y=90 to fit)
  _keyboard.draw(tft, 90, _keyboardShift);
}
