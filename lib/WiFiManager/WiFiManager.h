#pragma once
#include <WiFi.h>
#include "DisplayManager.h"

class WiFiManager {
public: 
    WiFiManager(DisplayManager& display);
    bool connect(String ssid, String password);
    void initAccessPoint();
private:
    DisplayManager& display;
};
