//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#include "esp_event.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "gui.h"
#include "lcds.h"
#include "leds.h"
#include "nvs_flash.h"
#include "touchpads.h"
#include "wifi.h"
#include <esp_psram.h>
#include <rom/ets_sys.h>
#include <sys/stat.h>

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
  for(int i = 0; i < NUM_LCDS; i++) {
    leds_set_rgb(&state, i, 239, 192, 112);
  }
  leds_update(&state);

  for (int i = 0; i < NUM_LCDS; i++) {
    lv_disp_set_default(gui_get_display(i));
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, "S:/spiffs/split_flap.png");
    lv_obj_set_pos(img, 0, 0);
    lv_obj_set_size(img, LCD_WIDTH, LCD_HEIGHT);

    lv_obj_t *label = lv_label_create(lv_scr_act());

    char text[2];
    snprintf(text, 2, "%d", i);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(lv_scr_act(), lv_color_hex(0xFCF9D9),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(lv_scr_act(), &lv_font_montserrat_48,
                               LV_PART_MAIN);

    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  }

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
  gui_init();

  touchpads_init(button_tapped, test_button_touched);

  struct stat st;
  if (stat(SPIFFS_MOUNTPOINT "wifi.txt", &st) == 0) {
    wifi_read_credentials_and_connect(SPIFFS_MOUNTPOINT "wifi.txt");
  } else {
    // usability affordance if someone doesn't follow directions
    wifi_read_credentials_and_connect(SPIFFS_MOUNTPOINT "wifi.sample.txt");
  }

  activate_scene1();
}
