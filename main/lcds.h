//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#pragma once

#include <stddef.h>
#include <stdint.h>

#define NUM_LCDS 6
#define LCD_WIDTH 80
#define LCD_HEIGHT 162

void lcds_init();
void lcd_select(size_t index);
void lcd_fill(uint16_t color);
void lcd_blit_rect(int x, int y, int width, int height, const uint16_t *pixels,
                   size_t pixels_size_bytes);
void lcd_copy_rect(int x, int y, int width, int height, const uint16_t *pixels,
                   size_t pixels_size_bytes);
void lcds_reset();

uint16_t color_to_rgb565(uint8_t red, uint8_t green, uint8_t blue);
