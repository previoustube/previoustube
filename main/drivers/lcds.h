//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>

constexpr size_t NUM_LCDS = 6;
constexpr uint8_t LCD_WIDTH = 80;
constexpr uint8_t LCD_HEIGHT = 162;
constexpr size_t LCD_SPI_MAX_TRANSFER_SIZE = 4092;

void lcds_init();
void lcd_select(size_t index);
void lcd_blit_rect(int x, int y, int width, int height, const uint16_t *pixels,
                   size_t pixels_size_bytes);
void lcds_reset();
void lcds_on();
void lcds_off();

uint16_t color_to_rgb565(uint8_t red, uint8_t green, uint8_t blue);
