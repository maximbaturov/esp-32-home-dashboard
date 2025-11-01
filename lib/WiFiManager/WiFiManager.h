#pragma once
#include <WiFi.h>
#include "DisplayManager.h"

class WiFiManager {
public: 
    WiFiManager(DisplayManager& display);
    bool connect(String ssid, String password);
    bool isAccesPointMode();
    void initAccessPoint();
private:
    DisplayManager& display;
};
