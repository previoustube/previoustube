//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#pragma once

#include <lvgl.h>

void gui_init();
lv_disp_t *gui_get_display(size_t index);
void gui_invalidate_all_screens();

LV_FONT_DECLARE(oswald_40)
LV_FONT_DECLARE(oswald_60)
LV_FONT_DECLARE(oswald_100)
LV_FONT_DECLARE(oswald_120)
