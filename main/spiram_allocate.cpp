//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT
//
//

#include "spiram_allocate.h"

#include <esp_heap_caps.h>

void *spiram_allocate(size_t size) {
  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

void spiram_free(void *ptr) {
  heap_caps_free(ptr);
}
