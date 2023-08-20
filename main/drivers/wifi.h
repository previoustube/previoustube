//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT

#pragma once

using wifi_connected_callback_t = void (*)();

void wifi_init(wifi_connected_callback_t callback);
void wifi_read_credentials_and_connect(const char *filename);
