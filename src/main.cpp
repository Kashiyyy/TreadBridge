/*
  TreadBridge - main application
  ESP32-C6 + NimBLE-Arduino 2.x (PlatformIO / pioarduino)

  - Web interface: scan for Bluetooth devices, pick one, connect
  - shows speed, pace, distance, incline and time live (FTMS 0x2ACD / RSC 0x2A53)
  - data available over HTTP: GET /api/data (JSON)
  - the chosen device is stored and reconnected automatically on the next start
  - Foot pod: sends speed/distance/cadence as BLE Running Speed & Cadence (0x1814) to the Garmin watch
    (ANT+ is not possible with the ESP32-C6)
*/
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_wifi.h>
#if __has_include("esp_coexist.h")
#include "esp_coexist.h"
#define HAVE_COEX 1
#endif
#include <NimBLEDevice.h>
#include <vector>
#include <algorithm>
#include <deque>
#include <map>
#include <stdarg.h>
#include "config.h"
#include "app.h"
#include "defaults.h"

struct Dev { NimBLEAddress addr; String name; int rssi; bool ftms; };
struct Live {
  float speed = -1;      // km/h
  long  dist = -1;       // m
  float incl = -999;     // %
  int   kcal = -1;
  int   hr = -1;
  int   timeS = -1;
  uint32_t lastMs = 0;
  String src = "";
};

AsyncWebServer server(80);
Preferences prefs;
std::vector<Dev> devs;
NimBLEClient* client = nullptr;
Live live;
String connName = "", connAddr = "", wifiInfo = "";
String targetAddr = "", savedName = "";      // saved device (MAC address in lower case)
volatile bool scanning = false, scanQuiet = false, autoHit = false;
volatile int pendingConnect = -1;
volatile bool pendingScan = false, pendingDisc = false, pendingForget = false;
int autoMiss = 0;                 // counter of unsuccessful automatic scans (only used to throttle log lines)
volatile int pendingFp = -1;   // -1 nothing, 0 off, 1 on
volatile int pendingWeb = -1;  // -1 nothing, 0 web server off, 1 on
volatile uint8_t webState = 0; // 0 off, 1 starting, 2 running
bool webEnabled = WEB_DEFAULT_ON;
int profile = 1;               // active person profile (1 or 2)
String fpName = FOOTPOD_NAME;  // foot pod name of this profile

// Settings that can be changed on the web page (defaults come from include/config.h)
struct Cfg {
  String wifiSsid, wifiPass, apSsid, apPass, host, fpBase;
  bool forceAp = false;
} cfg;
char profBuf[2][20];                       // profile names (UTF-8)
volatile bool rawLog = RAW_LOG;            // write raw data (hex) to the log
volatile int g_lang = DEFAULT_LANG;        // language: 0 German, 1 English (can be changed on the web page)
volatile uint32_t restartAt = 0;           // when set: restart at this time
bool autoEnabled = false;
uint32_t lastAttempt = 0, manualUntil = 0;

// Foot pod (BLE peripheral, Running Speed & Cadence)
NimBLEServer* bleServer = nullptr;
NimBLECharacteristic* rscMeas = nullptr;
bool fpEnabled = FOOTPOD_AUTOSTART;
volatile int cadenceManual = CADENCE_MANUAL;   // 0 = estimate automatically from the speed
volatile float simSpeed = -1;                  // >= 0: test speed instead of the treadmill (km/h)
double fpDist = 0;                             // total distance sent to the watch (m)
long fpLastTm = -1;                            // last treadmill distance (m)
uint32_t fpLastTick = 0;
float fpSpeedOut = 0; int fpCadOut = 0;
volatile bool fpSubscribed = false;
uint32_t fpSent = 0;

SemaphoreHandle_t mtx = nullptr;
String lastMsg = "";
std::deque<String> logBuf;                 // ring buffer for the log on the web page
uint32_t logSeq = 0;                       // last status message (shown on the page)
volatile uint8_t fpMask = FP_DEFAULT_MASK; // bit0 cadence, bit1 stride length, bit2 distance, bit3 running/walking
volatile bool pendingSaveMask = false;

// ---------------- Helper functions ----------------
void blog(const char* fmt, ...) {
  char b[360];
  va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
  if (SERIAL_LOG) Serial.println(b);
  if (!mtx) return;
  uint32_t sec = millis() / 1000;
  char line[400];
  snprintf(line, sizeof line, "%02lu:%02lu:%02lu  %s", (unsigned long)(sec / 3600), (unsigned long)((sec / 60) % 60), (unsigned long)(sec % 60), b);
  xSemaphoreTake(mtx, portMAX_DELAY);
  lastMsg = String(b);
  logBuf.push_back(String(line)); logSeq++;
  if (logBuf.size() > 120) logBuf.pop_front();
  xSemaphoreGive(mtx);
}

String hexStr(const uint8_t* d, size_t n) {
  String s; const size_t m = n > 64 ? 64 : n;
  char t[4];
  for (size_t i = 0; i < m; i++) { snprintf(t, sizeof t, "%02X", d[i]); s += t; if (i + 1 < m) s += ' '; }
  if (n > m) s += " ...";
  return s;
}

String shortUuid(const NimBLEUUID& u) {
  String s = String(u.toString().c_str()); s.toLowerCase();
  if (s.startsWith("0x")) return s.substring(2);
  if (s.length() == 36 && s.startsWith("0000") && s.endsWith("-0000-1000-8000-00805f9b34fb")) return s.substring(4, 8);
  return s;
}

String esc(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 32) o += ' ';
    else o += c;
  }
  return o;
}

String lower(String s) { s.toLowerCase(); return s; }
String pk(const char* base) { return String(base) + String(profile); }   // setting per profile
void setWifi(const String& v) { xSemaphoreTake(mtx, portMAX_DELAY); wifiInfo = v; xSemaphoreGive(mtx); }

const char* profileName(int p) { return profBuf[p == 2 ? 1 : 0]; }

void setProfName(int idx, const String& n) {            // idx 0 or 1
  strncpy(profBuf[idx], n.c_str(), sizeof profBuf[idx] - 1);
  profBuf[idx][sizeof profBuf[idx] - 1] = 0;
}

