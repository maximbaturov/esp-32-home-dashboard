#include "WiFiManager.h"

WiFiManager::WiFiManager(DisplayManager& displayRef)
    : display(displayRef) {}

bool WiFiManager::connect(String ssid, String password) {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int retries = 0;
  display.addMessage("Connecting to WIFI...");
  
  while (WiFi.status() != WL_CONNECTED && retries < 15) {
    delay(1000);
    display.addMessage("WIFI failed " + String(retries)); 
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    display.addMessage("WIFI connected");
    display.addMessage("Local IP:");
    display.addMessage(WiFi.localIP().toString());

    return true;
  }

  return false;
}

void WiFiManager::initAccessPoint() {
  String sid = "PopBot";

  WiFi.mode(WIFI_AP);
  WiFi.softAP(sid, "");

  IPAddress ip = WiFi.softAPIP();
  display.addMessage("Access Point started");

  char msg[64];
  snprintf(msg, sizeof(msg), "WiFi: %s", sid);
  display.addMessage(msg);

  display.addMessage("IP Address:");
  display.addMessage(ip.toString());
}

bool WiFiManager::isAccesPointMode() {
  return WiFi.getMode() == WIFI_AP;
}

bool WiFiManager::isClientMode() {
  return WiFi.getMode() == WIFI_STA;
}