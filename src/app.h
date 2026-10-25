#pragma once
#include <Arduino.h>

// Schnittstelle zwischen Anwendung (Bluetooth, Foot Pod, Web) und Display-Oberflaeche.

struct UiDev {
  char name[22];
  char addr[18];
  int  rssi;
  bool ftms;
  bool saved;
  bool connected;
  int  idx;            // Index in der Geraeteliste der Anwendung (fuer actConnect)
};

struct UiState {
  uint8_t ble;                 // Laufband: 0 getrennt, 1 sucht, 2 verbunden
  bool    scanning;            // Bluetooth-Suche laeuft (auch bei verbundenem Laufband)
  char connName[24];
  char savedName[24];
  char savedAddr[18];
  bool hasSaved, autoOn;
  char wifi[64];
  char host[24];               // Hostname (fuer die Anzeige "http://<host>.local")
  char msg[64];
  // Laufbanddaten
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
  uint8_t mask;                // bit0 Kadenz, bit1 Schrittlaenge, bit2 Distanz, bit3 Laufen/Gehen
  // Profil und Webserver
  int   profile;               // 1 oder 2
  bool  webOn;                 // Webserver gewuenscht
  uint8_t webState;            // 0 aus, 1 startet, 2 laeuft
  // Geraeteliste (sortiert: Fitness zuerst, dann mit Namen, dann Signalstaerke)
  int   devCount;
  UiDev devs[20];
};

extern volatile int g_lang;                 // Sprache: 0 Deutsch, 1 English (auf der Webseite einstellbar)
#define TR(de, en) (g_lang ? (en) : (de))

void appSnapshot(UiState& s);
const char* profileName(int p);   // Profilname (1 oder 2), auf der Webseite aenderbar
void actScan();
void actDisconnect();
void actConnect(int idx);
void actForget();                // vergisst das gespeicherte Laufband
void actAuto(bool on);
void actFootpod(bool on);
void actMask(uint8_t m);
void actCadence(int spm);      // 0 = automatisch
void actSim(float kmh);        // < 0 = aus
void actWeb(bool on);          // Webserver (und WLAN) ein/aus
void actRestart();             // Neustart (Profilwechsel)

void uiBegin();
void uiSplash(const char* line);
void uiLoop();
int  uiChooseProfile(int lastProfile);   // blockiert, bis am Display ein Profil gewaehlt wurde (1 oder 2)
