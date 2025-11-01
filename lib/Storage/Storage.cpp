#include "Storage.h"

Storage::Storage(const char* ns) : nsName(ns) {}

void Storage::removeAll() {
  preferences.begin(nsName, false); 
  preferences.clear();
  preferences.end();
}

String Storage::get(const char* key) {
  String value = "";
  preferences.begin(nsName, true);
  value = preferences.getString(key, "");
  preferences.end();

  return value;
}

void Storage::save(const char* key, String value) {
  preferences.begin(nsName, false);
  preferences.putString(key, value);
  preferences.end();
}