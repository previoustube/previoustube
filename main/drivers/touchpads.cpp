//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#include "touchpads.h"
#include "iot_touchpad.h"

#include <esp_attr.h>
#include <esp_event.h>
#include <esp_timer.h>

constexpr size_t NUM_TOUCHPADS = 3;

ESP_EVENT_DECLARE_BASE(TOUCHPAD_EVENTS);

enum {
  TOUCHPAD_EVENT_PUSHED,
  TOUCHPAD_EVENT_RELEASED,
  TOUCHPAD_EVENT_TAPPED,
};

static const touch_pad_t TOUCH_PADS[NUM_TOUCHPADS] = {
    TOUCH_PAD_NUM0, // pin 4, left button
    TOUCH_PAD_NUM2, // pin 2, middle button
    TOUCH_PAD_NUM3, // pin 15, right button
};

static const touchpad_button_t TOUCH_PAD_LOCATIONS[NUM_TOUCHPADS] = {
    TOUCHPAD_LEFT_BUTTON, TOUCHPAD_MIDDLE_BUTTON, TOUCHPAD_RIGHT_BUTTON};

constexpr auto TAG = "TOUCHPADS";

static button_tapped_cb button_tapped_callback = nullptr;
static button_touched_cb button_touched_callback = nullptr;

static void IRAM_ATTR touchpad_pushed_callback(void *arg) {
  const auto *button = static_cast<const touchpad_button_t *>(arg);
  ESP_ERROR_CHECK(esp_event_isr_post(TOUCHPAD_EVENTS, TOUCHPAD_EVENT_PUSHED,
                                     button, sizeof(touchpad_button_t),
                                     nullptr));
}

static void pushed_event_handler([[maybe_unused]] void *handler_args,
                                 [[maybe_unused]] esp_event_base_t base,
                                 [[maybe_unused]] int32_t id,
                                 void *event_data) {
  const auto *button = static_cast<const touchpad_button_t *>(event_data);
  if (button_touched_callback != nullptr) {
    button_touched_callback(*button, true);
  }
}

static void IRAM_ATTR touchpad_released_callback(void *arg) {
  const auto *button = static_cast<const touchpad_button_t *>(arg);
  ESP_ERROR_CHECK(esp_event_isr_post(TOUCHPAD_EVENTS, TOUCHPAD_EVENT_RELEASED,
                                     button, sizeof(touchpad_button_t),
                                     nullptr));
}

static void released_event_handler([[maybe_unused]] void *handler_args,
                                   [[maybe_unused]] esp_event_base_t base,
                                   [[maybe_unused]] int32_t id,
                                   void *event_data) {
  const auto *button = static_cast<const touchpad_button_t *>(event_data);
  if (button_touched_callback != nullptr) {
    button_touched_callback(*button, false);
  }
}

static void IRAM_ATTR touchpad_tapped_callback(void *arg) {
  const auto *button = static_cast<const touchpad_button_t *>(arg);
  ESP_ERROR_CHECK(esp_event_isr_post(TOUCHPAD_EVENTS, TOUCHPAD_EVENT_TAPPED,
                                     button, sizeof(touchpad_button_t),
                                     nullptr));
}

static void tapped_event_handler([[maybe_unused]] void *handler_args,
                                 [[maybe_unused]] esp_event_base_t base,
                                 [[maybe_unused]] int32_t id,
                                 void *event_data) {
  const auto *button = static_cast<const touchpad_button_t *>(event_data);
  if (button_tapped_callback != nullptr) {
    button_tapped_callback(*button);
  }
}

ESP_EVENT_DEFINE_BASE(TOUCHPAD_EVENTS);

void touchpads_init(button_tapped_cb tapped_callback,
                    button_touched_cb touched_callback) {
  button_tapped_callback = tapped_callback;
  button_touched_callback = touched_callback;

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      TOUCHPAD_EVENTS, TOUCHPAD_EVENT_PUSHED, pushed_event_handler, nullptr,
      nullptr));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      TOUCHPAD_EVENTS, TOUCHPAD_EVENT_RELEASED, released_event_handler, nullptr,
      nullptr));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      TOUCHPAD_EVENTS, TOUCHPAD_EVENT_TAPPED, tapped_event_handler, nullptr,
      nullptr));

  for (size_t i = 0; i < NUM_TOUCHPADS; i++) {
    touch_pad_t touch_num = TOUCH_PADS[i];

    tp_handle_t touchpad_handle = iot_tp_create(touch_num, 0.6f);
    assert(touchpad_handle);
    ESP_ERROR_CHECK(iot_tp_add_cb(touchpad_handle, TOUCHPAD_CB_PUSH,
                                  touchpad_pushed_callback,
                                  (void *)&TOUCH_PAD_LOCATIONS[i]));
    ESP_ERROR_CHECK(iot_tp_add_cb(touchpad_handle, TOUCHPAD_CB_RELEASE,
                                  touchpad_released_callback,
                                  (void *)&TOUCH_PAD_LOCATIONS[i]));
    ESP_ERROR_CHECK(iot_tp_add_cb(touchpad_handle, TOUCHPAD_CB_TAP,
                                  touchpad_tapped_callback,
                                  (void *)&TOUCH_PAD_LOCATIONS[i]));
  }
}
