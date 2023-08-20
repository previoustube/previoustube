//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT

#include "led_manager.h"

static void flush_callback_wrapper(void *user_data) {
  auto *instance = static_cast<led_manager *>(user_data);
  instance->flush();
}

led_manager::led_manager() { leds_update(&state); }

void led_manager::set_rgb(size_t index, uint8_t red, uint8_t green,
                          uint8_t blue) {
  state.set_rgb(index, red, green, blue);

  reschedule_update();
}

void led_manager::override_rgb(size_t index, uint8_t red, uint8_t green,
                               uint8_t blue) {
  overrides.set_rgb(index, red, green, blue);

  reschedule_update();
}

void led_manager::clear_override() {
  overrides.clear();
  reschedule_update();
}

void led_manager::flush() {
  merged.merge(state, overrides);
  bool updated = leds_update_if_free(&merged);
  if (!updated) {
    reschedule_update();
  }
}

void led_manager::reschedule_update() {
  lv_async_call_cancel(flush_callback_wrapper, this);
  lv_async_call(flush_callback_wrapper, this);
}

void led_manager::off() {
  state.clear();
  reschedule_update();
}
