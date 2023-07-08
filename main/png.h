//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void png_open(const char *filename);
void png_draw(uint32_t background);
void png_decode(const char *filename, uint16_t *buffer, size_t size);
void png_close();

#ifdef __cplusplus
}
#endif