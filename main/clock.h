//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <lvgl.h>
#include <memory>

#include "drivers/lcds.h"
#include "flapper.h"
#include "gui.h"

#include "flap_sequence.h"

class clock {
public:
  static auto get() -> clock & {
    static clock instance;
    return instance;
  }

  void update();
  void shuffle();

  clock(clock const &) = delete;
  void operator=(const clock &) = delete;
  clock(clock&&) = delete;
  clock& operator=(clock&&) = delete;

  ~clock();
private:
  clock();

  std::array<lv_obj_t *, NUM_LCDS> background_images{};
  std::array<lv_obj_t *, NUM_LCDS-1> digit_labels{};
  std::array<flapper *, NUM_LCDS> flappers{};
  std::array<std::unique_ptr<flap_sequence>, NUM_LCDS-1> flap_sequences{};
  std::array<lv_timer_t *, NUM_LCDS-1> delayed_start_timers{};
  lv_obj_t *ampm_label_top, *ampm_label_bottom;
  lv_timer_t *clock_update_timer;
  void delayed_start_flap_sequence(size_t index);
};
