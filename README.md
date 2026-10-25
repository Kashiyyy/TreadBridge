# Laufband-Bridge – Phase 1 (PlatformIO)

ESP32-C6 liest per Bluetooth die Daten vom Fitifito FT700, zeigt sie auf dem **Touch-Display** (Waveshare ESP32-C6-Touch-LCD-1.47) und auf einer **Webseite** an und sendet sie als Foot Pod an die Garmin-Uhr. Auf beiden wählst du das Laufband aus und siehst Tempo, Pace, Distanz, Steigung und Zeit live. Der Serial Monitor wird nicht mehr gebraucht. Display und Webseite gibt es auf Deutsch und Englisch.

## Einrichten (PlatformIO IDE in VS Code)

1. VS Code installieren, Erweiterung **PlatformIO IDE** installieren und VS Code neu starten.
2. **File → Open Folder…** und den Ordner `laufband_bridge_pio` öffnen (der Ordner mit `platformio.ini`, nicht ein übergeordneter).
3. Warten, bis unten in der Statusleiste die PlatformIO-Symbole erscheinen. Beim ersten Mal lädt PlatformIO Plattform, Arduino-Core und Bibliotheken (mehrere hundert MB, dauert einige Minuten).
4. `include/config.h` öffnen: `WIFI_SSID` und `WIFI_PASS` eintragen (nur 2,4 GHz).
5. Waveshare-Board per USB-C anschließen . Unten **Build** (Haken), dann **Upload** (Pfeil).

Per Konsole: `pio run -t upload`.

**Falls es nicht klappt:**
- *Plattform wird nicht gefunden / Fehler beim Installieren:* PlatformIO Core aktualisieren (PlatformIO Home → Settings oder `pip install -U platformio`) und VS Code neu starten.
- *Kein Port gefunden:* anderes USB-Kabel (Datenkabel!) probieren. Upload erzwingen: BOOT-Taste halten, kurz RESET drücken, BOOT loslassen.
- *Serial Monitor bleibt leer:* am ESP32-C6 den Port mit der Bezeichnung **USB** (nicht UART) verwenden, dann einmal RESET drücken.
- Bei jedem anderen Fehler: den Text aus dem Terminal-Fenster kopieren und mir schicken.

## Start: Profil wählen

Nach dem Einschalten zeigt das Display **„Wer laeuft:“** mit den beiden Namen. Tippe deinen Namen an. Erst danach starten Bluetooth und Foot Pod.

- Jedes Profil sendet einen **eigenen Foot Pod** mit dem Namen `FootPod` + Profilname, z. B. **„FootPod Person 1“** (Teil `FootPod` änderbar in `config.h` bei `FOOTPOD_NAME`), mit **eigener Bluetooth-Adresse**. Die Uhr merkt sich den Sensor über diese Adresse. Koppelt Person 1 ihre Uhr mit „FootPod 1“ und Person 2 ihre mit „FootPod 2“, verbindet sich beim jeweils anderen Profil keine falsche Uhr.
- Pro Profil gespeichert: Foot Pod an/aus, welche Werte an die Uhr gehen, Kadenz.
- Gemeinsam gespeichert: das Laufband (Bluetooth-Gerät), Webserver an/aus, Helligkeit.
- Profil wechseln: Seite **SYS** → **PROFIL WECHSELN** (zweimal tippen, danach startet das Gerät neu).
- Namen ändern: in `include/config.h` bei `PROFILE_NAME_1` und `PROFILE_NAME_2` (max. 8 Zeichen). Mit `PROFILE_AUTOSTART_S` (z. B. 10) wird nach 10 Sekunden automatisch das zuletzt benutzte Profil gewählt (Standard: 0 = immer warten).
- **Wichtig bei der ersten Kopplung:** Koppelt immer nur eine Person nacheinander, solange ihr Profil aktiv ist, damit nicht beide Uhren denselben Sensor erwischen.

## Bedienung am Display (Waveshare ESP32-C6-Touch-LCD-1.47)

Das 1,47"-Display (172 x 320, Hochformat, USB-Buchse unten) hat vier Seiten. Die Leiste unten zeigt, auf welcher Seite du bist. **Gewechselt wird durch Wischen nach links oder rechts** (nach links = nächste Seite, nach rechts = vorherige; nach SYS kommt wieder LIVE). Die Leiste ist nur noch eine Anzeige (Tippen funktioniert zusätzlich). Auf den anderen Seiten zeigt die Kopfzeile den Status von Laufband und Uhr und rechts das aktive Profil („P1“/„P2“).

