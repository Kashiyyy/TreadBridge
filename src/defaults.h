#pragma once
// Vorgabewerte fuer Einstellungen, die in include/config.h ueberschrieben werden koennen, z. B.
//   #define PROFILE_NAME_1 "Anna"
// Fehlt ein Eintrag in config.h, gilt der Wert hier. So bricht eine aeltere config.h den Build nicht.
#ifndef PROFILE_NAME_1
#define PROFILE_NAME_1 "Person 1"      // max. 8 Zeichen (Anzeige am Display)
#endif
#ifndef PROFILE_NAME_2
#define PROFILE_NAME_2 "Person 2"
#endif
#ifndef WEB_DEFAULT_ON
#define WEB_DEFAULT_ON true             // Webserver beim allerersten Start an
#endif
#ifndef PROFILE_AUTOSTART_S
#define PROFILE_AUTOSTART_S 0           // >0: nach so vielen Sekunden das zuletzt benutzte Profil automatisch waehlen (0 = immer warten)
#endif


// Automatisches Wiederverbinden (gespeichertes Laufband/Pulsgeraet): Suchlauf-Dauer, Pause dazwischen, Funkanteil
#ifndef AUTO_SCAN_MS
#define AUTO_SCAN_MS 4000            // Dauer eines automatischen Suchlaufs
#endif
#ifndef AUTO_PAUSE_MS
#define AUTO_PAUSE_MS 3000           // Pause zwischen zwei Suchlaeufen
#endif
#ifndef AUTO_SCAN_INTERVAL
#define AUTO_SCAN_INTERVAL 120       // ms; Interval/Window = ca. 50 % Funkzeit (nur solange etwas fehlt)
#endif
#ifndef AUTO_SCAN_WINDOW
#define AUTO_SCAN_WINDOW 60
#endif

// Sprache beim ersten Start (0 Deutsch, 1 English). Spaeter auf der Webseite unter Einstellungen aenderbar.
#ifndef DEFAULT_LANG
#define DEFAULT_LANG 0
#endif
