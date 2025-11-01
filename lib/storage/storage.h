#include <Preferences.h>

class Storage {
    public: 
        void removeAll();
        void save(const char* key, String value);
        String get(const char* key);
};