| Seite | Inhalt |
|---|---|
| **LIVE** | Oben zwei Anzeigen **LAUFBAND** und **UHR** (grün = verbunden bzw. sendet, gelb = sucht/wartet, rot = getrennt, grau = Foot Pod aus). Darunter das **Tempo in großen Ziffern** mit „km/h“ daneben, dazu Pace, Distanz, Zeit (hh:mm:ss) und Steigung. Im Querformat stehen Zeit und Steigung gleich groß und bündig in einer Reihe |
| **GERÄT** | **SUCHEN** startet die Bluetooth-Suche, ein Gerät antippen verbindet es als Laufband (Fitnessgeräte stehen oben und sind grün), **TRENN.** trennt. Zu lange Namen werden gekürzt. Scrollen per Wischen nach oben/unten oder **AUF/AB** |
| **FOOT** | Foot Pod an/aus, welche Werte an die Uhr gehen (Kadenz, Schrittlänge, Distanz, Laufen/Gehen), Kadenz per **-/+** (auto = aus dem Tempo), Testtempo per **-/+** |
| **SYS** | Helligkeit, Auto-Verbinden, **Webserver an/aus**, **Waagrecht** (Querformat an/aus), Gerät vergessen (zweimal tippen), Profil wechseln (zweimal tippen), WLAN-Adresse und Systeminfos |

**Webserver aus:** Schaltet Webserver **und** WLAN ab. Das entlastet den Funk und kann Bluetooth/Foot Pod stabiler machen. Die Einstellung bleibt nach dem Neustart erhalten.

**Waagrechte Ansicht:** Auf der Seite SYS den Schalter **Waagrecht** umlegen. Alle Seiten haben ein eigenes Querformat-Layout, die Einstellung bleibt gespeichert. Steht das Bild auf dem Kopf, in `include/pins.h` `LANDSCAPE_ROT` von 1 auf 3 ändern.

**Touch gespiegelt oder vertauscht?** In `include/pins.h` `TOUCH_FLIP_X`, `TOUCH_FLIP_Y`, `TOUCH_SWAP_XY` umstellen (0/1).
Umlaute (ä ö ü ß) werden unterstützt, auch in den Profilnamen. Andere Sonderzeichen erscheinen als „?“.

## Projektaufbau

```
laufband_bridge_pio/
├── platformio.ini
├── include/
│   ├── config.h      WLAN, Foot-Pod- und Funk-Einstellungen
│   └── pins.h        Pins des Waveshare-Boards, Touch-Ausrichtung
└── src/
    ├── main.cpp      Bluetooth (Laufband lesen, Foot Pod senden), Webseite, Ablauf
    ├── app.h         Schnittstelle zwischen Anwendung und Display
    ├── defaults.h    Vorgabewerte (Profilnamen usw.), in config.h überschreibbar
    ├── webpage.h     Weboberfläche (HTML, CSS, JavaScript)
    ├── ui.cpp        Display-Oberfläche
    └── touch.cpp/.h  Treiber für den Touch-Controller AXS5106L
```

## Benutzen

1. Nach dem Flashen verbindet sich der ESP32 mit deinem WLAN und startet die Webseite. Öffne **http://laufband.local** im Browser (Handy/PC im selben WLAN). Funktioniert der Name nicht, nimm die IP-Adresse, die einmalig im Serial Monitor (115200 Baud) beim Start steht oder in der Geräteliste deines Routers.
   Klappt das WLAN nicht, öffnet der ESP32 ein eigenes WLAN **Laufband-Bridge** (Passwort `laufband123`), die Seite ist dann unter **http://192.168.4.1** erreichbar.
2. Laufband einschalten (Konsole aktiv) und vorher nRF Connect/FitShow-App am Handy trennen (das Laufband erlaubt nur eine Verbindung).
3. Auf der Seite **Geräte suchen** drücken, dein Laufband (`FS-…`, Markierung „Fitness“) in der Liste wählen und **Verbinden**.
4. Das gewählte Gerät wird im Flash gespeichert. Beim nächsten Start (und nach Verbindungsabbrüchen) verbindet sich der ESP32 automatisch wieder. **Vergessen** löscht das gespeicherte Gerät, **Trennen** schaltet das Auto-Verbinden bis zum nächsten Klick auf „Mit gespeichertem Gerät verbinden“ (oder bis zum Neustart) aus.

