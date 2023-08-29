//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT
//
//

#pragma once

#include <time.h>

#ifndef HAVE_TIMEGM

#ifdef __cplusplus
extern "C" {
#endif

time_t timegm(struct tm *tm);

#ifdef __cplusplus
}
#endif

#endif
