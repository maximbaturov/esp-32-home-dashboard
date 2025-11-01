#pragma once
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#define MAX_LOG_LINES 6

class DisplayManager {
public:
  DisplayManager(uint8_t width, uint8_t height, int8_t resetPin, TwoWire *wire = &Wire);

  Adafruit_SSD1306 &getDisplay();

  bool begin(uint8_t addr = 0x3C);
  void addMessage(const String &msg);
  void clear();

private:
  String logLines[MAX_LOG_LINES];
  int logCount = 0;
  Adafruit_SSD1306 display;
};