// Load settings from flash; anything missing there comes from include/config.h or defaults.h
void loadCfg() {
  cfg.wifiSsid = prefs.getString("wssid", WIFI_SSID);
  cfg.wifiPass = prefs.getString("wpass", WIFI_PASS);
  cfg.apSsid   = prefs.getString("apssid", AP_SSID);
  cfg.apPass   = prefs.getString("appass", AP_PASS);
  cfg.host     = prefs.getString("host", HOSTNAME);
  cfg.fpBase   = prefs.getString("fpbase", FOOTPOD_NAME);
  cfg.forceAp  = prefs.getBool("forceap", FORCE_AP);
  g_lang       = prefs.getUChar("lang", DEFAULT_LANG) ? 1 : 0;
  setProfName(0, prefs.getString("pn1", PROFILE_NAME_1));
  setProfName(1, prefs.getString("pn2", PROFILE_NAME_2));
}
bool isConn() { return client && client->isConnected(); }
bool hasTarget() { return targetAddr.length() > 0 || strlen(TARGET_NAME) > 0; }

void setConn(const String& name, const String& addr) {
  xSemaphoreTake(mtx, portMAX_DELAY); connName = name; connAddr = addr; xSemaphoreGive(mtx);
}

void saveTarget(const String& addr, const String& name) {
  xSemaphoreTake(mtx, portMAX_DELAY); targetAddr = lower(addr); savedName = name; xSemaphoreGive(mtx);
  prefs.putString("addr", lower(addr));
  prefs.putString("name", name);
}

void forgetTarget() {
  xSemaphoreTake(mtx, portMAX_DELAY); targetAddr = ""; savedName = ""; xSemaphoreGive(mtx);
  prefs.remove("addr"); prefs.remove("name");
  autoEnabled = false;
}

// ---------------- Decoding ----------------
void parseFTMS(const uint8_t* d, size_t n) {   // 0x2ACD Treadmill Data
  if (n < 2) return;
  uint16_t f = d[0] | (d[1] << 8);
  size_t p = 2;
  auto ok  = [&](size_t k) { return p + k <= n; };
  auto u16 = [&]() { uint16_t v = d[p] | (d[p + 1] << 8); p += 2; return v; };
  Live t;
  xSemaphoreTake(mtx, portMAX_DELAY); t = live; xSemaphoreGive(mtx);
  if (!(f & 0x0001) && ok(2)) t.speed = u16() / 100.0f;          // instantaneous speed
  if ((f & 0x0002) && ok(2)) p += 2;                              // average speed
  if ((f & 0x0004) && ok(3)) { t.dist = d[p] | (d[p + 1] << 8) | (d[p + 2] << 16); p += 3; }
  if ((f & 0x0008) && ok(4)) { int16_t v = (int16_t)u16(); t.incl = v / 10.0f; p += 2; }
  if ((f & 0x0010) && ok(4)) p += 4;                              // elevation gain
  if ((f & 0x0020) && ok(1)) p += 1;                              // Pace
  if ((f & 0x0040) && ok(1)) p += 1;                              // Avg Pace
  if ((f & 0x0080) && ok(5)) { t.kcal = u16(); p += 3; }
  if ((f & 0x0100) && ok(1)) { t.hr = d[p]; p += 1; }
  if ((f & 0x0200) && ok(1)) p += 1;                              // MET
  if ((f & 0x0400) && ok(2)) { t.timeS = u16(); }
  t.lastMs = millis(); t.src = "FTMS 0x2ACD";
  xSemaphoreTake(mtx, portMAX_DELAY); live = t; xSemaphoreGive(mtx);
}

void parseRSC(const uint8_t* d, size_t n) {    // 0x2A53 Running Speed & Cadence
  if (n < 4) return;
  uint8_t f = d[0];
  Live t;
  xSemaphoreTake(mtx, portMAX_DELAY); t = live; xSemaphoreGive(mtx);
  t.speed = (d[1] | (d[2] << 8)) / 256.0f * 3.6f;
  size_t p = 4;
  if (f & 0x01) p += 2;
  if ((f & 0x02) && n >= p + 4) {
    uint32_t v = d[p] | (d[p + 1] << 8) | (d[p + 2] << 16) | ((uint32_t)d[p + 3] << 24);
    t.dist = v / 10;
  }
  t.lastMs = millis(); t.src = "RSC 0x2A53";
  xSemaphoreTake(mtx, portMAX_DELAY); live = t; xSemaphoreGive(mtx);
}

void onNotify(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
  String u = shortUuid(c->getUUID());
  if (rawLog) blog("[%lu] %s (%u B): %s", millis() / 1000, u.c_str(), (unsigned)len, hexStr(data, len).c_str());
  if (u == "2acd") parseFTMS(data, len);
  else if (u == "2a53") parseRSC(data, len);
}

// ---------------- BLE ----------------
bool matchTarget(const Dev& d) {
  if (targetAddr.length()) return lower(String(d.addr.toString().c_str())) == targetAddr;
  if (strlen(TARGET_NAME)) return d.name.startsWith(TARGET_NAME);
  return false;
}

class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    Dev nd{d->getAddress(), String(d->getName().c_str()), d->getRSSI(),
           d->isAdvertisingService(NimBLEUUID((uint16_t)0x1826))};
    Dev merged = nd;
    xSemaphoreTake(mtx, portMAX_DELAY);
    bool found = false;
    for (auto& e : devs) {
      if (e.addr == nd.addr) {
        e.rssi = nd.rssi;
        if (nd.name.length()) e.name = nd.name;
        if (nd.ftms) e.ftms = true;
        merged = e;
        found = true; break;
      }
    }
    if (!found && devs.size() < 80) devs.push_back(nd);
    xSemaphoreGive(mtx);
    // only automatic scans connect by themselves, manual scans never do.
    // We only raise a flag here: the BLE task stops the scan and starts the connection.
    // (In NimBLE-Arduino 2.x scan->stop() does NOT call onScanEnd, so the old code left
    // "scanning" stuck on true and the automatic reconnect never happened.)
    if (scanQuiet && autoEnabled && !autoHit && !isConn() && hasTarget() && matchTarget(merged)) autoHit = true;
  }
  void onScanEnd(const NimBLEScanResults& r, int reason) override {
    scanning = false;
    lastAttempt = millis();
    if (autoHit) return;                 // saved device was seen: the BLE task starts the connection
    if (scanQuiet) {
      if ((autoMiss++ % 10) == 0) blog(TR("Gespeichertes Gerät noch nicht gefunden - suche weiter (Laufband eingeschaltet?)", "Saved device not found yet - still searching (treadmill switched on?)"));
    } else {
      blog(TR("Suche beendet: %d Geräte gefunden", "Scan finished: %d devices found"), (int)devs.size());
      int shown = 0;
      for (size_t i = 0; i < devs.size() && shown < 15; i++) {
        if (!devs[i].name.length() && !devs[i].ftms) continue;
        blog("  %s  %s  %ddBm%s", devs[i].name.length() ? devs[i].name.c_str() : TR("(ohne Name)", "(no name)"),
             devs[i].addr.toString().c_str(), devs[i].rssi, devs[i].ftms ? " [Fitness]" : "");
        shown++;
      }
    }
  }
} scanCB;

