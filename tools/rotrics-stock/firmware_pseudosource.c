/**
 * Rotrics Nextube Stock Firmware - Annotated Pseudo-Source
 *
 * Reverse-engineered from firmware binary (app0 partition, 0x10000)
 * Captured from: ESP32-D0WD-V3 rev3, MAC 34:86:5d:a9:23:f8
 * Method: radare2 strings analysis + serial output capture + LittleFS inspection
 * Author: RSargeant1949, 2026-04-09
 *
 * This is NOT decompiled code. It is a reconstructed architecture based on:
 *   - String constants extracted from the binary (radare2 izz, 20867 strings)
 *   - Serial debug output observed at runtime (115200 baud)
 *   - LittleFS filesystem structure (mklittlefs -u extraction)
 *   - NVS partition dump and analysis
 *   - Nextube Studio Electron app (app.asar) renderer source analysis
 *
 * Build environment (inferred from binary path strings):
 *   PlatformIO + Arduino framework for ESP32
 *   arduinoespressif32 @ src-e7ea702ca094c0cc7efa57163afce758
 *   Developer path: C:/Users/HERRY0812/.platformio/
 *   LittleFS: esp_littlefs component
 *   WiFi: AutoConnect library (handles portal + credential storage)
 */

// ============================================================
// PARTITION TABLE (read from device at 0x8000)
// ============================================================
// Name     Type  SubType  Offset      Size       Notes
// nvs      data  nvs      0x00009000  0x5000     20KB  WiFi creds, PHY cal data
// otadata  data  ota      0x0000E000  0x2000     8KB   OTA boot selector
// app0     app   ota_0    0x00010000  0x480000   4608KB Active firmware
// app1     app   ota_1    0x00490000  0x480000   4608KB OTA update slot
// spiffs   data  spiffs   0x00910000  0x6F0000   7104KB LittleFS filesystem
//
// NOTE: Partition type is "spiffs" but driver is LittleFS (not classic SPIFFS).
// mklittlefs params: -b 4096 -p 256 -s 7274496
// Flash command: esptool --port COMx --baud 460800 write_flash 0x910000 spiffs.bin

// ============================================================
// LITTLEFS FILESYSTEM LAYOUT
// ============================================================
// /config.json                                     App configuration
// /audio/bell.wav
// /audio/timer.wav
// /audio/tremolo.wav, tremolo1.wav .. tremolo4.wav
// /audio/Unwritten.mp3
// /images/system/waiting/0.jpg .. 7.jpg
// /images/system/setting/album.jpg, blank.jpg, countdown.jpg, E.jpg, O.jpg, R.jpg
// /images/system/matrix/a.jpg, i.jpg, m.jpg, r.jpg, t.jpg, x.jpg
// /images/themes/{ThemeName}/Numbers/0.jpg .. 9.jpg
// /images/themes/{ThemeName}/AMPM/am.jpg, pm.jpg, blank.jpg, colon.jpg, dot.jpg,
//   countdown.jpg, pomodoro.jpg, pomodorolb.jpg, pomodorosb.jpg, sub.jpg,
//   youtube.jpg, k-sub.jpg, m-sub.jpg, alarm1.jpg, alarm2.jpg, alarmoff.jpg
// /images/themes/{ThemeName}/MutiInfo/Weather/fewClouds.jpg, fog.jpg,
//   overcastClouds.jpg, rain.jpg, sand.jpg, snow.jpg, squalls.jpg, sun.jpg,
//   thunderstorm.jpg, tornado.jpg, volcanicAsh.jpg
// /images/themes/{ThemeName}/MutiInfo/Temperature/degreec.jpg, degreef.jpg, minus.jpg
// /images/themes/{ThemeName}/MutiInfo/Humidity/degree.jpg, humidity.jpg
// /images/themes/{ThemeName}/MutiInfo/WeekDate/date/0.jpg..9.jpg
// /images/themes/{ThemeName}/MutiInfo/WeekDate/week/monday.jpg..sunday.jpg
//
// Themes: NixieOY, FlipClock, LightFuture, DarkSlate, DotMatrixRG, DotMatrixY,
//         Formula1, GlitchGR, NotionRain, RedDigits, RetroPaper, WireMesh,
//         Custom, Custom01, Custom02, Custom03

