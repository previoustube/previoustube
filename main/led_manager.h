//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "drivers/leds.h"
#include "lvgl.h"

class led_manager {
public:
  static auto get() -> led_manager & {
    static led_manager instance;
    return instance;
  }

  void set_rgb(size_t index, uint8_t red, uint8_t green, uint8_t blue);
  void override_rgb(size_t index, uint8_t red, uint8_t green, uint8_t blue);
  void clear_override();
  void flush();

  led_manager(const led_manager &led_manager) = delete;
  void operator=(const led_manager &) = delete;
  ~led_manager() = default;
  led_manager(led_manager&&) = delete;
  led_manager& operator=(led_manager&&) = delete;

  void off();

private:
  led_manager();
  void reschedule_update();

  leds_state state{};
  leds_state overrides{};

  leds_state merged{};
};
