#pragma once
// Host-side stand-in for the Arduino core: just enough for src/ui.cpp to compile and run on a PC.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <algorithm>
using std::min; using std::max;
#define PROGMEM
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define constrain(a,lo,hi) ((a)<(lo)?(lo):((a)>(hi)?(hi):(a)))
uint32_t millis();
inline void delay(uint32_t) {}
inline void pinMode(int,int) {}
inline void digitalWrite(int,int) {}
inline void ledcWrite(int,uint32_t) {}
inline bool ledcAttach(int,int,int) { return true; }
struct SerialStub {
  void println(const char*) {}
  void printf(const char*, ...) {}
};
extern SerialStub Serial;
struct EspStub { void restart() {} uint32_t getFreeHeap() { return 214000; } };
extern EspStub ESP;
