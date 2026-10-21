#pragma once
#include "Arduino.h"
#include <map>
#include <string>

struct FakePreferencesState
{
    std::map<std::string, std::map<std::string, std::string>> values;
    int writesBeforeFailure = 1000;
};
extern FakePreferencesState preferencesState;

class Preferences
{
public:
    bool begin(const char *name, bool readOnly)
    {
        name_ = name;
        if (readOnly && preferencesState.values.count(name_) == 0) return false;
        preferencesState.values[name_];
        return true;
    }
    bool getBool(const char *key, bool fallback)
    {
        auto &values = preferencesState.values[name_];
        return values.count(key) == 0 ? fallback : values[key] == "1";
    }
    String getString(const char *key, const char *fallback)
    {
        auto &values = preferencesState.values[name_];
        return values.count(key) == 0 ? String(fallback) : String(values[key].c_str());
    }
    size_t putBool(const char *key, bool value) { return put(key, value ? "1" : "0"); }
    size_t putString(const char *key, const char *value) { return put(key, value); }
    bool clear() { preferencesState.values[name_].clear(); return true; }
    void end() { }
private:
    std::string name_;
    size_t put(const char *key, const char *value)
    {
        if (preferencesState.writesBeforeFailure-- <= 0) return 0;
        preferencesState.values[name_][key] = value;
        return std::strlen(value);
    }
};
