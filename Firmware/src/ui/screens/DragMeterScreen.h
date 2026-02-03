#ifndef DRAG_METER_SCREEN_H
#define DRAG_METER_SCREEN_H

#include "../UIManager.h"
#include <Arduino.h>
#include <vector>

class DragMeterScreen : public UserScreen {
public:
  void begin(UIManager *ui) override { _ui = ui; }
  void onShow() override;
  void update() override;

private:
  UIManager *_ui;
  enum State {
    STATE_MENU,
    STATE_DRAG_MODE_MENU,
    STATE_PREDICTIVE_MENU,
    STATE_RUNNING,
    STATE_SUMMARY_VIEW
  };
  State _state;

  struct Discipline {
    String name;
    bool isDistance;          // true=distance (m), false=speed (km/h)
    float startSpeed;         // Start speed
    float target;             // meters or km/h (End target)
    unsigned long resultTime; // ms
    bool completed;
    float endSpeed;        // speed at completion
    float slope;           // %
    float peakSpeed;       // kph
    float brakingDistance; // meters
    bool valid;            // slope check
  };

  std::vector<Discipline> _disciplines;
  std::vector<Discipline> _sessionBest;
  bool _summaryShowBest;

  float _brakingStartDistance;
  bool _brakingMeasurable;
  float _totalRunDistance;

  unsigned long _startTime;
  unsigned long _lastUpdate;
  int _selectedBtn; // -1:None, 0:Back, 1:Reset

  // Display Data
  float _currentSpeed;
  float _slope;
  String _highlightTitle;
  String _highlightValue;

  // Menu
  // Menu
  std::vector<String> _menuItems;
  std::vector<String> _dragModeItems;
  std::vector<String> _predictiveItems;
  int _selectedMenuIdx;
  int _selectedDragModeIdx;
  int _selectedPredictiveIdx;
  int _lastTapIdx;
  unsigned long _lastTapTime;
  void drawMenu();
  void drawDragModeMenu();
  void drawPredictiveMenu();
  void drawSummary();
  void handleMenuTouch(int idx);
  void handleDragModeTouch(int idx);
  void handlePredictiveTouch(int idx);

  // Logic
  // Logic
  void checkStartCondition();
  void checkStopCondition();
  void updateDisciplines();
  void loadDisciplines(int modeIdx);

  // Advanced Run Logic
  enum RunState { RUN_WAITING, RUN_COUNTDOWN, RUN_RUNNING, RUN_FINISHED };
  RunState _runState;

  bool _rolloutEnabled;
  bool _oneFootReached;
  unsigned long _treeInterval;
  unsigned long _reactionTime;
  unsigned long _runStartTime;
  float _startPosition; // To track distance for rollout

  // Geometric Tracking
  double _startLat = 0.0;
  double _startLon = 0.0;
  double _startAlt = 0.0;

  void startChristmasTree();
  void drawChristmasTreeOverlay();

  // Predictive Mode
  enum DisplayMode { DISPLAY_NORMAL, DISPLAY_PREDICTIVE };
  DisplayMode _displayMode;

  float _targetTime;
  float _referenceTime; // Best run time
  float _predictedFinalTime;

  void calculatePrediction();
  void saveReferenceRun();
  void loadReferenceRun();

  // Drawing
  void drawPredictiveMode();
  void drawDashboardStatic(bool forceStatusBar = true); // Simplified routing
  void drawDashboardDynamic(); // Dipanggil di loop update
  void drawResults();          // Summary
};

#endif