class CliCB : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient* c, int reason) override {
    blog(TR("Verbindung getrennt (Grund %d)%s", "Connection lost (reason %d)%s"), reason, autoEnabled ? TR(" - suche neu...", " - searching again...") : "");
    setConn("", "");
    lastAttempt = millis();
  }
} cliCB;

// Called from the BLE task after the automatic scan saw the saved device:
// stop the scan and queue the connection.
void handleAutoHit() {
  autoHit = false;
  NimBLEScan* sc = NimBLEDevice::getScan();
  if (sc->isScanning()) sc->stop();
  scanning = false;
  lastAttempt = millis();
  int hit = -1;
  xSemaphoreTake(mtx, portMAX_DELAY);
  for (size_t i = 0; i < devs.size(); i++) if (matchTarget(devs[i])) { hit = (int)i; break; }
  String nm = hit >= 0 ? devs[hit].name : String();
  xSemaphoreGive(mtx);
  if (hit < 0 || isConn()) return;
  autoMiss = 0;
  blog(TR("Gespeichertes Gerät gefunden: %s", "Saved device found: %s"), nm.c_str());
  pendingConnect = hit;
}

void startScan(bool quiet) {
  scanQuiet = quiet;
  lastAttempt = millis();
  autoHit = false;
  if (!quiet) {                       // manual scan: clear the list, pause automatic scanning for 60 s
    xSemaphoreTake(mtx, portMAX_DELAY); devs.clear(); xSemaphoreGive(mtx);
    manualUntil = millis() + 60000;
  }
  NimBLEScan* sc = NimBLEDevice::getScan();
  sc->setScanCallbacks(&scanCB, true);
  sc->setActiveScan(true);
  if (quiet) { sc->setInterval(AUTO_SCAN_INTERVAL); sc->setWindow(AUTO_SCAN_WINDOW); }   // automatic scan (only while the saved device is missing)
  else       { sc->setInterval(160); sc->setWindow(50); }    // manual scan: ~30 % radio time, WLAN stays usable
  scanning = sc->start(quiet ? AUTO_SCAN_MS : SCAN_MS, false);
  if (!quiet) blog(scanning ? "Scan started (%lu s)..." : "Scan could not start", SCAN_MS / 1000);
}

void doConnect(int idx) {
  Dev d;
  xSemaphoreTake(mtx, portMAX_DELAY);
  bool valid = idx >= 0 && idx < (int)devs.size();
  if (valid) d = devs[idx];
  xSemaphoreGive(mtx);
  if (!valid) { blog(TR("Ungültige Geräte-Nummer", "Invalid device number")); return; }
  NimBLEDevice::getScan()->stop(); scanning = false;
  delay(200);
  if (!client) { client = NimBLEDevice::createClient(); client->setClientCallbacks(&cliCB, false); client->setConnectTimeout(10000); }
  String addr = String(d.addr.toString().c_str());
  blog(TR("Verbinde mit %s ...", "Connecting to %s ..."), addr.c_str());
  if (!client->connect(d.addr)) { blog(TR("Verbindung fehlgeschlagen (Fehlercode %d)", "Connection failed (error code %d)"), client->getLastError()); lastAttempt = millis(); return; }
  xSemaphoreTake(mtx, portMAX_DELAY); live = Live(); xSemaphoreGive(mtx);
  if (!client->getService("1826") && !client->getService("1814")) {   // Fitness Machine or Running Speed & Cadence
    blog(TR("Kein Laufband: Gerät hat keinen Fitness-Dienst (FTMS).", "Not a treadmill: device has no fitness service (FTMS)."));
    client->disconnect();
    lastAttempt = millis();
    return;
  }
  client->updateConnParams(80, 160, 0, 500);   // treadmill sends ~1 packet/s: a 100-200 ms interval is enough
  String nm = d.name.length() ? d.name : addr;
  setConn(nm, addr);
  blog(TR("Verbunden mit %s", "Connected to %s"), nm.c_str());
  for (auto* s : client->getServices(true)) {
    blog("SERVICE %s", shortUuid(s->getUUID()).c_str());
    for (auto* c : s->getCharacteristics(true)) {
      String pr;
      if (c->canRead()) pr += "read ";
      if (c->canWrite()) pr += "write ";
      if (c->canWriteNoResponse()) pr += "writeNR ";
      if (c->canNotify()) pr += "notify ";
      if (c->canIndicate()) pr += "indicate ";
      blog("  CHAR %s  [%s]", shortUuid(c->getUUID()).c_str(), pr.c_str());
      if (c->canNotify()) c->subscribe(true, onNotify);
      else if (c->canIndicate()) c->subscribe(false, onNotify);
    }
  }
  saveTarget(addr, nm);
  autoEnabled = true;
  blog(TR("Gerät gespeichert. Warte auf Daten (Laufband starten / Tempo aendern)...", "Device saved. Waiting for data (start the treadmill / change speed) ..."));
}

// ---------------- Foot pod (RSC 0x1814) ----------------
class RscCb : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic* c, NimBLEConnInfo& info, uint16_t subValue) override {
    fpSubscribed = subValue != 0;
    blog(TR("Uhr: Foot-Pod-Daten %s", "Watch: foot pod data %s"), fpSubscribed ? TR("abonniert - sie nutzt den Sensor", "subscribed - it is using the sensor") : TR("abgemeldet", "unsubscribed"));
  }
} rscCb;

class SrvCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* sv, NimBLEConnInfo& info) override {
    blog(TR("Uhr verbunden: %s (Intervall %.1f ms)", "Watch connected: %s (interval %.1f ms)"), info.getAddress().toString().c_str(), info.getConnInterval() * 1.25f);
    // Long connection intervals: the watch only needs ~1 packet/s, which leaves radio time for WLAN
    sv->updateConnParams(info.getConnHandle(), 80, 160, 0, 500);   // 100-200 ms, timeout 5 s
  }
  void onConnParamsUpdate(NimBLEConnInfo& info) override {
    blog(TR("Uhr-Verbindung: Intervall jetzt %.1f ms", "Watch link: interval now %.1f ms"), info.getConnInterval() * 1.25f);
  }
  void onDisconnect(NimBLEServer* sv, NimBLEConnInfo& info, int reason) override {
    fpSubscribed = false;
    blog(TR("Uhr getrennt (Grund %d)", "Watch disconnected (reason %d)"), reason);
  }
} srvCb;

