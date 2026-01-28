#include "KeyboardComponent.h"

// Helper to calculate key width dynamically
int getDynamicKeyWidth() {
  // Max keys in a row is 10 (1234567890 or QWERTYUIOP)
  // We want some margin on sides, say 2px gap between keys?
  // 10 keys * W = SCREEN_WIDTH - margin
  // Let's use entire width minus 10px margin
  return (SCREEN_WIDTH - 20) / 10;
}

void KeyboardComponent::draw(TFT_eSPI *tft, int startY, bool isUppercase) {
  tft->setTextFont(1);
  tft->setTextSize(1);

  int keyW = getDynamicKeyWidth();
  // Adjust height slightly larger for easier touch
  int keyH = 35;

  // QWERTY keyboard rows
  String rows[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};

  for (int row = 0; row < 4; row++) {
    String keys = rows[row];
    int numKeys = keys.length();
    int totalW = numKeys * keyW;
    int startX = (SCREEN_WIDTH - totalW) / 2;

    for (int col = 0; col < numKeys; col++) {
      int x = startX + (col * keyW);
      int ky = startY + (row * keyH);

      // Draw Key Box
      tft->drawRect(x, ky, keyW, keyH, TFT_WHITE);
      tft->setTextColor(TFT_WHITE, COLOR_BG);
      tft->setTextDatum(MC_DATUM);

      char c = keys[col];
      if (!isUppercase && c >= 'A' && c <= 'Z') {
        c = c + ('a' - 'A');
      }
      // Draw Character
      // Center in the box
      tft->drawString(String(c), x + keyW / 2, ky + keyH / 2);
    }
  }

  // Special keys (Row 4)
  int specialY = startY + (4 * keyH);

  // Distribute special keys across full width
  // Total available width ~ SCREEN_WIDTH
  // Ratios: SHIFT(1.5), DEL(1.5), SPACE(4), OK(1.5) -> Total 8.5 units?
  // Let's simplify: 4 buttons.
  // Shift (15%), Del (15%), Space (50%), Ok (20%)

  int availW = SCREEN_WIDTH - 20; // Margin
  int shiftW = availW * 0.15;
  int delW = availW * 0.15;
  int okW = availW * 0.20;
  int spaceW = availW - shiftW - delW - okW - (3 * GAP); // Remainder

  int startX = 10; // Left Margin

  int shiftX = startX;
  int delX = shiftX + shiftW + GAP;
  int spaceX = delX + delW + GAP;
  int okX = spaceX + spaceW + GAP;

  // SHIFT
  uint16_t shiftColor = isUppercase ? COLOR_HIGHLIGHT : COLOR_BG;
  uint16_t shiftTxtColor = isUppercase ? TFT_BLACK : TFT_WHITE;
  tft->fillRect(shiftX, specialY, shiftW, keyH, shiftColor);
  if (!isUppercase)
    tft->drawRect(shiftX, specialY, shiftW, keyH, TFT_WHITE);
  tft->setTextColor(shiftTxtColor, shiftColor);
  tft->setTextDatum(MC_DATUM);
  tft->drawString("SHFT", shiftX + shiftW / 2, specialY + keyH / 2);

  // DEL
  tft->drawRect(delX, specialY, delW, keyH, TFT_WHITE);
  tft->setTextColor(TFT_WHITE, COLOR_BG);
  tft->drawString("DEL", delX + delW / 2, specialY + keyH / 2);

  // SPACE
  tft->drawRect(spaceX, specialY, spaceW, keyH, TFT_WHITE);
  tft->drawString("SPACE", spaceX + spaceW / 2, specialY + keyH / 2);

  // OK
  tft->fillRect(okX, specialY, okW, keyH, COLOR_PRIMARY);
  tft->setTextColor(TFT_BLACK, COLOR_PRIMARY);
  tft->drawString("OK", okX + okW / 2, specialY + keyH / 2);
}

KeyboardComponent::KeyResult KeyboardComponent::handleTouch(int x, int y,
                                                            int startY) {
  KeyResult result = {KEY_NONE, 0};

  int keyW = getDynamicKeyWidth();
  int keyH = 35; // Match draw height

  int keyY = y - startY;
  if (keyY < 0)
    return result;

  int row = keyY / keyH;
  String rows[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};

  if (row >= 0 && row < 4) {
    String keys = rows[row];
    int numKeys = keys.length();
    int totalW = numKeys * keyW;
    int startX = (SCREEN_WIDTH - totalW) / 2;

    // Check X bounds for row
    if (x < startX || x > startX + totalW)
      return result;

    int col = (x - startX) / keyW;

    if (col >= 0 && col < numKeys) {
      result.type = KEY_CHAR;
      result.value = keys[col];
    }
  } else if (row == 4) {
    // Recalculate special key dimensions locally
    int availW = SCREEN_WIDTH - 20;
    int shiftW = availW * 0.15;
    int delW = availW * 0.15;
    int okW = availW * 0.20;
    int spaceW = availW - shiftW - delW - okW - (3 * GAP);

    int startX = 10;
    int shiftX = startX;
    int delX = shiftX + shiftW + GAP;
    int spaceX = delX + delW + GAP;
    int okX = spaceX + spaceW + GAP;

    if (x >= shiftX && x < shiftX + shiftW) {
      result.type = KEY_SHIFT;
    } else if (x >= delX && x < delX + delW) {
      result.type = KEY_DEL;
    } else if (x >= spaceX && x < spaceX + spaceW) {
      result.type = KEY_SPACE;
    } else if (x >= okX && x < okX + okW) {
      result.type = KEY_OK;
    }
  }

  return result;
}
