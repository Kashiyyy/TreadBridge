// Renders the real src/ui.cpp into PPM files on a PC (no hardware needed). See README.md in this folder.
#define PROGMEM
#include "glcdfont.c"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <SPI.h>

uint32_t g_ms = 100000;
uint32_t millis() { return g_ms; }
SerialStub Serial; EspStub ESP; SpiStub SPI;
bool g_prefLand = false;
volatile int g_lang = 1;                       // English
bool g_captureProfile = false;

#include "../../src/ui.cpp"   // same translation unit: gives access to the internals

// ---- fake application ----
static UiState demo;
const char* profileName(int p) { return p == 1 ? "Alex" : "Sam"; }
void appSnapshot(UiState& s) { s = demo; }
void actScan() {} void actDisconnect() {} void actConnect(int) {} void actForget() {} void actAuto(bool) {}
void actFootpod(bool) {} void actMask(uint8_t) {} void actCadence(int) {} void actSim(float) {} void actWeb(bool) {} void actRestart() {}
bool touchBegin() { return true; }
static void dump(const char* name);
bool touchPoll(int&, int&) { if (g_captureProfile) throw 1; return false; }

static void dump(const char* name) {
  FILE* f = fopen(name, "wb");
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  for (int i = 0; i < W * H; i++) {
    uint16_t c = gfx->fb[i];
    uint8_t r = (c >> 8) & 0xF8, g = (c >> 3) & 0xFC, b = (c << 3) & 0xF8;
    r |= r >> 5; g |= g >> 6; b |= b >> 5;
    fputc(r, f); fputc(g, f); fputc(b, f);
  }
  fclose(f);
}

static void fill() {
  memset(&demo, 0, sizeof demo);
  demo.ble = 2; demo.scanning = false;
  strcpy(demo.connName, "FS-B0B1AC"); strcpy(demo.savedName, "FS-B0B1AC"); strcpy(demo.savedAddr, "c8:5a:cf:b0:b1:ac");
  demo.hasSaved = true; demo.autoOn = true;
  strcpy(demo.wifi, "WLAN 192.168.1.42"); strcpy(demo.host, "treadbridge");
  strcpy(demo.msg, "Connected to FS-B0B1AC");
  demo.liveFresh = true; demo.speed = 10.5f; demo.dist = 5230; demo.incl = 1.5f; demo.timeS = 1847; demo.ageMs = 400; strcpy(demo.liveSrc, "FTMS");
  demo.fpOn = true; demo.fpSub = true; demo.fpClients = 1; demo.fpSent = 1843; demo.fpSpeed = 10.5f; demo.fpDist = 5230; demo.sim = -1; demo.fpCad = 165; demo.cadManual = 0; demo.mask = 5;
  demo.profile = 1; demo.webOn = true; demo.webState = 2;
  demo.devCount = 4;
  auto dev = [&](int i, const char* n, const char* a, int rssi, bool ftms, bool saved, bool conn) {
    strcpy(demo.devs[i].name, n); strcpy(demo.devs[i].addr, a); demo.devs[i].rssi = rssi; demo.devs[i].ftms = ftms;
    demo.devs[i].saved = saved; demo.devs[i].connected = conn; demo.devs[i].idx = i; };
  dev(0, "FS-B0B1AC", "c8:5a:cf:b0:b1:ac", -52, true, true, true);
  dev(1, "Forerunner 970", "d4:11:22:33:44:55", -61, false, false, false);
  dev(2, "Heart Rate 4F2A", "e1:22:33:44:4f:2a", -70, false, false, false);
  dev(3, "", "7a:11:90:ee:21:05", -83, false, false, false);
}

int main() {
  fill();
  const char* names[5] = {"profile", "live", "device", "foot", "sys"};
  for (int land = 0; land < 2; land++) {
    g_prefLand = land;
    uiBegin();                                   // real start-up code (colours, orientation)
    // profile selection: uiChooseProfile draws its screen, then polls the touch -> we capture there
    g_captureProfile = true;
    try { uiChooseProfile(1); } catch (int) {}
    g_captureProfile = false;
    char fn[64]; snprintf(fn, sizeof fn, "%s_%s.ppm", land ? "land" : "port", names[0]); dump(fn);
    for (int p = 0; p < 4; p++) {
      page = p; needFull = true; appSnapshot(S); render();
      snprintf(fn, sizeof fn, "%s_%s.ppm", land ? "land" : "port", names[p + 1]); dump(fn);
    }
  }
  return 0;
}
