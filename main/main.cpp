//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#include <cstdlib>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <esp_psram.h>
#include <esp_spiffs.h>
#include <nvs_flash.h>
#include <sys/stat.h>

#include "clock.h"
#include "drivers/lcds.h"
#include "drivers/leds.h"
#include "drivers/touchpads.h"
#include "drivers/wifi.h"
#include "gui.h"
#include "led_manager.h"
#include "rtc.h"
#include "webserver.h"

#define SPIFFS_MOUNTPOINT_NO_SLASH "/spiffs"
#define SPIFFS_MOUNTPOINT SPIFFS_MOUNTPOINT_NO_SLASH "/"

static const auto TAG = "previoustube";

static const auto SNTP_SERVER = "pool.ntp.org";

ESP_EVENT_DECLARE_BASE(DISPATCH_EVENTS);
enum {
  DISPATCH_EVENT_TIME_CHANGED,
  DISPATCH_EVENT_RTC_TIME_LOADED,
};

bool blinks_enabled = true;
void blink_led(size_t led_index, uint8_t color_r, uint8_t color_g,
               uint8_t color_b, int repetitions, int period);

void test_button_touched(touchpad_button_t button, bool is_pressed) {
  auto &leds = led_manager::get();

  switch (button) {
  case TOUCHPAD_LEFT_BUTTON:
    if (is_pressed) {
      leds.set_rgb(0, 0, 255, 0);
    } else {
      leds.set_rgb(0, 255, 0, 0);
    }
    break;
  case TOUCHPAD_MIDDLE_BUTTON:
    if (is_pressed) {
      leds.set_rgb(1, 0, 255, 0);
    } else {
      leds.set_rgb(1, 255, 0, 0);
    }
    break;
  case TOUCHPAD_RIGHT_BUTTON:
    if (is_pressed) {
      leds.set_rgb(2, 0, 255, 0);
    } else {
      leds.set_rgb(2, 255, 0, 0);
    }
    break;
  }
}

void warm_leds() {
  auto &leds = led_manager::get();
  for (int i = 0; i < NUM_LCDS; i++) {
    //    leds.set_rgb(i, 239, 192, 112);
    leds.set_rgb(i, 228, 112, 37);
  }
}

void toggle_power_state() {
  static int state = 0;
  switch (state) {
  case 0:
    led_manager::get().off();
    lcds_on();
    break;
  case 1:
    led_manager::get().off();
    lcds_off();
    break;
  case 2:
    warm_leds();
    lcds_on();
    break;
  }

  state = (state + 1) % 3;
}

void button_tapped(touchpad_button_t button) {
  switch (button) {
  case TOUCHPAD_LEFT_BUTTON:
    toggle_power_state();
    gui_invalidate_all_screens();
    break;
  case TOUCHPAD_MIDDLE_BUTTON:
    clock::get().shuffle();
    break;
  case TOUCHPAD_RIGHT_BUTTON:
    blinks_enabled = !blinks_enabled;
    if (blinks_enabled) {
      blink_led(0, 0, 0, 255, 3, 1000);
    } else {
      blink_led(0, 255, 0, 0, 2, 1000);
    }
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
      .partition_label = nullptr,
      .max_files = 5,
      .format_if_mount_failed = false,
  };
  ESP_ERROR_CHECK(esp_vfs_spiffs_register(&config));
}

void ntp_changed_time(struct timeval *tv) {
  ESP_LOGI(TAG, "Time changed: %lld", tv->tv_sec);

  rtc_persist();
  clock::get().update();
}

void rtc_loaded_time(struct timeval *tv) {
  ESP_LOGI(TAG, "Time loaded: %lld", tv->tv_sec);

  clock::get().update();
}

void sntp_init() {
  esp_sntp_config_t config = {
      .smooth_sync = false,
      .server_from_dhcp = false,
      .wait_for_sync = true,
      .start = true,
      .sync_cb =
          [](struct timeval *tv) IRAM_ATTR {
            ESP_ERROR_CHECK(
                esp_event_post(DISPATCH_EVENTS, DISPATCH_EVENT_TIME_CHANGED, tv,
                               sizeof(struct timeval), portMAX_DELAY));
          },
      .renew_servers_after_new_IP = true,
      .ip_event_to_renew = IP_EVENT_STA_GOT_IP,
      .index_of_first_server = 0,
      .num_of_servers = 1,
      .servers = {SNTP_SERVER},
  };

  ESP_ERROR_CHECK(esp_netif_sntp_init(&config));
}

void on_wifi_connected() {
  ESP_LOGI(TAG, "Connected to wifi");

  sntp_init();
}

