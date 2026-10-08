#include "wifi.h"
#include <WiFi.h>

void initAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_NAME, "");

  //   IPAddress ip = WiFi.softAPIP();
  //   addLog(ip.toString());
}

bool wifiConnectDevMode() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int retries = 0;

  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    retries++;
  }

  return WiFi.status() == WL_CONNECTED;
}

bool wifiConnect(String ssid, String password) {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int retries = 0;

  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  return false;
}

bool isWifiInAccessMode() {
  return WIFI_AP == WiFi.getMode();
  ;
}

String getIp() { return WiFi.localIP().toString(); }
