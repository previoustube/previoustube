//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#include "esp_event.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcds.h"
#include "leds.h"
#include "nvs_flash.h"
#include "png.h"
#include "touchpads.h"
#include "wifi.h"
#include <esp_psram.h>
#include <rom/ets_sys.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define SPIFFS_MOUNTPOINT_NO_SLASH "/spiffs"
#define SPIFFS_MOUNTPOINT SPIFFS_MOUNTPOINT_NO_SLASH "/"

static const char *TAG = "previoustube";

leds_state test_leds_state = {0};

void test_button_touched(touchpad_button_t button, bool is_pressed) {
  switch (button) {
  case TOUCHPAD_LEFT_BUTTON:
    if (is_pressed) {
      leds_set_rgb(&test_leds_state, 0, 0, 255, 0);
    } else {
      leds_set_rgb(&test_leds_state, 0, 255, 0, 0);
    }
    break;
  case TOUCHPAD_MIDDLE_BUTTON:
    if (is_pressed) {
      leds_set_rgb(&test_leds_state, 1, 0, 255, 0);
    } else {
      leds_set_rgb(&test_leds_state, 1, 255, 0, 0);
    }
    break;
  case TOUCHPAD_RIGHT_BUTTON:
    if (is_pressed) {
      leds_set_rgb(&test_leds_state, 2, 0, 255, 0);
    } else {
      leds_set_rgb(&test_leds_state, 2, 255, 0, 0);
    }
    break;
  }

  leds_update(&test_leds_state);
}

void activate_lcd_test() {
  lcd_select(0);
  lcd_fill(color_to_rgb565(255, 0, 0));
  lcd_select(1);
  lcd_fill(color_to_rgb565(0, 255, 0));
  lcd_select(2);
  lcd_fill(color_to_rgb565(0, 0, 255));
  lcd_select(3);
  lcd_fill(color_to_rgb565(255, 0, 0));
  lcd_select(4);
  lcd_fill(color_to_rgb565(0, 255, 0));
  lcd_select(5);
  lcd_fill(color_to_rgb565(0, 0, 255));
}

void leds_off() {
  leds_state state = {0};
  leds_update(&state);
}

void activate_scene1() {
  leds_state state = {0};
  leds_set_rgb(&state, 0, 255, 0, 0);
  leds_set_rgb(&state, 1, 0, 255, 0);
  leds_set_rgb(&state, 2, 0, 0, 255);
  leds_set_rgb(&state, 3, 255, 0, 128);
  leds_set_rgb(&state, 4, 128, 255, 0);
  leds_set_rgb(&state, 5, 0, 255, 128);
  leds_update(&state);

  lcd_select(0);

  png_open(SPIFFS_MOUNTPOINT "0.png");
  png_draw(-1);
  png_close();

  lcd_select(1);

  png_open(SPIFFS_MOUNTPOINT "1.png");
  png_draw(-1);
  png_close();

  lcd_select(2);

  png_open(SPIFFS_MOUNTPOINT "2.png");
  png_draw(-1);
  png_close();

  lcd_select(3);

  png_open(SPIFFS_MOUNTPOINT "3.png");
  png_draw(-1);
  png_close();

  lcd_select(4);

  png_open(SPIFFS_MOUNTPOINT "4.png");
  png_draw(-1);
  png_close();

  lcd_select(5);
  png_open(SPIFFS_MOUNTPOINT "5.png");
  png_draw(-1);
  png_close();
}

#define NUM_FRAMES 10

static uint16_t *frames[NUM_FRAMES * NUM_LCDS];
const size_t frame_size = 80 * 160 * 2;

void preload_images() {
  leds_state progress_leds = {0};
  leds_update(&progress_leds);

  for (int j = 0; j < NUM_FRAMES; j++) {
    int lit_led = (j + 1) * NUM_LEDS / NUM_FRAMES;
    if (lit_led < NUM_LEDS) {
      leds_set_rgb(&progress_leds, lit_led, 255, 255, 255);
      leds_update(&progress_leds);
    }

    for (int i = 0; i < NUM_LCDS; i++) {
      char filename[42];
      //      snprintf(filename, 42, SPIFFS_MOUNTPOINT
      //      "/frame_%d_screen_%d.png", j, i);
      snprintf(filename, 42, SPIFFS_MOUNTPOINT "%d.png",
               ((j * NUM_LCDS + i) % 10));
      filename[41] = '\0';
      int frame_index = j * NUM_LCDS + i;

      frames[frame_index] = heap_caps_malloc(frame_size, MALLOC_CAP_SPIRAM);
      assert(frames[frame_index]);
      png_decode(filename, frames[frame_index], frame_size);
    }
  }

  leds_off();
}

#define LCD_BENCHMARK 0

void activate_scene2() {
#if LCD_BENCHMARK
  esp_log_level_set("gpio", ESP_LOG_NONE);
#endif

  leds_state state = {.pixel_grb = {0x22, 0x00, 0x00, 0x55, 0x00, 0x00, 0x99,
                                    0x00, 0x00, 0x77, 0x00, 0x00, 0xFF, 0x00,
                                    0x00, 0x44, 0x00, 0x00}};
  leds_update(&state);

#if LCD_BENCHMARK
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  int64_t start_time_us =
      (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
#endif

  int64_t iterations = 0;

  while (iterations < 100) {
    for (int j = 0; j < NUM_FRAMES; j++) {
      for (int i = 0; i < NUM_LCDS; i++) {
        lcd_select(i);
        lcd_copy_rect(0, 0, 80, 160, frames[j * NUM_LCDS + i], frame_size);
      }
      iterations++;
    }
  }

#if LCD_BENCHMARK
  gettimeofday(&tv_now, NULL);
  int64_t end_time_us =
      (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;

  int64_t average_frame_us = (end_time_us - start_time_us) / iterations;

  ESP_LOGI(TAG, "Benchmark complete. Time per frame: %lld", average_frame_us);
#endif
}

void button_tapped(touchpad_button_t button) {
  switch (button) {
  case TOUCHPAD_LEFT_BUTTON:
    activate_lcd_test();
    leds_off();
    break;
  case TOUCHPAD_MIDDLE_BUTTON:
    activate_scene1();
    break;
  case TOUCHPAD_RIGHT_BUTTON:
    activate_scene2();
    break;
  }
}

void nvs_init() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

void spiffs_init() {
  // optimize pngs with 'oxipng.exe -o 4 -i 0 --strip all *.png'
  esp_vfs_spiffs_conf_t config = {
      .base_path = SPIFFS_MOUNTPOINT_NO_SLASH,
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = false,
  };
  ESP_ERROR_CHECK(esp_vfs_spiffs_register(&config));
}

void app_main() {
  size_t psram_size = esp_psram_get_size();
  ESP_LOGI(TAG, "Starting... PSRAM size: %d bytes", psram_size);

  ESP_ERROR_CHECK(esp_event_loop_create_default());

  spiffs_init();
  nvs_init();

  leds_init();
  leds_off();
  lcds_init();
  wifi_init();

  preload_images();

  touchpads_init(button_tapped, test_button_touched);

  activate_scene1();

  struct stat st;
  if (stat(SPIFFS_MOUNTPOINT "wifi.txt", &st) == 0) {
    wifi_read_credentials_and_connect(SPIFFS_MOUNTPOINT "wifi.txt");
  } else {
    // usability affordance if someone doesn't follow directions
    wifi_read_credentials_and_connect(SPIFFS_MOUNTPOINT "wifi.sample.txt");
  }
}
