#ifndef RPM_SENSOR_SCREEN_H
#define RPM_SENSOR_SCREEN_H

#include "../UIManager.h"

class RpmSensorScreen : public UserScreen {
public:
  void begin(UIManager *ui) override { _ui = ui; }
  void onShow() override;
  void onHide() override;
  void update() override;

private:
  UIManager *_ui;
  void drawScreen();

  // UI Helpers
  void updateValues();

  // Constants

  // State
  int _currentRpm;
  int _maxRpm;
  int _currentLvl;

  // Speed Tracking

  unsigned long _lastUpdate;

  // Interrupt Handling
  static volatile unsigned long _rpmPulses;
  static volatile unsigned long _lastPulseMicros;
  static void IRAM_ATTR onPulse();
  unsigned long _lastRpmCalcTime;
};

#endif