void setupFootpod() {
  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(&srvCb, false);
  bleServer->advertiseOnDisconnect(true);

  NimBLEService* rsc = bleServer->createService("1814");
  rscMeas = rsc->createCharacteristic("2A53", NIMBLE_PROPERTY::NOTIFY);
  rscMeas->setCallbacks(&rscCb);

  // Feature: bit0 stride length, bit1 total distance, bit2 walking/running status
  auto* feat = rsc->createCharacteristic("2A54", NIMBLE_PROPERTY::READ);
  uint8_t f[2] = {0x07, 0x00};
  feat->setValue(f, 2);

  auto* loc = rsc->createCharacteristic("2A5D", NIMBLE_PROPERTY::READ);
  uint8_t l = 1;  // Top of shoe
  loc->setValue(&l, 1);

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID("1814");
  adv->setAppearance(0x0440);  // Running Walking Sensor
  adv->enableScanResponse(true);
  adv->setName(fpName.c_str());
  adv->setMinInterval(400);   // 250 ms (unit 0.625 ms) - easier on the WLAN
  adv->setMaxInterval(480);   // 300 ms
  if (fpEnabled) adv->start();
}

void setFootpod(bool on) {
  fpEnabled = on;
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (on) { fpLastTick = millis(); if (!adv->isAdvertising()) adv->start(); blog(TR("Foot Pod an", "Foot pod on")); }
  else { adv->stop(); blog(TR("Foot Pod aus", "Foot pod off")); }
  prefs.putBool(pk("fpon").c_str(), on);
}

int estimateCadence(float kmh) {              // steps/min, rough estimate from the speed
  if (kmh < 1.0f) return 0;
  float c = kmh < 7.0f ? 90.0f + kmh * 5.0f : 150.0f + (kmh - 7.0f) * 5.0f;
  return (int)constrain(c, 0.0f, 254.0f);
}

void sendFootpod() {
  uint32_t now = millis();
  float dt = (now - fpLastTick) / 1000.0f;
  fpLastTick = now;

  Live L;
  xSemaphoreTake(mtx, portMAX_DELAY); L = live; xSemaphoreGive(mtx);
  bool fresh = L.lastMs != 0 && (now - L.lastMs) < 5000;     // is the treadmill still sending data?
  float kmh = simSpeed >= 0 ? simSpeed : ((fresh && L.speed > 0) ? L.speed : 0.0f);
  float v = kmh / 3.6f;                                       // m/s

  bool useTm = simSpeed < 0 && fresh && L.dist >= 0;          // prefer the real treadmill distance
  if (useTm) {
    if (fpLastTm >= 0 && L.dist >= fpLastTm) fpDist += (L.dist - fpLastTm);
    fpLastTm = L.dist;
  } else {
    fpLastTm = -1;
    fpDist += v * dt;
  }

  int cad = kmh < 1.0f ? 0 : (cadenceManual > 0 ? (int)cadenceManual : estimateCadence(kmh));
  const uint8_t m = fpMask;
  const bool wCad = m & 1, wStride = m & 2, wDist = m & 4, wStatus = m & 8;
  fpSpeedOut = kmh; fpCadOut = wCad ? cad : 0;

  if (!bleServer || bleServer->getConnectedCount() == 0) return;

  uint16_t speed = (uint16_t)(v * 256.0f);                                        // 1/256 m/s
  uint16_t stride = cad > 0 ? (uint16_t)(v * 120.0f / cad * 100.0f) : 0;          // 1/100 m (2 steps)
  uint32_t dist = (uint32_t)(fpDist * 10.0);                                      // 1/10 m
  // Flags: bit0 stride length, bit1 distance, bit2 status (1 = running, 0 = walking)
  uint8_t flags = (wStride ? 0x01 : 0) | (wDist ? 0x02 : 0) | ((wStatus && kmh >= 7.0f) ? 0x04 : 0);
  uint8_t d[10]; size_t n = 0;
  d[n++] = flags;
  d[n++] = (uint8_t)speed; d[n++] = (uint8_t)(speed >> 8);   // speed is always sent
  d[n++] = (uint8_t)(wCad ? cad : 0);                         // the cadence field is mandatory, 0 when switched off
  if (wStride) { d[n++] = (uint8_t)stride; d[n++] = (uint8_t)(stride >> 8); }
  if (wDist) { d[n++] = (uint8_t)dist; d[n++] = (uint8_t)(dist >> 8); d[n++] = (uint8_t)(dist >> 16); d[n++] = (uint8_t)(dist >> 24); }
  rscMeas->setValue(d, n);
  if (rscMeas->notify()) fpSent++;
}

// ---------------- Web ----------------
#include "webpage.h"

String jnum(bool ok, const String& v) { return ok ? v : String("null"); }

String argStr(AsyncWebServerRequest* req, const char* name) {
  return req->hasParam(name) ? req->getParam(name)->value() : String("");
}

static void cp(char* d, size_t n, const String& s) { strncpy(d, s.c_str(), n - 1); d[n - 1] = 0; }

// Snapshot of all values (thread-safe) for the web page and the display
void appSnapshot(UiState& u) {
  memset(&u, 0, sizeof(u));
  const uint32_t now = millis();
  u.ble = isConn() ? 2 : (scanning ? 1 : 0);
  u.scanning = scanning;
  u.autoOn = autoEnabled;
  u.fpOn = fpEnabled;
  u.fpClients = bleServer ? (int)bleServer->getConnectedCount() : 0;
  u.fpSub = fpSubscribed; u.fpSent = fpSent;
  u.fpSpeed = fpSpeedOut; u.fpCad = fpCadOut; u.fpDist = (float)fpDist;
  u.sim = simSpeed; u.cadManual = cadenceManual; u.mask = fpMask;
  u.profile = profile; u.webOn = webEnabled; u.webState = webState;

  Live L;
  xSemaphoreTake(mtx, portMAX_DELAY);
  cp(u.connName, sizeof u.connName, connName);
  cp(u.savedName, sizeof u.savedName, savedName);
  cp(u.savedAddr, sizeof u.savedAddr, targetAddr);
  cp(u.wifi, sizeof u.wifi, wifiInfo);
  cp(u.host, sizeof u.host, cfg.host);
  cp(u.msg, sizeof u.msg, lastMsg);
  L = live;
  std::vector<int> idx;
  for (size_t i = 0; i < devs.size(); i++) idx.push_back((int)i);
  std::sort(idx.begin(), idx.end(), [](int a, int b) {       // fitness devices first, then named devices, then by signal strength
    const Dev& x = devs[a]; const Dev& y = devs[b];
    if (x.ftms != y.ftms) return x.ftms;
    bool nx = x.name.length() > 0, ny = y.name.length() > 0;
    if (nx != ny) return nx;
    return x.rssi > y.rssi;
  });
  if (idx.size() > 20) idx.resize(20);
  const String cAddr = lower(connAddr);
  for (size_t k = 0; k < idx.size(); k++) {
    const Dev& d = devs[idx[k]];
    UiDev& o = u.devs[k];
    String a = lower(String(d.addr.toString().c_str()));
    cp(o.name, sizeof o.name, d.name);
    cp(o.addr, sizeof o.addr, a);
    o.rssi = d.rssi; o.ftms = d.ftms; o.idx = idx[k];
    o.saved = targetAddr.length() > 0 && a == targetAddr;
    o.connected = u.ble == 2 && a == cAddr;
  }
  u.devCount = (int)idx.size();
  xSemaphoreGive(mtx);

  u.hasSaved = u.savedAddr[0] != 0;
  u.liveFresh = L.lastMs != 0 && (now - L.lastMs) < 5000;
  u.speed = L.speed; u.dist = L.dist; u.incl = L.incl; u.timeS = L.timeS; u.kcal = L.kcal;
  u.ageMs = L.lastMs ? now - L.lastMs : 0;
  cp(u.liveSrc, sizeof u.liveSrc, L.src);
}