constexpr int BLINK_PERIOD_MS = 2500;
constexpr int BLINK_TIMES = 10;
constexpr uint8_t BLINK_COLOR[] = {0xFF, 0xFF, 0x00};

struct blink_user_data {
  size_t led_index;

  uint8_t color_r;
  uint8_t color_g;
  uint8_t color_b;

  bool _state = false;
};

void clear_override_callback(lv_timer_t *timer) {
  led_manager::get().clear_override();

  auto *user_data = static_cast<blink_user_data *>(timer->user_data);
  delete user_data;
}

void blink_callback(lv_timer_t *timer) {
  auto *user_data = static_cast<blink_user_data *>(timer->user_data);

  if (!user_data->_state) {
    led_manager::get().override_rgb(user_data->led_index, user_data->color_r,
                                    user_data->color_g, user_data->color_b);
  } else {
    led_manager::get().clear_override();
  }
  user_data->_state = !user_data->_state;
}

void blink_led(size_t led_index, uint8_t color_r, uint8_t color_g,
               uint8_t color_b, int repetitions, int period) {
  auto *user_data = new blink_user_data{.led_index = led_index,
                                        .color_r = color_r,
                                        .color_g = color_g,
                                        .color_b = color_b};
  int toggles = repetitions * 2;
  int cleanup_delay = period + 100;
  auto *clear_timer =
      lv_timer_create(clear_override_callback, cleanup_delay, user_data);
  lv_timer_set_repeat_count(clear_timer, 1);

  int blink_interval = period / toggles;
  auto *blinky_timer =
      lv_timer_create(blink_callback, blink_interval, user_data);
  lv_timer_set_repeat_count(blinky_timer, toggles);
  lv_timer_ready(blinky_timer);

  // cleanup must happen after all blinks because it frees the user_data memory
  assert(cleanup_delay > blink_interval * toggles);
}

void webhook_handler(const uint8_t *buffer, size_t length) {
  ESP_LOGI(TAG, "Webhook received: enabled? %d", blinks_enabled);

  if (!blinks_enabled) {
    return;
  }
  // TODO parse from message body
  int period = BLINK_PERIOD_MS;
  int repetitions = BLINK_TIMES;
  uint8_t color_r = BLINK_COLOR[0], color_g = BLINK_COLOR[1],
          color_b = BLINK_COLOR[2];

  size_t led_index = 5;

  blink_led(led_index, color_r, color_g, color_b, repetitions, period);
}

static void dispatch_event_handler([[maybe_unused]] void *handler_args,
                                   [[maybe_unused]] esp_event_base_t base,
                                   int32_t id, void *event_data) {
  switch (id) {
  case DISPATCH_EVENT_TIME_CHANGED:
    ntp_changed_time(static_cast<struct timeval *>(event_data));
    break;
  case DISPATCH_EVENT_RTC_TIME_LOADED:
    rtc_loaded_time(static_cast<struct timeval *>(event_data));
    break;
  }
}

extern "C" void app_main() {
  size_t psram_size = esp_psram_get_size();
  ESP_LOGI(TAG, "Starting... PSRAM size: %d bytes", psram_size);

  ESP_ERROR_CHECK(esp_event_loop_create_default());

  esp_log_level_set("gpio", ESP_LOG_WARN);
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      DISPATCH_EVENTS, ESP_EVENT_ANY_ID, dispatch_event_handler, nullptr,
      nullptr));

  spiffs_init();
  nvs_init();

  leds_init();
  leds_off();
  lcds_init();
  bool got_time = rtc_init();

  wifi_init(on_wifi_connected);
  gui_init();

  touchpads_init(button_tapped, /*test_button_touched*/ nullptr);

  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
  tzset();

  clock::get();
  warm_leds();

  webserver_init(webhook_handler);

  struct stat st {};
  if (stat(SPIFFS_MOUNTPOINT "wifi.txt", &st) == 0) {
    wifi_read_credentials_and_connect(SPIFFS_MOUNTPOINT "wifi.txt");
  } else {
    // usability affordance if someone doesn't follow directions
    wifi_read_credentials_and_connect(SPIFFS_MOUNTPOINT "wifi.sample.txt");
  }

  if (got_time) {
    struct timeval tv {};
    gettimeofday(&tv, nullptr);

    ESP_ERROR_CHECK(esp_event_post(DISPATCH_EVENTS, DISPATCH_EVENT_RTC_TIME_LOADED,
                                   &tv, sizeof(struct timeval), portMAX_DELAY));
  }
}

ESP_EVENT_DEFINE_BASE(DISPATCH_EVENTS);
