//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT
//
//

#pragma once

#include <cstddef>
#include <cstdint>

using webhook_callback_t = void (*)(const uint8_t *body, size_t length);

void webserver_init(webhook_callback_t callback);