// Actions (from display and web): they only set flags, the Bluetooth task executes them
void actScan() { pendingScan = true; }
void actDisconnect() { pendingDisc = true; }
void actConnect(int idx) { pendingConnect = idx; }
void actForget() { pendingForget = true; }
void actAuto(bool on) {
  if (on) { autoEnabled = hasTarget(); lastAttempt = millis() - AUTO_PAUSE_MS; manualUntil = 0; }
  else autoEnabled = false;
}
void actFootpod(bool on) { pendingFp = on ? 1 : 0; }
void actMask(uint8_t m) { fpMask = m & 15; pendingSaveMask = true; }
void actCadence(int spm) { cadenceManual = constrain(spm, 0, 254); pendingSaveMask = true; }
void actWeb(bool on) { webEnabled = on; pendingWeb = on ? 1 : 0; }
void actRestart() { delay(100); ESP.restart(); }
void actSim(float kmh) { simSpeed = kmh < 0 ? -1.0f : constrain(kmh, 0.0f, 40.0f); }

static const char* tf(bool v) { return v ? "true" : "false"; }

void handleState(AsyncWebServerRequest* req) {
  static UiState u;                 // handlers run one after another in the async_tcp task
  appSnapshot(u);
  String j; j.reserve(3600);
  j += "{\"profile\":" + String(u.profile) + ",\"profName\":\"" + esc(String(profileName(u.profile))) + "\"";
  j += ",\"wifi\":\"" + esc(String(u.wifi)) + "\"";
  j += String(",\"lang\":") + String((int)g_lang) + ",\"scanning\":" + tf(u.scanning) + ",\"auto\":" + tf(u.autoOn) + ",\"raw\":" + tf(rawLog) + ",\"mask\":" + String((int)u.mask);
  j += String(",\"t\":{\"conn\":") + tf(u.ble == 2) + ",\"name\":\"" + esc(String(u.connName)) + "\",\"savedName\":\"" +
       esc(String(u.savedName[0] ? u.savedName : u.savedAddr)) + "\"}";
  j += String(",\"fp\":{\"on\":") + tf(u.fpOn) +
       ",\"clients\":" + String(u.fpClients) +
       ",\"sub\":" + tf(u.fpSub) + ",\"sent\":" + String(u.fpSent) +
       ",\"speed\":" + String(u.fpSpeed, 1) + ",\"cad\":" + String(u.fpCad) +
       ",\"dist\":" + String(u.fpDist, 1) + ",\"sim\":" + String(u.sim, 1) +
       ",\"cadMan\":" + String(u.cadManual) + ",\"name\":\"" + esc(fpName) + "\"}";
  j += ",\"live\":{\"speed\":" + String(u.speed, 2) + ",\"dist\":" + String(u.dist) +
       ",\"incl\":" + String(u.incl, 1) + ",\"time\":" + String(u.timeS) +
       ",\"age\":" + String(u.ageMs) + ",\"src\":\"" + esc(String(u.liveSrc)) + "\"}";
  j += ",\"devs\":[";
  for (int k = 0; k < u.devCount; k++) {
    const UiDev& d = u.devs[k];
    if (k) j += ',';
    j += "{\"i\":" + String(d.idx) + ",\"name\":\"" + esc(String(d.name)) + "\",\"addr\":\"" + String(d.addr) +
         "\",\"rssi\":" + String(d.rssi) + ",\"ftms\":" + tf(d.ftms) + ",\"saved\":" + tf(d.saved) +
         ",\"conn\":" + tf(d.connected) + "}";
  }
  j += "],\"msg\":\"" + esc(String(u.msg)) + "\"}";
  req->send(200, "application/json", j);
}

void handleLog(AsyncWebServerRequest* req) {
  uint32_t since = (uint32_t)argStr(req, "since").toInt();
  String j; j.reserve(4096);
  xSemaphoreTake(mtx, portMAX_DELAY);
  uint32_t first = logSeq - (uint32_t)logBuf.size();
  if (since < first || since > logSeq) since = first;     // too old, or the device was restarted
  j += "{\"lines\":[";
  for (uint32_t k = since; k < logSeq; k++) {
    if (k > since) j += ',';
    j += "\"" + esc(logBuf[k - first]) + "\"";
  }
  j += "],\"next\":" + String(logSeq) + "}";
  xSemaphoreGive(mtx);
  req->send(200, "application/json", j);
}

// ---- Settings (web page) ----
static int u8len(const String& s) {          // number of characters (not bytes) in UTF-8
  int n = 0;
  for (size_t i = 0; i < s.length(); i++) if (((uint8_t)s[i] & 0xC0) != 0x80) n++;
  return n;
}

void handleSettingsGet(AsyncWebServerRequest* req) {
  String j; j.reserve(700);
  j += "{\"profile\":" + String(profile) + ",\"profName\":\"" + esc(String(profileName(profile))) + "\"";
  j += ",\"pn1\":\"" + esc(String(profileName(1))) + "\",\"pn2\":\"" + esc(String(profileName(2))) + "\"";
  j += ",\"fpbase\":\"" + esc(cfg.fpBase) + "\",\"wssid\":\"" + esc(cfg.wifiSsid) + "\",\"hasWpass\":" + tf(cfg.wifiPass.length() > 0);
  j += ",\"host\":\"" + esc(cfg.host) + "\",\"apssid\":\"" + esc(cfg.apSsid) + "\",\"hasAppass\":" + tf(cfg.apPass.length() > 0);
  j += String(",\"forceap\":") + tf(cfg.forceAp) + ",\"lang\":" + String((int)g_lang) + "}";
  req->send(200, "application/json", j);
}

