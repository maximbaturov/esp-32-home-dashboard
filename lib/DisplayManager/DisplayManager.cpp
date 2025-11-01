#include "DisplayManager.h"

DisplayManager::DisplayManager(uint8_t width, uint8_t height, int8_t resetPin, TwoWire* wire)
    : display(width, height, wire, resetPin) {}

bool DisplayManager::begin(uint8_t addr) {
    return display.begin(SSD1306_SWITCHCAPVCC, addr);
}

Adafruit_SSD1306& DisplayManager::getDisplay() {
    return display;
}

void DisplayManager::addMessage(const String &msg) {
  if (logCount >= MAX_LOG_LINES) {
    for (int i = 1; i < MAX_LOG_LINES; i++) {
      logLines[i - 1] = logLines[i];
    }

    logLines[MAX_LOG_LINES - 1] = msg;
  } else {
    logLines[logCount++] = msg;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  for (int i = 0; i < logCount; i++) {
    display.setCursor(0, i * 10);
    display.print(logLines[i]);
  }

  display.display();

  if (Serial) {
    Serial.println(msg);
  }
}

void DisplayManager::clear() {
    display.clearDisplay();
    display.display();
}