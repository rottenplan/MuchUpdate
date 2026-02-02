#include "WebServerScreen.h"
#include "../fonts/Org_01.h"

extern WiFiManager wifiManager;

void WebServerScreen::onShow() {
  _lastUpdate = 0;
  _lastTouchTime = millis();

  // Ensure WiFi is ON
  if (!wifiManager.isEnabled()) {
    wifiManager.setEnabled(true);
  }

  TFT_eSPI *tft = _ui->getTft();
  tft->fillScreen(TFT_BLACK);

  drawStatic();
  drawStatus();
}

void WebServerScreen::drawStatic() {
  TFT_eSPI *tft = _ui->getTft();

  // Standardized Back Button (Bottom-Left)
  tft->fillTriangle(10, 290, 25, 282, 25, 298, COLOR_ACCENT);

  // Instructions (Header)
  tft->setTextDatum(TC_DATUM);
  tft->setTextColor(TFT_SILVER, _ui->getBackgroundColor());
  tft->setTextFont(2);
  tft->drawString("Connect via Phone/Laptop to:", SCREEN_WIDTH / 2, 35);
}

void WebServerScreen::drawStatus() {
  TFT_eSPI *tft = _ui->getTft();

  // Card Background (Matching About Device)
  int cardX = 20;
  int cardY = 60;
  int cardW = SCREEN_WIDTH - 40;
  int cardH = 180;

  tft->fillRoundRect(cardX, cardY, cardW, cardH, 10, 0x18E3); // Charcoal
  tft->drawRoundRect(cardX, cardY, cardW, cardH, 10, TFT_DARKGREY);

  // Content Centered inside Card
  tft->setTextDatum(TC_DATUM);

  // IP Address (Big - Font 4)
  tft->setTextColor(TFT_CYAN, 0x18E3);
  tft->setTextFont(4);
  tft->drawString("192.168.4.1", SCREEN_WIDTH / 2, cardY + 20);

  // SSID
  tft->setTextFont(2);
  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->drawString("SSID:", SCREEN_WIDTH / 2, cardY + 60);
  tft->setTextColor(TFT_GREEN, 0x18E3);
  tft->drawString("MuchRacing-GPS", SCREEN_WIDTH / 2, cardY + 80);

  // PASS
  tft->setTextColor(TFT_SILVER, 0x18E3);
  tft->drawString("PASS:", SCREEN_WIDTH / 2, cardY + 110);
  tft->setTextColor(TFT_WHITE, 0x18E3);
  tft->drawString("12345678", SCREEN_WIDTH / 2, cardY + 130);

  // Footer Hint
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(TFT_DARKGREY, _ui->getBackgroundColor());
  tft->setTextFont(2);
  tft->drawString("Open IP in Browser to download data", SCREEN_WIDTH / 2, 260);

  _ui->drawStatusBar();
}

void WebServerScreen::update() {
  UIManager::TouchPoint p = _ui->getTouchPoint();

  if (p.x != -1) {
    if (millis() - _lastTouchTime < 200)
      return;
    _lastTouchTime = millis();

    // Standardized Back Button Touch Area (Bottom-Left)
    if (p.x < 120 && p.y > 260) {
      _ui->switchScreen(SCREEN_SETTINGS); // Return to Settings
      return;
    }
  }

  // Refresh Status periodically
  if (millis() - _lastUpdate > 1000) {
    _ui->drawStatusBar();
    _lastUpdate = millis();
  }
}
