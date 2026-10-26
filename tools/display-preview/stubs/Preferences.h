#pragma once
#include <Arduino.h>
extern bool g_prefLand;
struct Preferences {
  bool begin(const char*, bool) { return true; }
  uint8_t getUChar(const char*, uint8_t d) { return d; }
  bool getBool(const char* k, bool d) { return !strcmp(k, "land") ? g_prefLand : d; }
  void putUChar(const char*, uint8_t) {}
  void putBool(const char*, bool) {}
};