## Garmin Foot Pod (BLE Running Speed & Cadence)

Der ESP32-C6 gibt die Laufbanddaten zusätzlich als Bluetooth-Foot-Pod (Service `0x1814`) weiter, Name **Laufband FootPod**. Dadurch bekommt die Uhr Tempo und Distanz direkt vom Laufband.

- **Tempo:** vom Laufband (FTMS). Sendet das Laufband länger als 5 s nichts, geht das Tempo auf 0.
- **Distanz:** Laufband-Distanz, sonst aus dem Tempo berechnet. Wird im Foot-Pod-Paket als Gesamtdistanz gesendet.
- **Kadenz:** Das Laufband liefert keine Kadenz. Sie wird aus dem Tempo geschätzt (z. B. ca. 150 Schritte/min bei 7 km/h, ca. 165 bei 10 km/h) oder fest vorgegeben (Webseite oder `CADENCE_MANUAL`). Die Schrittlänge errechnet sich aus Tempo und Kadenz, damit beides zusammenpasst.
- **Laufen/Gehen:** ab 7 km/h wird „Laufen“ gemeldet, darunter „Gehen“.

**Welche Werte gehen an die Uhr?** Auf der Webseite in der Foot-Pod-Karte unter „An die Uhr übertragen“: Die **Geschwindigkeit** wird immer gesendet, **Kadenz, Schrittlänge, Distanz** und **Laufen/Gehen** lassen sich einzeln zuschalten. Die Auswahl wird gespeichert. Standard ist nur Geschwindigkeit (`FP_DEFAULT_MASK` in `config.h`). Nicht gesendete Werte sind im Paket schlicht nicht enthalten, nur das Kadenz-Feld ist im Standard Pflicht und wird dann mit 0 gesendet.

**An der Uhr einrichten (Forerunner 970):**
1. Foot Pod auf der Webseite einschalten (Standard: an), Laufband verbinden.
2. An der Uhr unter Sensoren & Zubehör einen neuen Sensor vom Typ Foot Pod hinzufügen und **Laufband FootPod** auswählen. Die genauen Menünamen können je nach Firmware abweichen.
3. In der Laufband-Aktivität den Foot Pod für Tempo/Distanz verwenden (statt GPS/Schätzung).

**Test ohne Laufband:** Auf der Webseite ein Testtempo eingeben (z. B. 8,0 km/h). Der ESP32 sendet dann dieses Tempo an die Uhr. So kannst du prüfen, ob die Uhr den Sensor findet und nutzt.

**Wichtig:** ANT+ ist mit dem ESP32-C6 nicht möglich. Ob die Forerunner 970 einen Bluetooth-Foot-Pod als Tempoquelle akzeptiert, ist nicht belegt. Der Testmodus zeigt es schnell. Wenn nicht, bleibt ANT+ (Sensor oder ANT+-USB-Stick).

## Weboberfläche

Erreichbar unter `http://<Hostname>.local` (Standard `http://laufband.local`) oder über die IP-Adresse, die auf der Seite SYS steht. Das Aussehen entspricht dem Display. Unten wählst du die Seite:

| Seite | Inhalt |
|---|---|
| **Live** | Anzeigen für Laufband und Uhr (Foot Pod), Tempo groß, Pace, Distanz, Zeit (hh:mm:ss), Steigung |
| **Geräte** | **Geräte suchen**, dann beim Laufband **Verbinden**. Oben siehst du das angemeldete Laufband mit **Trennen** und **Vergessen**. Das gespeicherte Laufband verbindet sich beim Start und nach Verbindungsverlust (z. B. Laufband aus- und wieder eingeschaltet) automatisch, sobald es gefunden wird. Nach **Trennen** bleibt das Auto-Verbinden aus, bis du wieder verbindest |
| **Foot Pod** | Foot Pod an/aus, welche Werte an die Uhr gehen, Kadenz, Test-Tempo |
| **Log** | Lesbares Protokoll mit Farben (rot Fehler, grün Erfolg, gelb Vorgänge), Pause, Löschen, Kopieren. Mit **Rohdaten** werden zusätzlich Hex-Pakete mitgeschrieben |
| **Einstellungen** | **Sprache (Deutsch/English)**, Profilnamen, Foot-Pod-Name, WLAN, Hostname, eigenes WLAN, Neustart |

