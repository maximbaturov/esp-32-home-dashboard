#include "storage.h"

Preferences preferences;

void Storage::removeAll() {
  preferences.begin("settings", false); 
  preferences.clear();
  preferences.end();
}

String Storage::get(const char* key) {
  String value = "";
  preferences.begin("settings", true);
  value = preferences.getString(key, "");
  preferences.end();

  return value;
}

void Storage::save(const char* key, String value) {
  preferences.begin("settings", false);
  preferences.putString(key, value);
  preferences.end();
}