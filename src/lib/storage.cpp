#include "storage.h"

Preferences preferences;

void removeSettings() {
  preferences.begin("settings", false); 
  preferences.clear();
  preferences.end();
}

String getSetting(const char* key) {
  String value = "";
  preferences.begin("settings", true);
  value = preferences.getString(key, "");
  preferences.end();

  return value;
}

void saveSetting(const char* key, String value) {
  preferences.begin("settings", false);
  preferences.putString(key, value);
  preferences.end();
}