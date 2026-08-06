/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "camera-config.h"

#include <stdbool.h>
#include <stdint.h>

struct signaling_metadata_client;

typedef void (*signaling_metadata_callback_t)(uint32_t width, uint32_t height, void *context);

struct signaling_metadata_client *signaling_metadata_start(const struct camera_config *config,
							   signaling_metadata_callback_t callback, void *context);
void signaling_metadata_stop(struct signaling_metadata_client *client);
bool signaling_metadata_matches(const struct signaling_metadata_client *client, const struct camera_config *config);
bool signaling_metadata_parse_dimensions(const char *message, const char *source_id, uint32_t *width, uint32_t *height);
bool signaling_metadata_is_terminal_error(const char *message);
uint32_t signaling_metadata_reconnect_delay_ms(uint32_t attempt);
