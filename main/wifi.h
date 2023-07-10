//  SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//  SPDX-License-Identifier: MIT

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void wifi_init();
void wifi_read_credentials_and_connect(const char *filename);

#ifdef __cplusplus
}
#endif
