# PreviousTube

UNOFFICIAL reverse-engineered open source firmware for the Rotrics Nextube clock.

## Feature Status: Incomplete and Unusable!

The *only* reason you would install this is to contribute *code* to the effort. Much later this may be helpful to
others.

## Hardware Notes

The core of the device is an ESP32-WROVER-E with 16MB of Flash and 8MB of PSRAM. This is capable of WiFi and Bluetooth.
This is connected via SPI to six ST7735-based 16-bit color LCD displays, three touchpads, a speaker, and an external RTC
chip (with
battery), and six WS2812 (aka Neopixel)-compatible RGB LEDs. Flashing can be done using the built-in USB to Serial
adapter.

## Reverse Engineering Status:

| Part        | Model                         | Works?             | Pins                                                                                                                                                                         | Notes                                                                                                    |
|:------------|:------------------------------|:-------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:---------------------------------------------------------------------------------------------------------|
| CPU:        | ESP32-WROVER-E                | :heavy_check_mark: |                                                                                                                                                                              | 16MB Flash, 8MB PSRAM                                                                                    |
| Displays    | Unknown ST7735-based          | :heavy_check_mark: | Backlight PWM GPIO19, SPI SCK GPIO12, SPI MOSI GPIO13, DC GPIO14, Reset GPIO27, LCD1 CS GPIO33, LCD2 CS GPIO26, LCD3 CS GPIO21, LCD4 CS GPIO0, LCD5 CS GPIO5, LCD6 CS GPIO18 | Seems capable of up to 60fps per display, 30fps overall, PWM backlight                                   |
| LEDs        | Unknown WS2812-compatible RGB | :heavy_check_mark: | Output GPIO32                                                                                                                                                                | Updated from one pin using WS2812 "Neopixel" protocol                                                    |
| Touch Pads  | 3x metal pins on surface      | :heavy_check_mark: | GPIO2, GPIO4, GPIO15                                                                                                                                                         | Connected to standard ESP32 touch input peripheral                                                       |
| RTC (Clock) | Unconfirmed PCF8563           | :x:                | i2c SCL GPIO22, i2c SDA GPIO23                                                                                                                                               | Probably connected via i2c                                                                               |
| Speaker     | Unconfirmed LTK8002D amp      | :x:                | Probably DAC on pin 25                                                                                                                                                       | Untested                                                                                                 |
| WiFi        | ESP32 Built-in                | :x:                | n/a                                                                                                                                                                          | This is completely standard and just unimplemented in software here. Confirmed working with MicroPython. |

All on Hardware Rev "1.31 2022/01/19" according to the PCB.

## Building

1. Install ESP-IDF with the official
   instructions: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html
2. Activate ESP-IDF environment: `source <path-to-esp-idf>/esp-idf/export.sh`
3. `idf.py build`

## Workflow

I use CLion with the ESP-IDF instructions https://www.jetbrains.com/help/clion/esp-idf.html and use "idf.py monitor" for
logs. For faster iteration you can comment out 'FLASH_IN_PROJECT' in CMakeLists.txt to avoid flashing the art assets
over and over if you have already flashed once and they haven't changed.

