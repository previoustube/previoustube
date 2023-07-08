/*
 * Adapted from:
 * https://github.com/espressif/esp-idf/blob/master/examples/peripherals/rmt/led_strip/main/led_strip_example_main.c
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "leds.h"

#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "led_strip_encoder.h"

#define RMT_LED_STRIP_RESOLUTION_HZ                                            \
  10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high
           // resolution)
#define RMT_LED_STRIP_GPIO_NUM 32

static const char *TAG = "leds";

static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

static rmt_transmit_config_t tx_config = {
    .loop_count = 0, // no transfer loop
};

void leds_init() {
  ESP_LOGI(TAG, "Create RMT TX channel");
  rmt_tx_channel_config_t tx_chan_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
      .gpio_num = RMT_LED_STRIP_GPIO_NUM,
      .mem_block_symbols =
          64, // increase the block size can make the LED less flickering
      .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
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

void leds_update(const leds_state *state) {
  ESP_LOGI(TAG, "Updating LEDS with RMT");

  ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, state->pixel_grb,
                               sizeof(state->pixel_grb), &tx_config));
  ESP_ERROR_CHECK(rmt_tx_wait_all_done(
      led_chan, portMAX_DELAY)); // TODO: maybe don't need to wait
}

void leds_set_rgb(leds_state *state, size_t index, uint8_t red, uint8_t green, uint8_t blue) {
  assert(index < NUM_LEDS);
  state->pixel_grb[index * 3 + 0] = green;
  state->pixel_grb[index * 3 + 1] = red;
  state->pixel_grb[index * 3 + 2] = blue;
}