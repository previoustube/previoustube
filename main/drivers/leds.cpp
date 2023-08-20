//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT

#include "leds.h"

#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "led_strip_encoder.h"

#define RMT_LED_STRIP_RESOLUTION_HZ                                            \
  10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high
           // resolution)
constexpr gpio_num_t RMT_LED_STRIP_GPIO_NUM = GPIO_NUM_32;

constexpr auto TAG = "leds";

static rmt_channel_handle_t led_chan = nullptr;
static rmt_encoder_handle_t led_encoder = nullptr;

static rmt_transmit_config_t tx_config = {
    .loop_count = 0, // no transfer loop
};

void leds_init() {
  ESP_LOGI(TAG, "Create RMT TX channel");
  rmt_tx_channel_config_t tx_chan_config = {
      .gpio_num = RMT_LED_STRIP_GPIO_NUM,
      .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
      .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
      .mem_block_symbols =
          64, // increase the block size can make the LEDs flicker less
      .trans_queue_depth = 4, // set the number of transactions that can be
                              // pending in the background
  };
  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

  ESP_LOGI(TAG, "Install led strip encoder");
  led_strip_encoder_config_t encoder_config = {
      .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
  };
  ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

  ESP_LOGI(TAG, "Enable RMT TX channel");
  ESP_ERROR_CHECK(rmt_enable(led_chan));
}

void leds_off() {
  leds_state state = {};
  leds_update(&state);
}

bool leds_update_if_free(const leds_state *state) {
  esp_err_t err = rmt_tx_wait_all_done(led_chan, 0);
  if (err == ESP_ERR_TIMEOUT) {
    return false;
  }
  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, state->pixel_grb,
                               sizeof(state->pixel_grb), &tx_config));
  return true;
}

void leds_update(const leds_state *state) {
  ESP_LOGV(TAG, "Updating LEDS with RMT");

  ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, state->pixel_grb,
                               sizeof(state->pixel_grb), &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}
