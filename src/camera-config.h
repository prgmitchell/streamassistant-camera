/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAMERA_SESSION_CAPACITY 33
#define CAMERA_SOURCE_CAPACITY 65
#define CAMERA_SECRET_CAPACITY 81
#define CAMERA_URL_CAPACITY 1024

struct camera_config {
	char session_id[CAMERA_SESSION_CAPACITY];
	char source_id[CAMERA_SOURCE_CAPACITY];
	char host_secret[CAMERA_SECRET_CAPACITY];
	char guest_secret[CAMERA_SECRET_CAPACITY];
	char publisher_secret[CAMERA_SECRET_CAPACITY];
	char viewer_secret[CAMERA_SECRET_CAPACITY];
};

bool camera_config_generate(struct camera_config *config);
bool camera_config_is_valid(const struct camera_config *config);
bool camera_build_pairing_url(const struct camera_config *config, const char *plugin_version, char *url,
			      size_t capacity);
bool camera_build_receiver_url(const struct camera_config *config, char *url, size_t capacity);
