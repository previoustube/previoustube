//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>

typedef enum {
  TOUCHPAD_LEFT_BUTTON,
  TOUCHPAD_MIDDLE_BUTTON,
  TOUCHPAD_RIGHT_BUTTON
} touchpad_button_t;

typedef void (*button_touched_cb)(touchpad_button_t button, bool is_pressed);
typedef void (*button_tapped_cb)(touchpad_button_t button);

void touchpads_init(button_tapped_cb tapped_callback,
                    button_touched_cb touched_callback);