typedef std::map<String, String> KV;

// Validates and stores settings. Keys that are missing keep their current value.
// Used by the web page (POST /api/settings) and by the serial provisioning (TB+SET).
static bool applySettings(const KV& in, const char*& err, bool& needRestart) {
  auto G = [&](const char* k, const String& cur) -> String { auto it = in.find(k); return it == in.end() ? cur : it->second; };
  String pn1 = G("pn1", profileName(1)), pn2 = G("pn2", profileName(2)), fpb = G("fpbase", cfg.fpBase);
  String ws = G("wssid", cfg.wifiSsid), wp = G("wpass", ""), host = G("host", cfg.host);
  String aps = G("apssid", cfg.apSsid), app = G("appass", "");
  bool force = G("forceap", cfg.forceAp ? "1" : "0") == "1";
  const bool hasLang = in.count("lang") > 0;
  const int newLang = G("lang", "0") == "1" ? 1 : 0;
  pn1.trim(); pn2.trim(); fpb.trim(); ws.trim(); host.trim(); host.toLowerCase(); aps.trim();

  err = nullptr;
  if (u8len(pn1) < 1 || u8len(pn1) > 8 || pn1.length() > 16) err = TR("Profil 1: 1 bis 8 Zeichen", "Profile 1: 1 to 8 characters");
  else if (u8len(pn2) < 1 || u8len(pn2) > 8 || pn2.length() > 16) err = TR("Profil 2: 1 bis 8 Zeichen", "Profile 2: 1 to 8 characters");
  else if (u8len(fpb) < 1 || u8len(fpb) > 14 || fpb.length() > 20) err = TR("Foot-Pod-Name: 1 bis 14 Zeichen", "Foot pod name: 1 to 14 characters");
  else if (ws.length() > 32) err = TR("WLAN-Name: höchstens 32 Zeichen", "WLAN name: at most 32 characters");
  else if (wp.length() > 63) err = TR("WLAN-Passwort: höchstens 63 Zeichen", "WLAN password: at most 63 characters");
  else if (aps.length() < 1 || aps.length() > 32) err = TR("Eigenes WLAN: Name 1 bis 32 Zeichen", "Own WLAN: name 1 to 32 characters");
  else if (app.length() > 0 && (app.length() < 8 || app.length() > 63)) err = TR("Eigenes WLAN: Passwort mindestens 8 Zeichen", "Own WLAN: password at least 8 characters");
  else if (host.length() < 1 || host.length() > 24) err = TR("Hostname: 1 bis 24 Zeichen", "Hostname: 1 to 24 characters");
  else {
    for (size_t i = 0; i < host.length(); i++) {
      char c = host[i];
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) { err = TR("Hostname: nur a-z, 0-9 und Bindestrich", "Hostname: only a-z, 0-9 and hyphen"); break; }
    }
  }
  if (err) return false;

  needRestart = (ws != cfg.wifiSsid) || wp.length() > 0 || host != cfg.host || aps != cfg.apSsid ||
                app.length() > 0 || force != cfg.forceAp || fpb != cfg.fpBase ||
                pn1 != String(profileName(1)) || pn2 != String(profileName(2));
  if (hasLang) { g_lang = newLang; prefs.putUChar("lang", (uint8_t)newLang); }   // language applies immediately (display, web page, log)
  setProfName(0, pn1); setProfName(1, pn2);              // profile names apply immediately (display, web page)
  cfg.wifiSsid = ws; if (wp.length()) cfg.wifiPass = wp;
  cfg.apSsid = aps;  if (app.length()) cfg.apPass = app;
  cfg.host = host; cfg.fpBase = fpb; cfg.forceAp = force;
  prefs.putString("pn1", pn1); prefs.putString("pn2", pn2); prefs.putString("fpbase", fpb);
  prefs.putString("wssid", ws); if (wp.length()) prefs.putString("wpass", wp);
  prefs.putString("host", host); prefs.putString("apssid", aps); if (app.length()) prefs.putString("appass", app);
  prefs.putBool("forceap", force);
  blog(TR("Einstellungen gespeichert%s", "Settings saved%s"), needRestart ? TR(" (Neustart nötig für WLAN/Foot-Pod-Name)", " (restart needed for WLAN/foot pod name)") : "");
  return true;
}

void handleSettingsPost(AsyncWebServerRequest* req) {
  KV in;
  for (size_t i = 0; i < req->params(); i++) {
    const AsyncWebParameter* p = req->getParam(i);
    if (p->isPost()) in[p->name()] = p->value();
  }
  const char* err = nullptr;
  bool needRestart = false;
  if (!applySettings(in, err, needRestart)) {
    req->send(200, "application/json", String("{\"ok\":false,\"error\":\"") + err + "\"}");
    return;
  }
  req->send(200, "application/json", String("{\"ok\":true,\"restart\":") + tf(needRestart) + "}");
}

void handleData(AsyncWebServerRequest* req) {   // simple JSON interface to read the values
  static UiState u;
  appSnapshot(u);
  String j = String("{\"connected\":") + (u.ble == 2 ? "true" : "false") + ",\"device\":\"" + esc(String(u.connName)) + "\"";
  j += ",\"speed_kmh\":" + jnum(u.speed >= 0, String(u.speed, 2));
  j += ",\"pace_s_per_km\":" + jnum(u.speed > 0.1f, String((int)(3600.0f / (u.speed > 0.1f ? u.speed : 1) + 0.5f)));
  j += ",\"distance_m\":" + jnum(u.dist >= 0, String(u.dist));
  j += ",\"incline_pct\":" + jnum(u.incl > -900, String(u.incl, 1));
  j += ",\"energy_kcal\":" + jnum(u.kcal >= 0, String(u.kcal));
  j += ",\"elapsed_s\":" + jnum(u.timeS >= 0, String(u.timeS));
  j += ",\"age_ms\":" + jnum(u.ageMs != 0 || u.liveFresh, String(u.ageMs)) + "}";
  AsyncWebServerResponse* res = req->beginResponse(200, "application/json", j);
  res->addHeader("Access-Control-Allow-Origin", "*");
  req->send(res);
}

