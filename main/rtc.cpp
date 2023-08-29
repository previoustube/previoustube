//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT
//
//

#include "rtc.h"
#include <esp_log.h>
#include <sys/time.h>

#include "i2c_helper.h"
#include "pcf8563.h"

#include "timegm.h"

static i2c_port_t i2c_port = I2C_NUM_0;
static const pcf8563_t rtc = {
    .read = i2c_read,
    .write = i2c_write,
    .handle = &i2c_port,
};

static const auto TAG = "external_rtc";

bool rtc_init() {
  ESP_ERROR_CHECK(i2c_init(i2c_port));
  ESP_ERROR_CHECK(pcf8563_init(&rtc));

  tm rtc_time = {};
  pcf8563_err_t err = pcf8563_read(&rtc, &rtc_time);
  if (err == PCF8563_ERR_LOW_VOLTAGE) {
    ESP_LOGE(TAG, "pcf8563_read failed: low voltage");
    return false;
  }
  if (err != PCF8563_OK) {
    ESP_LOGE(TAG, "pcf8563_read failed: %ld", err);
    return false;
  }

  time_t now = timegm(&rtc_time);
  timeval tv = {.tv_sec = now, .tv_usec = 0};
  settimeofday(&tv, nullptr);

  char formatted_time[26];
  asctime_r(&rtc_time, formatted_time);
  ESP_LOGI(TAG, "Got time from RTC: %s", formatted_time);

  return true;
}

void rtc_persist() {
  auto now = time(nullptr);
  tm utc_time = {};
  gmtime_r(&now, &utc_time);

  pcf8563_err_t err = pcf8563_write(&rtc, &utc_time);
  if (err != PCF8563_OK) {
    ESP_LOGE(TAG, "pcf8563_write failed: %ld", err);
    return;
  }

  char formatted_time[26];
  asctime_r(&utc_time, formatted_time);
  ESP_LOGI(TAG, "Saved time to RTC: %s", formatted_time);
}