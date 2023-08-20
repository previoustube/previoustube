//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

constexpr size_t NUM_LEDS = 6;

struct leds_state {
  uint8_t pixel_grb[NUM_LEDS * 3] = {0,};

  void set_rgb(size_t index, uint8_t red, uint8_t green, uint8_t blue) {
    assert(index < NUM_LEDS);
    pixel_grb[index * 3 + 0] = green;
    pixel_grb[index * 3 + 1] = red;
    pixel_grb[index * 3 + 2] = blue;
  }

  void clear() {
    for (auto &i : pixel_grb) {
      i = 0;
    }
  }

  leds_state() {
    clear();
  }

  void merge(const leds_state &base, const leds_state &override) {
    for (size_t i = 0; i < NUM_LEDS; i++) {
      bool overridden = override.pixel_grb[i * 3 + 0] ||
                        override.pixel_grb[i * 3 + 1] ||
                        override.pixel_grb[i * 3 + 2];
      pixel_grb[i * 3 + 0] = overridden ? override.pixel_grb[i * 3 + 0]
                                        : base.pixel_grb[i * 3 + 0];
      pixel_grb[i * 3 + 1] = overridden ? override.pixel_grb[i * 3 + 1]
                                        : base.pixel_grb[i * 3 + 1];
      pixel_grb[i * 3 + 2] = overridden ? override.pixel_grb[i * 3 + 2]
                                        : base.pixel_grb[i * 3 + 2];
    }
  }
};

void leds_init();
void leds_off();

void leds_update(const leds_state *state);

bool leds_update_if_free(
    const leds_state *state); // returns false if update skipped because a
                              // previous update was in progress