// ============================================================
// CONFIG STRUCTURE (/config.json)
// ============================================================
// Note: field names taken verbatim from binary strings and app source.
// "temperature_formate" is a typo in the original Rotrics code.
typedef struct {
    char     rtc_type[8];             // "pcf"
    // apps[]: array of app slot objects (max 16)
    char     default_app_name[16];    // "app1"
    int      album_switch_time;       // ms between album slides (default 6000)
    int      album_switch_step;       // 6
    char     video_site[16];          // "youtube"
    char     youtube_id[64];
    char     youtube_key[64];         // Google Data API key
    char     bili_uid[16];            // Bilibili UID
    int      default_countdown_time;
    // alarm_time[]: array
    int      spectrum_RGB[3];         // default [50, 80, 100]
    char     ssid[64];                // Not used for WiFi auth (AutoConnect uses NVS)
    char     password[64];            // Not used for WiFi auth
    char     weather_api_key[36];     // OpenWeatherMap API key
    int      time_zone;               // UTC offset in seconds (CEST=7200, CET=3600)
    char     City[64];                // OWM city query e.g. "Kloten,CH"
    char     temperature_formate[16]; // "Celsius" or "Fahrenheit"
    char     music_file[32];          // "/audio/Unwritten.mp3"
    char     bell_file[32];           // "/audio/bell.wav"
    char     tone_file[32];           // "/audio/tremolo3.wav"
    char     timer_file[32];          // "/audio/timer.wav"
    int      volume;                  // 0-100
    int      lcd_brightness;          // 0-100
    int      led_brightness;          // 0-100
    char     backlight_onoff[4];      // "ON" or "OFF"
    char     backlight_mode[16];      // "Solid", "Breathing", "Marquee", "Neon"
    int      backlight_RGB[];
} NextubeConfig;

// ============================================================
// FREERTOS TASK ARCHITECTURE
// ============================================================
// Observed from serial debug strings at runtime:
//
// TaskConfigs        - Reads /config.json, distributes via xMessageBufferConfig
// TaskNtp            - NTP sync (pool.ntp.org), updates PCF RTC
// TaskWeather        - Polls OWM API, sends to TaskDisplay via mailboxWeatherData
// TaskDisplay        - Renders time/weather/info on six ST7735 LCDs
// TaskLed            - WS2812 backlight LED control
// TaskAudio          - Sound playback
// TaskIIC            - I2C bus master (PCF RTC chip)
// TaskWifiServer     - HTTP server: AutoConnect WiFi portal + /api/* endpoints
// TaskYoutubeAndBili - YouTube/Bilibili subscriber count fetch and display
//
// Inter-task communication: FreeRTOS mailboxes (xMessageBuffer)
// Common error pattern: "Error! mailboxConfigs get config failed in Task[X]."