**Sprache:** Unter Einstellungen stellst du **Deutsch** oder **English** ein. Die Sprache gilt sofort für Display, Webseite und Log und bleibt gespeichert. Vorgabe beim ersten Start: `DEFAULT_LANG` in `src/defaults.h` (0 = Deutsch, 1 = English). Profilnamen und Foot-Pod-Name sind Eigennamen und werden nicht übersetzt.

**Einstellungen:** Alles, was du hier änderst, wird im Flash gespeichert und überschreibt die Werte aus `config.h`. Profilnamen und Sprache gelten sofort. WLAN, Hostname und Foot-Pod-Name gelten nach dem Neustart („Speichern und neu starten“). Nach einem Neustart musst du am Display wieder ein Profil wählen. Ein leeres Passwortfeld bedeutet „unverändert“. Klappt das WLAN nach der Änderung nicht, öffnet das Gerät nach 15 Sekunden sein eigenes WLAN (Standard `Laufband-Bridge`).

## Daten abrufen

`GET http://laufband.local/api/data` liefert JSON, z. B.:

```json
{"connected":true,"device":"FS-B0B1AC","speed_kmh":8.00,"pace_s_per_km":450,
 "distance_m":1234,"incline_pct":1.0,"heart_rate":null,"energy_kcal":62,
 "elapsed_s":555,"age_ms":420}
```

Nicht gelieferte Werte sind `null`. `age_ms` ist das Alter des letzten Pakets. Die Schnittstelle erlaubt Abrufe von anderen Webseiten (CORS) und eignet sich z. B. für Home Assistant, Skripte oder die nächste Phase (Footpod-Bridge für die Garmin).

## Einstellungen (`include/config.h`)

| Option | Bedeutung |
|---|---|
| `WIFI_SSID`, `WIFI_PASS` | dein WLAN (2,4 GHz) |
| `HOSTNAME` | Name für `http://<name>.local` |
| `AUTO_CONNECT` | mit gespeichertem Gerät automatisch verbinden |
| `TARGET_NAME` | optional Namensanfang (z. B. `FS-`) für Auto-Verbinden ohne gespeichertes Gerät |
| `FORCE_AP` | `true` = immer eigenes WLAN statt Heim-WLAN (z. B. am Laufband ohne Router) |
| `FOOTPOD_NAME`, `FOOTPOD_AUTOSTART` | Name des Foot Pods an der Uhr, Start beim Einschalten |
| `CADENCE_MANUAL` | 0 = Kadenz automatisch, sonst fester Wert |
| `SERIAL_LOG` | `true` = Meldungen zusätzlich im Serial Monitor (Debug) |

## Hinweise

- **Webseite langsam, sobald die Uhr verbunden ist?** Die Verbindungen zu Uhr und Laufband nutzen lange Intervalle (100-200 ms), WLAN läuft ohne WiFi 6 und bekommt bei Funk-Konflikten Vorrang (`WIFI_LEGACY`, `COEX_PREFER_WIFI` in `config.h`). Im Log der Seite steht beim Verbinden das Intervall der Uhr. Bricht die Uhr-Verbindung dadurch öfter ab, setze `COEX_PREFER_WIFI = false`.
- **Webseite langsam?** ESP32-C6 teilt sich die Funkeinheit zwischen WLAN und Bluetooth. Deshalb nutzt das Projekt einen asynchronen Webserver, sucht nur mit kleinem Funkanteil und pausiert die automatische Suche, solange du manuell suchst. Dauert es trotzdem lange, hilft ein Neustart des ESP32 oder `FORCE_AP = true` (direkte Verbindung zum ESP32 ohne Router, meist am schnellsten).

- Die Seite zeigt nur die **letzte Statusmeldung** (unter der Statuszeile). Ein ausführliches Protokoll gibt es im Serial Monitor mit `SERIAL_LOG = true` (und `RAW_LOG = true` für alle Rohpakete).
- Kommen keine Werte an, starte das Laufband (viele Geräte senden erst bei laufendem Band).
- Die Webseite ist nicht passwortgeschützt, nutze sie nur im Heimnetz.

## Nächste Phase

Footpod-Signal für die Forerunner 970 senden, sobald die Werte stimmen.
