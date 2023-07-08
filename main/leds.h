//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>
#include <stddef.h>

#define NUM_LEDS 6

typedef struct {
  uint8_t pixel_grb[NUM_LEDS * 3];
} leds_state;

void leds_init();
void leds_update(const leds_state *state);
void leds_set_rgb(leds_state *state, size_t index, uint8_t red, uint8_t green, uint8_t blue);
