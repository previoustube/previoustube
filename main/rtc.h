//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT
//
//

#pragma once

/* returns true if the RTC contained a valid time upon init */
bool rtc_init();

void rtc_persist();
