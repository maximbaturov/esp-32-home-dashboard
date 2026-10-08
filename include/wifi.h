#pragma once
#include <Arduino.h>

void initAccessPoint();
bool wifiConnectDevMode();
bool wifiConnect(String ssid, String password);
bool isWifiInAccessMode();
String getIp();