// ---------------- Serial provisioning (used by the web flasher) ----------------
// Line protocol over USB, one command per line:
//   TB+PING                    -> TB+PONG <firmware version>
//   TB+GET                     -> TB+CFG {json}   (passwords are never sent back)
//   TB+SET key=value&key=...   -> TB+OK {"restart":true|false} | TB+ERR <message>   (values are percent-encoded)
//   TB+RESTART                 -> restarts the device
// Keys: wssid wpass host apssid appass forceap pn1 pn2 fpbase lang
static String urlDecode(const String& s) {
  String o; o.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '+') o += ' ';
    else if (c == '%' && i + 2 < s.length() && isxdigit(s[i + 1]) && isxdigit(s[i + 2])) {
      char h[3] = {s[i + 1], s[i + 2], 0};
      o += (char)strtol(h, nullptr, 16); i += 2;
    } else o += c;
  }
  return o;
}

static KV parseKV(const String& s) {
  KV kv;
  int pos = 0;
  while (pos <= (int)s.length()) {
    int amp = s.indexOf('&', pos);
    if (amp < 0) amp = s.length();
    String pair = s.substring(pos, amp);
    int eq = pair.indexOf('=');
    if (eq > 0) kv[urlDecode(pair.substring(0, eq))] = urlDecode(pair.substring(eq + 1));
    pos = amp + 1;
  }
  return kv;
}

static void serialCommand(String line) {
  line.trim();
  if (!line.startsWith("TB+")) return;
  if (line == "TB+PING") {
    Serial.println(String("TB+PONG ") + FW_VERSION_FULL);
  } else if (line == "TB+GET") {
    String j = "{\"fw\":\"" FW_VERSION_FULL "\",\"pn1\":\"" + esc(String(profileName(1))) + "\",\"pn2\":\"" + esc(String(profileName(2))) +
               "\",\"fpbase\":\"" + esc(cfg.fpBase) + "\",\"wssid\":\"" + esc(cfg.wifiSsid) + "\",\"hasWpass\":" + tf(cfg.wifiPass.length() > 0) +
               ",\"host\":\"" + esc(cfg.host) + "\",\"apssid\":\"" + esc(cfg.apSsid) + "\",\"hasAppass\":" + tf(cfg.apPass.length() > 0) +
               ",\"forceap\":" + tf(cfg.forceAp) + ",\"lang\":" + String((int)g_lang) + "}";
    Serial.println("TB+CFG " + j);
  } else if (line.startsWith("TB+SET ")) {
    KV in = parseKV(line.substring(7));
    const char* err = nullptr;
    bool needRestart = false;
    if (applySettings(in, err, needRestart)) {
      Serial.println(String("TB+OK {\"restart\":") + tf(needRestart) + "}");
      if (needRestart) { delay(600); ESP.restart(); }
    } else {
      Serial.println(String("TB+ERR ") + err);
    }
  } else if (line == "TB+RESTART") {
    Serial.println("TB+OK {\"restart\":true}");
    delay(300); ESP.restart();
  } else {
    Serial.println("TB+ERR unknown command");
  }
}

void serialTask(void*) {
  String line;
  for (;;) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\n') { serialCommand(line); line = ""; }
      else if (c != '\r' && line.length() < 1800) line += c;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ---------------- Bluetooth task ----------------
// Runs separately from display/touch so that a long connect does not block the UI.
void bleTask(void*) {
  for (;;) {
    if (fpEnabled && millis() - fpLastTick >= 1000) sendFootpod();
    if (pendingSaveMask) {
      pendingSaveMask = false;
      prefs.putUChar(pk("fpmask").c_str(), fpMask);
      prefs.putInt(pk("cad").c_str(), cadenceManual);
    }
    if (pendingFp >= 0) { int on = pendingFp; pendingFp = -1; setFootpod(on == 1); }
    if (pendingForget) { pendingForget = false; forgetTarget(); if (isConn()) pendingDisc = true; blog(TR("Gespeichertes Gerät vergessen", "Saved device forgotten")); }
    // keep our flag in sync with the real scanner state (stop() and errors do not always call onScanEnd)
    if (scanning && !NimBLEDevice::getScan()->isScanning()) { scanning = false; lastAttempt = millis(); }
    if (autoHit) handleAutoHit();
    if (pendingScan) { pendingScan = false; startScan(false); }
    if (pendingDisc) { pendingDisc = false; autoEnabled = false; if (isConn()) client->disconnect(); }
    if (pendingConnect >= 0) { int i = pendingConnect; pendingConnect = -1; doConnect(i); }
    if (autoEnabled && hasTarget() && !isConn() && !scanning && pendingConnect < 0 && !pendingScan &&
        millis() > manualUntil && millis() - lastAttempt >= AUTO_PAUSE_MS) startScan(true);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ---------------- Web server / WLAN (own task so nothing blocks) ----------------
static bool routesAdded = false;

void setupRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(req->beginResponse(200, "text/html", (const uint8_t*)PAGE, sizeof(PAGE) - 1));   // served straight from flash without a copy
  });
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/data", HTTP_GET, handleData);
  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* req) { pendingScan = true; req->send(200, "text/plain", "ok"); });
  server.on("/api/connect", HTTP_GET, [](AsyncWebServerRequest* req) { pendingConnect = argStr(req, "i").toInt(); req->send(200, "text/plain", "ok"); });
  server.on("/api/disconnect", HTTP_GET, [](AsyncWebServerRequest* req) { pendingDisc = true; req->send(200, "text/plain", "ok"); });
  server.on("/api/auto", HTTP_GET, [](AsyncWebServerRequest* req) {
    autoEnabled = hasTarget(); lastAttempt = millis() - AUTO_PAUSE_MS; manualUntil = 0;
    req->send(200, "text/plain", "ok");
  });
  server.on("/api/forget", HTTP_GET, [](AsyncWebServerRequest* req) { pendingForget = true; req->send(200, "text/plain", "ok"); });
  server.on("/api/raw", HTTP_GET, [](AsyncWebServerRequest* req) { rawLog = argStr(req, "on") == "1"; req->send(200, "text/plain", "ok"); });
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/clearlog", HTTP_GET, [](AsyncWebServerRequest* req) {
    xSemaphoreTake(mtx, portMAX_DELAY); logBuf.clear(); xSemaphoreGive(mtx);
    req->send(200, "text/plain", "ok");
  });
  server.on("/api/settings", HTTP_GET, handleSettingsGet);
  server.on("/api/settings", HTTP_POST, handleSettingsPost);
  server.on("/api/restart", HTTP_GET, [](AsyncWebServerRequest* req) { restartAt = millis() + 800; req->send(200, "text/plain", "ok"); });
  server.on("/api/fpmask", HTTP_GET, [](AsyncWebServerRequest* req) { fpMask = constrain(argStr(req, "m").toInt(), 0, 15); pendingSaveMask = true; req->send(200, "text/plain", "ok"); });
  server.on("/api/footpod", HTTP_GET, [](AsyncWebServerRequest* req) { pendingFp = argStr(req, "on") == "1" ? 1 : 0; req->send(200, "text/plain", "ok"); });
  server.on("/api/cadence", HTTP_GET, [](AsyncWebServerRequest* req) { cadenceManual = constrain(argStr(req, "v").toInt(), 0, 254); req->send(200, "text/plain", "ok"); });
  server.on("/api/sim", HTTP_GET, [](AsyncWebServerRequest* req) {
    float v = argStr(req, "speed").toFloat();
    simSpeed = v < 0 ? -1.0f : constrain(v, 0.0f, 40.0f);
    req->send(200, "text/plain", "ok");
  });
  server.on("/api/fpreset", HTTP_GET, [](AsyncWebServerRequest* req) { fpDist = 0; fpLastTm = -1; req->send(200, "text/plain", "ok"); });
}

