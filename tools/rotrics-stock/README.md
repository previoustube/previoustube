# Rotrics Stock Artefacts

Captured from a live Nextube clock (ESP32-D0WD-V3 rev3, MAC: 34:86:5d:a9:23:f8) on 2026-04-09.
**Status: Weather and time CONFIRMED WORKING as of 2026-04-09 23:30 CEST.**

## Partition Table
| Name    | Type | Offset     | Size    |
|---------|------|------------|---------|
| nvs     | data | 0x00009000 | 20 KB   |
| otadata | data | 0x0000E000 | 8 KB    |
| app0    | app  | 0x00010000 | 4608 KB |
| app1    | app  | 0x00490000 | 4608 KB |
| spiffs  | data | 0x00910000 | 7104 KB |

## ROOT CAUSE: LFS_NAME_MAX mismatch (SOLVED)

The Rotrics-supplied `mklittlefs_win.exe` (v0.2.3) was compiled with `LFS_NAME_MAX=32`.
The firmware's `esp_littlefs` component requires `LFS_NAME_MAX=64`.

This mismatch causes `Corrupted dir pair at {0x0, 0x1}` on every LittleFS mount.
The firmware then attempts to reformat the partition (wiping `/config.json`),
falls back to hardcoded defaults: `weather_api_key=""`, `City="New York"` -> 0 degrees.

### Fix: use mklittlefs v4.0.2

```
tools\mklittlefs_esp32_v4.0.2_win64.exe --version
# mklittlefs ver. 4.0.2, LFS_NAME_MAX: 64  <-- correct
```

Build and flash:
```
mklittlefs_esp32_v4.0.2_win64.exe -c data_dir -d 0 -b 4096 -p 256 -s 7274496 spiffs.bin
esptool --port COMx --baud 460800 write_flash 0x910000 spiffs.bin
```

The correct mklittlefs binary (Windows x64) is in `tools/`.

## OWM API Key

Rotrics original key `14e392ef37de27100ac00d560c3b0879` is dead/revoked.
Register your own free key at openweathermap.org and set `weather_api_key` in `config.json`.

The Nextube Studio app.asar also hardcodes the dead key in:
`packages/renderer/dist/index.*.js`
Patch this with your own key, or the app will overwrite `config.json` on every startup.

## DST Bug (partial fix)

`configTime(fixed_offset, 0, "pool.ntp.org")` in TaskNtp uses a fixed UTC offset
set from the host PC clock at upload time. Does not auto-adjust at DST transitions.

Workaround: re-upload via Nextube Studio after each DST change (Spring/Autumn).
Permanent fix: `configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org")` -- requires firmware recompile.

## NVS

NVS contains ONLY WiFi credentials (AutoConnect library) and PHY calibration data.
App config (weather key, city, timezone) is entirely LittleFS-based. Not NVS.

## HTTP API (all confirmed in binary)

When the clock is connected to WiFi, it exposes a REST API:
```
POST /api/settings        push config JSON (bypasses SPIFFS entirely)
GET  /api/ping            health check
GET  /api/firmwareVersion
POST /api/update_firmware OTA update
POST /api/reset
POST /api/file/upload     upload file to LittleFS
GET  /api/file/download
GET  /api/file/ls
GET  /api/file/df
POST /api/file/delete
```
AP mode (hold left button ~2s, double-click middle): SSID `nextube-ap-{MAC}`, password `12345678`, IP `10.10.10.1`

## Files

### firmware/
- `nextube_firmware_app0_full.bin` -- full 4608KB app0 partition dump
- `nextube_firmware_app0.bin` -- first 2MB (partial, earlier capture)
- `partitions_raw.bin` -- raw partition table (4096 bytes from 0x8000)
- `strings_full.txt` -- all 20867 strings extracted by radare2

### spiffs/
- `config.json` -- working config (Kloten,CH / CEST / your own OWM key placeholder)
- `spiffs_kloten_ch.bin` -- BROKEN image (old mklittlefs v0.2.3, LFS_NAME_MAX=32)
- `spiffs_kloten_ch_lfsmax64.bin` -- CORRECT image (mklittlefs v4.0.2, LFS_NAME_MAX=64)

### tools/
- `mklittlefs_esp32_v4.0.2_win64.exe` -- correct mklittlefs for this firmware
- `libwinpthread-1.dll` -- required DLL

### Root
- `firmware_pseudosource.c` -- annotated pseudo-source (task architecture, HTTP API, bug analysis)