// ============================================================
// TASK: TaskConfigs (config loading)
// ============================================================
void TaskConfigs(void* param) {

    // Mount LittleFS (registered as "/spiffs" VFS path despite using LittleFS driver)
    if (!LittleFS.begin()) {
        Serial.println("SPIFFS MOUNT FAIL");
        // FALLBACK to hardcoded defaults:
        //   weather_api_key = ""       -> OWM 401 -> weather shows 0
        //   City = "New York"
        //   time_zone = 28800          (UTC+8, Rotrics China default)
        //   temperature_formate = "Celsius"
    } else {
        Serial.println("SPIFFS MOUNTED");

        File f = LittleFS.open("/config.json", "r");
        if (!f) {
            Serial.println("Failed to read file, using default configuration");
            // Same fallback as mount failure above
        } else {
            // Deserialize JSON into NextubeConfig struct
            // Fields read: all fields in NextubeConfig
        }
    }

    // Distribute config to all tasks via xMessageBufferConfig
    // On send failure: "Error! mailboxConfigs error in TaskConfigs."

    // Loop: respond to config update requests from TaskWifiServer (/api/settings)
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================
// TASK: TaskWeather
// ============================================================
void TaskWeather(void* param) {
    NextubeConfig config;

    // Receive config from mailbox
    // Timeout error: "Error! mailboxConfigs get config failed in TaskWeather."

    while (true) {
        // Build OWM request URL:
        //   "http://api.openweathermap.org/data/2.5/weather?q="
        //   + config.City
        //   + "&units=" + (Celsius ? "metric" : "imperial")
        //   + "&APPID=" + config.weather_api_key

        // HTTP GET (no HTTPS - plain HTTP)
        // On connect/send fail: "Request weather failed!"
        // On malformed response: "Failed to read  weather http resp"

        // Parse JSON response fields:
        //   main.temp, main.feels_like, main.temp_min, main.temp_max
        //   main.pressure, main.humidity
        //   weather[0] (condition code/description)
        //   timezone (UTC offset from OWM - used for sunrise/sunset calculation)
        //   sys.sunrise, sys.sunset

        // Send parsed data to TaskDisplay via mailboxWeatherData
        // Error: "Error! mailboxWeatherData get weatherData failed in TaskDisplay."

        // Also: "weatherImgPtr malloc failed" if image memory allocation fails

        vTaskDelay(pdMS_TO_TICKS(/* ~30 minutes polling interval */));
    }
}

// ============================================================
// TASK: TaskNtp  -- DST BUG IS HERE
// ============================================================
void TaskNtp(void* param) {
    NextubeConfig config;

    // Receive config from mailbox
    // Error: "Error! mailboxConfigs get config failed in TaskNtp."
    // Error: "Error! xSemaphoreWeb error in TaskNtp."

    Serial.println("ntp++++++");

    // BUG: configTime() takes a fixed UTC offset.
    // The Nextube Studio app computes time_zone as:
    //   Math.round(new Date().getTimezoneOffset() * -60)
    // This is correct at the moment of upload but DOES NOT auto-adjust for DST.
    //
    // For Switzerland:
    //   Summer (CEST): getTimezoneOffset() = -120 -> time_zone = 7200 (correct)
    //   Winter (CET):  getTimezoneOffset() = -60  -> time_zone = 3600 (correct)
    // But if you upload in summer and DST ends, clock stays on 7200 = 1hr fast.
    // If you upload in winter and DST starts, clock stays on 3600 = 1hr slow.
    //
    configTime(config.time_zone, 0, "pool.ntp.org");

    // FIX: Replace the above with POSIX TZ string for automatic DST handling:
    //   configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
    //   Germany/Austria/Switzerland POSIX: "CET-1CEST,M3.5.0,M10.5.0/3"
    //   DST starts: last Sunday March at 02:00
    //   DST ends:   last Sunday October at 03:00

    // Wait for NTP sync
    Serial.println("Ntp adjust_time finished.");

    // Update PCF RTC chip via I2C (TaskIIC)
    // Repeats on NTP re-sync interval (every ~2 hours based on observation)
}

// ============================================================
// HTTP API ENDPOINTS (TaskWifiServer)
// ============================================================
//
// Application API:
//   GET  /api/ping              Health check -> {"status":"OK"}
//   GET  /api/firmwareVersion   -> {"version":"x.x.x"}
//   GET  /api/hardwareVersion   -> {"version":"x.x.x"}
//   POST /api/update_firmware   OTA firmware update (binary body or URL)
//   POST /api/reset             Reboots device
//   POST /api/button            Simulates button press
//   POST /api/settings          Update settings (JSON body matching NextubeConfig)
//   POST /api/file/upload       Upload file to LittleFS
//   GET  /api/file/download     Download file from LittleFS (?path=/config.json)
//   GET  /api/file/ls           List directory (?path=/)
//   GET  /api/file/df           LittleFS disk usage
//   POST /api/file/delete       Delete file from LittleFS
//
// AutoConnect WiFi portal (served when in AP mode or via /cfg/ path):
//   GET  /cfg/config            WiFi config HTML page
//   POST /cfg/connect           Submit SSID + password
//   GET  /cfg/result            Redirect page after connect attempt
//   POST /cfg/del               Delete saved WiFi credential
//
// AP mode activation: hold left button ~2s (one beep) then double-click middle button
// AP SSID:     nextube-ap-{MAC_LAST6}  e.g. "nextube-ap-34865DA923F8"
// AP password: 12345678
// AP IP:       10.10.10.1
//
// IMPORTANT: /api/settings is the clean fix path for weather/timezone config
// without needing to reflash firmware or patch SPIFFS. Requires WiFi connection.

// ============================================================
// HARDWARE PINOUT (previoustube README + GPIO strings in binary)
// ============================================================
// Display (6x ST7735 160x80 IPS LCDs via SPI):
//   SPI SCK:    GPIO12
//   SPI MOSI:   GPIO13
//   DC:         GPIO14
//   Reset:      GPIO27
//   LCD1 CS:    GPIO33   (leftmost tube)
//   LCD2 CS:    GPIO26
//   LCD3 CS:    GPIO21
//   LCD4 CS:    GPIO0
//   LCD5 CS:    GPIO5
//   LCD6 CS:    GPIO18   (rightmost tube)
//   Brightness: GPIO19   (PWM backlight)
//
// Backlight:
//   WS2812 RGB LEDs: GPIO19 (shared with backlight PWM? TBD)
//
// Touch pads: 3x (GPIO TBD - correspond to left/middle/right buttons)
// Speaker:    (GPIO TBD)
// RTC (PCF):  I2C SDA/SCL (GPIO TBD)
// USB Serial: CH340G USB-to-UART (DTR/RTS connected to ESP32 EN/IO0 for reset)

// ============================================================
// BUG ANALYSIS SUMMARY
// ============================================================
//
// BUG 1: Weather shows 0 degrees
//
//   Symptom: Sixth display tube shows weather icon but "0" for temperature.
//   Root cause chain:
//     1. Rotrics OWM key (14e392ef37de27100ac00d560c3b0879) is dead/revoked.
//     2. Nextube Studio app.asar hardcodes this dead key in renderer JS.
//        Location: packages/renderer/dist/index.*.js
//        Pattern: weather_api_key:"14e392ef37de27100ac00d560c3b0879"
//     3. App overwrites /config.json on startup with dead key.
//     4. Firmware reads /config.json (or uses default with empty key).
//     5. OWM returns HTTP 401 for dead/empty key.
//     6. Weather task receives no data, display shows 0.
//
//   Fix A (Nextube Studio patch):
//     Extract app.asar, replace dead key with your own OWM key in renderer JS,
//     repack and replace app.asar. Then Upload via Nextube Studio.
//
//   Fix B (WiFi API - cleanest):
//     Connect Nextube to WiFi, find its IP, POST to /api/settings:
//       curl -X POST http://{NEXTUBE_IP}/api/settings \
//         -H "Content-Type: application/json" \
//         -d '{"weather_api_key":"YOUR_KEY","City":"Kloten,CH","time_zone":7200}'
//
//   Fix C (SPIFFS flash):
//     Build correct LittleFS image with your key in /config.json,
//     flash to 0x910000. Currently not working - see open question below.
//
//   Fix D (custom firmware):
//     Build previoustube or modified stock firmware with key hardcoded.
//
//   OPEN QUESTION: Why does SPIFFS flash not take effect?
//     Despite confirmed correct flash (address 0x910000, params matching),
//     firmware still falls back to default config after LittleFS mount.
//     Hypothesis: LittleFS version mismatch causing silent format/remount,
//     or firmware checks an integrity marker before trusting the filesystem.
//     Further investigation needed with serial debug output from boot sequence.
//
// BUG 2: Time 1 hour behind (DST)
//
//   Symptom: Clock shows correct time after upload but drifts 1 hour behind
//     after DST transition or after ~2hr NTP re-sync.
//   Root cause: configTime(fixed_offset, 0, "pool.ntp.org") in TaskNtp.
//     Fixed offset set from PC clock at upload time, not auto-adjusting.
//   Fix: Replace with configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org")
//     Requires firmware recompile or binary patch at the configTime call site.
//
// ============================================================
