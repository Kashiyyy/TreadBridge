#pragma once
// ============================================================================
//  TreadBridge - build-time configuration
//
//  Copy this file to  include/config.h  and adjust it.  (config.h is ignored by
//  git so your WLAN password never ends up in the repository.)
//
//  Almost everything here can ALSO be changed later without recompiling:
//  on the web page (Settings) or with the web flasher. Values saved that way
//  override the ones from this file.
// ============================================================================

// ---- WLAN (2.4 GHz only) ---------------------------------------------------
#define WIFI_SSID   ""                  // your home WLAN; empty = start the own access point
#define WIFI_PASS   ""
#define HOSTNAME    "treadbridge"       // web page reachable at http://<HOSTNAME>.local
#define FORCE_AP    false               // true = always use the own access point instead of the home WLAN
#define AP_SSID     "TreadBridge"       // name of the own access point (fallback if the home WLAN fails)
#define AP_PASS     "treadbridge123"    // at least 8 characters

// ---- Treadmill (Bluetooth client) ------------------------------------------
#define AUTO_CONNECT   true             // reconnect to the saved treadmill automatically
#define TARGET_NAME    ""               // optional name prefix (e.g. "FS-") used when no device is saved yet
#define SCAN_MS        8000             // duration of a manual scan in ms

// ---- Foot pod (Bluetooth server for the watch) -----------------------------
#define FOOTPOD_NAME       "TreadBridge"    // shown on the watch as "<FOOTPOD_NAME> <profile name>"
#define FOOTPOD_AUTOSTART  true         // start the foot pod on power-up
#define CADENCE_MANUAL     0            // 0 = estimate cadence from the speed, otherwise a fixed value (steps/min)
#define FP_DEFAULT_MASK    0            // values sent to the watch besides speed:
                                        // bit0 cadence, bit1 stride length, bit2 distance, bit3 running/walking

// ---- Radio coexistence (WLAN and Bluetooth share one radio on the ESP32-C6) -
#define WIFI_LEGACY        true         // 802.11 b/g/n only (no WiFi 6): more stable next to Bluetooth
#define COEX_PREFER_WIFI   true         // WLAN has priority on conflicts; set false if the watch link drops

// ---- Debugging -------------------------------------------------------------
#define SERIAL_LOG  false               // also print log lines to the serial monitor
#define RAW_LOG     false               // log raw hex packets from the treadmill