void startNet() {
  webState = 1;
  setWifi("");
  WiFi.mode(WIFI_STA);
  if (WIFI_LEGACY) esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  WiFi.setHostname(cfg.host.c_str());
  const bool tryWifi = !cfg.forceAp && cfg.wifiSsid.length() > 0;
  if (tryWifi) WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
  uint32_t t0 = millis();
  while (tryWifi && WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) vTaskDelay(pdMS_TO_TICKS(250));
  if (WiFi.status() == WL_CONNECTED) {
    setWifi("WLAN " + WiFi.localIP().toString());
  } else {
    WiFi.mode(WIFI_AP);
    if (WIFI_LEGACY) esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    WiFi.softAP(cfg.apSsid.c_str(), cfg.apPass.c_str());
    setWifi("Eigenes WLAN '" + cfg.apSsid + "' " + WiFi.softAPIP().toString());
  }
  MDNS.begin(cfg.host.c_str());
  if (!routesAdded) { setupRoutes(); routesAdded = true; }
  server.begin();
  webState = 2;
  Serial.printf("[NET] Web server running: http://%s.local\n", cfg.host.c_str());
  blog(TR("Webserver läuft", "Web server running"));
}

void stopNet() {
  server.end();
  MDNS.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  setWifi("");
  webState = 0;
  Serial.println("[NET] Web server off (WLAN off)");
  blog(TR("Webserver aus", "Web server off"));
}

void netTask(void*) {
  if (webEnabled) startNet();
  for (;;) {
    if (pendingWeb >= 0) {
      int on = pendingWeb; pendingWeb = -1;
      webEnabled = (on == 1);
      prefs.putBool("web", webEnabled);
      if (webEnabled && webState == 0) startNet();
      else if (!webEnabled && webState != 0) stopNet();
    }
    if (restartAt && (int32_t)(millis() - restartAt) >= 0) { Serial.println("[SYS] Restart"); delay(50); ESP.restart(); }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.setRxBufferSize(2048);   // must be called before begin(); the web flasher sends long settings lines
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) delay(10);
  Serial.println("[SYS] TreadBridge " FW_VERSION_FULL);
  mtx = xSemaphoreCreateMutex();
  uiBegin();
  prefs.begin("bridge", false);
  loadCfg();                                   // load settings (names, WLAN ...)
  xTaskCreate(serialTask, "serial", 8192, nullptr, 1, nullptr);   // serial provisioning, runs even while the profile screen is shown

  // Profile selection: Bluetooth and the foot pod are only started afterwards
  profile = uiChooseProfile(prefs.getUChar("lastprof", 1));
  if (profile != 2) profile = 1;
  prefs.putUChar("lastprof", (uint8_t)profile);
  {  // Foot pod name = FOOTPOD_NAME + profile name, e.g. "FootPod Person 1" (keep the BLE name short, the profile name stays complete)
    String base = cfg.fpBase;
    int room = 24 - 1 - (int)strlen(profileName(profile));
    if (room < 0) room = 0;
    if ((int)base.length() > room) base = base.substring(0, room);
    fpName = base + " " + profileName(profile);
  }
  Serial.printf("[SETUP] Profile %d (%s), foot pod '%s'\n", profile, profileName(profile), fpName.c_str());
  uiSplash(profileName(profile));

  // Settings: per profile (foot pod) or shared (treadmill, web server)
  fpMask = prefs.getUChar(pk("fpmask").c_str(), FP_DEFAULT_MASK) & 0x0F;
  cadenceManual = prefs.getInt(pk("cad").c_str(), CADENCE_MANUAL);
  fpEnabled = prefs.getBool(pk("fpon").c_str(), FOOTPOD_AUTOSTART);
  targetAddr = prefs.getString("addr", "");
  savedName = prefs.getString("name", "");
  webEnabled = prefs.getBool("web", WEB_DEFAULT_ON);
  autoEnabled = AUTO_CONNECT && hasTarget();

  NimBLEDevice::init(fpName.c_str());
  {  // Own Bluetooth address per profile: the watch remembers the foot pod by this address,
     // so the watch of person 1 never connects to the foot pod of person 2.
    uint64_t mac = ESP.getEfuseMac();
    uint8_t a[6];
    for (int i = 0; i < 6; i++) a[i] = (uint8_t)((mac >> (8 * i)) & 0xFF);
    a[0] = (uint8_t)(a[0] + profile * 0x11);
    a[5] = (uint8_t)((a[5] & 0x3F) | 0xC0);               // random static address
    bool okA = NimBLEDevice::setOwnAddr(a);
    bool okT = NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    Serial.printf("[SETUP] BLE address profile %d: %02X:%02X:%02X:%02X:%02X:%02X (%s)\n", profile,
                  a[5], a[4], a[3], a[2], a[1], a[0], (okA && okT) ? "ok" : "ERROR");
  }
#ifdef HAVE_COEX
  if (COEX_PREFER_WIFI) esp_coex_preference_set(ESP_COEX_PREFER_WIFI);   // WLAN gets priority on radio conflicts
#endif
  fpLastTick = millis();
  setupFootpod();
  if (autoEnabled) {
    blog(TR("Verbinde automatisch mit gespeichertem Gerät %s", "Connecting automatically to saved device %s"), savedName.length() ? savedName.c_str() : targetAddr.c_str());
    lastAttempt = millis() - AUTO_PAUSE_MS;   // start immediately
  } else {
    blog(TR("Bereit. Auf dem Display (Seite GERÄT) das Laufband wählen.", "Ready. Choose the treadmill on the display (page DEVICE)."));
  }
  xTaskCreate(bleTask, "ble", 10240, nullptr, 1, nullptr);   // Bluetooth logic separate from display/touch
  xTaskCreate(netTask, "net", 8192, nullptr, 1, nullptr);    // WLAN + web server (on/off via display)
}

void loop() {
  uiLoop();   // Display + Touch
}
