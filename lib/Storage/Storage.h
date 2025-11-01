#pragma once
#include <Preferences.h>

class Storage {
public: 
    Storage(const char* ns = "settings");
    void removeAll();
    void save(const char* key, String value);
    String get(const char* key);

private:
    Preferences preferences;
    const char* nsName;
};
