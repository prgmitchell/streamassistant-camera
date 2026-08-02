/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

bool camera_platform_random_bytes(void *buffer, size_t size);
bool camera_platform_open_url(const char *url);
bool camera_platform_confirm_pairing_reset(void);
void camera_platform_secure_zero(void *buffer, size_t size);
