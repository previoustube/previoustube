//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT
//
//

#include "wifi.h"

#include "INIReader.h"
#include <cstring>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <string>

#define RETRY_LIMIT UINT_MAX

static const char *const TAG = "wifi";
static wifi_connected_callback_t s_wifi_connected_callback = nullptr;

static void event_handler([[maybe_unused]] void *arg,
                          esp_event_base_t event_base, int32_t event_id,
                          void *event_data) {
  static unsigned int s_retry_num = 0;

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < RETRY_LIMIT) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      ESP_LOGE(TAG, "failed to connect to the AP");
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    auto *event = static_cast<ip_event_got_ip_t *>(event_data);
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;

    if (s_wifi_connected_callback != nullptr) {
      s_wifi_connected_callback();
    }
  }
}

void wifi_init(wifi_connected_callback_t callback) {
  ESP_LOGI(TAG, "Starting wifi & TCP/IP...");

  s_wifi_connected_callback = callback;

  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  esp_event_handler_instance_t instance_any_id = nullptr;
  esp_event_handler_instance_t instance_got_ip = nullptr;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr,
      &instance_got_ip));
}

/* TODO: implement QR code provisioning from
 https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/provisioning/wifi_provisioning.html
 */
void wifi_read_credentials_and_connect(const char *filename) {
  try {
    auto filename_string = std::string(filename);
    auto data = INIReader(filename_string);

    auto ssid = data.GetString("", "ssid", "");
    auto psk = data.GetString("", "psk", "");

    wifi_config_t wifi_config = {};
    strlcpy(reinterpret_cast<char *>(wifi_config.sta.ssid), ssid.c_str(),
            sizeof(wifi_config.sta.ssid));
    strlcpy(reinterpret_cast<char *>(wifi_config.sta.password), psk.c_str(),
            sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_LOGI(TAG, "Attempting to connect to SSID '%s'",
             reinterpret_cast<char *>(wifi_config.sta.ssid));

    ESP_ERROR_CHECK(esp_wifi_start());
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "Failed to read wifi credentials: %s", e.what());
  }
}
