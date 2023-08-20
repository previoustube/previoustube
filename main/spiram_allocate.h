//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT
//
//

#pragma once

#include <stddef.h>

void *spiram_allocate(size_t size);
void spiram_free(void *ptr);
