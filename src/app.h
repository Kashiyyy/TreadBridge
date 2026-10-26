#pragma once
#include <Arduino.h>

// Interface between the application (Bluetooth, foot pod, web) and the display UI.

struct UiDev {
  char name[22];
  char addr[18];
  int  rssi;
  bool ftms;
  bool saved;
  bool connected;
  int  idx;            // index in the application's device list (for actConnect)
};

struct UiState {
  uint8_t ble;                 // treadmill: 0 disconnected, 1 searching, 2 connected
  bool    scanning;            // Bluetooth scan running (also while the treadmill is connected)
  char connName[24];
  char savedName[24];
  char savedAddr[18];
  bool hasSaved, autoOn;
  char wifi[64];
  char host[24];               // hostname (for showing "http://<host>.local")
  char msg[64];
  // treadmill data
  bool  liveFresh;
  float speed; long dist; float incl; int hr, timeS, kcal;
  uint32_t ageMs;
  char liveSrc[16];
  // Foot Pod
  bool  fpOn, fpSub;
  int   fpClients;
  uint32_t fpSent;
  float fpSpeed, fpDist, sim;
  int   fpCad, cadManual;
  uint8_t mask;                // bit0 cadence, bit1 stride length, bit2 distance, bit3 running/walking
  // profile and web server
  int   profile;               // 1 or 2
  bool  webOn;                 // web server requested
  uint8_t webState;            // 0 off, 1 starting, 2 running
  // device list (sorted: fitness first, then named, then signal strength)
  int   devCount;
  UiDev devs[20];
};

extern volatile int g_lang;                 // language: 0 German, 1 English (can be changed on the web page)
#define TR(de, en) (g_lang ? (en) : (de))

void appSnapshot(UiState& s);
const char* profileName(int p);   // profile name (1 or 2), can be changed on the web page
void actScan();
void actDisconnect();
void actConnect(int idx);
void actForget();                // forget the saved treadmill
void actAuto(bool on);
void actFootpod(bool on);
void actMask(uint8_t m);
void actCadence(int spm);      // 0 = automatic
void actSim(float kmh);        // < 0 = off
void actWeb(bool on);          // web server (and WLAN) on/off
void actRestart();             // restart (profile switch)

void uiBegin();
void uiSplash(const char* line);
void uiLoop();
int  uiChooseProfile(int lastProfile);   // blocks until a profile has been chosen on the display (1 or 2)
