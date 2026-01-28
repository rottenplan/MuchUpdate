#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "../config.h"
#include <TFT_eSPI.h>

// Forward declaration
class UIManager;

enum ScreenType {
  SCREEN_SPLASH,
  SCREEN_SETUP,
  SCREEN_MENU,
  SCREEN_LAP_TIMER,
  SCREEN_DRAG_METER,
  SCREEN_HISTORY,
  SCREEN_SETTINGS,
  SCREEN_TIME_SETTINGS,
  SCREEN_AUTO_OFF,
  SCREEN_RPM_SENSOR,
  SCREEN_SPEEDOMETER,
  SCREEN_GPS_STATUS,
  SCREEN_SYNCHRONIZE,
  SCREEN_GNSS_LOG,
  SCREEN_WEB_SERVER
};

// ...

class UserScreen {
public:
  virtual void begin(UIManager *ui) = 0;
  virtual void onShow() = 0;
  virtual void onHide() {}
  virtual void update() = 0;
};

// Include the Touch class declaration (using forward decl if possible to avoid
// circular dep)
#include "TAMC_GT911.h"

// ... existing code ...

class UIManager {
public:
  UIManager(TFT_eSPI *tft);
  void begin();
  void update();
  void switchScreen(ScreenType type);
  void drawStatusBar(bool force = false); // Added force parameter
  void setTitle(String title) { _screenTitle = title; }
  void setAutoOff(unsigned long ms);
  void setBrightness(int level);
  void wakeUp();
  void checkSleep();
  void updateInteraction();

  // UI Helper
  void showToast(String message, int duration = 2000);
  void drawCarbonBackground(int x, int y, int w, int h);

  // Touch Handling
  void setTouch(TAMC_GT911 *touch) { _touch = touch; }
  TAMC_GT911 *getTouch() { return _touch; }

  struct TouchPoint {
    int x;
    int y;
  };
  TouchPoint getTouchPoint();

  TFT_eSPI *getTft() { return _tft; }

  // Theme Support
  void setDarkMode(bool enable);
  bool isDarkMode() { return _isDarkMode; }
  uint16_t getBackgroundColor();
  uint16_t getTextColor();
  uint16_t getSecondaryColor();

  // Debug Touch
  void setDebugTouch(bool enable) { _debugTouchEnabled = enable; }
  bool isDebugTouchEnabled() { return _debugTouchEnabled; }

private:
  TFT_eSPI *_tft;
  TAMC_GT911 *_touch; // Added touch pointer
  UserScreen *_currentScreen;
  ScreenType _currentType;
  bool _isDarkMode;
  // ... existing code ...

  // Screens
  UserScreen *_splashScreen;
  UserScreen *_setupScreen;
  UserScreen *_menuScreen;
  UserScreen *_lapTimerScreen;
  UserScreen *_dragMeterScreen;
  UserScreen *_historyScreen;
  UserScreen *_settingsScreen;
  UserScreen *_timeSettingScreen;
  // UserScreen *_autoOffScreen;
  UserScreen *_rpmSensorScreen;
  UserScreen *_speedometerScreen;
  UserScreen *_gpsStatusScreen;
  UserScreen *_synchronizeScreen;
  UserScreen *_gnssLogScreen;
  UserScreen *_webServerScreen;

  String _screenTitle; // Added title

  // Status Bar State Trackers
  String _lastTimeStr;
  double _lastHdop;
  bool _lastFix;
  int _lastSignalStrength;
  int _lastBat;
  bool _lastLogging;
  int _lastWifiStatus; // -1 = unknown, 0 = disconnected, 1 = connected

  // Sleep / Auto-Off
  unsigned long _lastInteractionTime;
  unsigned long _autoOffMs;
  bool _isScreenOff;
  int _currentBrightness; // To restore after sleep

  // Wakeup
  unsigned long _lastTapTime;
  bool _wasTouched;
  unsigned long _lastTouchProcessedTime; // Added for debounce

  // Debug
  bool _debugTouchEnabled;
};

#endif
