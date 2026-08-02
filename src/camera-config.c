/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "camera-config.h"
#include "platform-services.h"

#include <stdio.h>
#include <string.h>

#define CAMERA_ORIGIN "https://camera.streamassistant.app"

static bool random_base64url(char *output, size_t output_capacity, size_t byte_count)
{
	unsigned char bytes[48];
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	size_t input_index = 0;
	size_t output_index = 0;

	if (!output || byte_count > sizeof(bytes) || output_capacity < ((byte_count * 4 + 2) / 3) + 1)
		return false;

	if (!camera_platform_random_bytes(bytes, byte_count))
		return false;

	while (input_index + 3 <= byte_count) {
		const unsigned int value = ((unsigned int)bytes[input_index] << 16) |
					   ((unsigned int)bytes[input_index + 1] << 8) |
					   (unsigned int)bytes[input_index + 2];
		output[output_index++] = alphabet[(value >> 18) & 0x3f];
		output[output_index++] = alphabet[(value >> 12) & 0x3f];
		output[output_index++] = alphabet[(value >> 6) & 0x3f];
		output[output_index++] = alphabet[value & 0x3f];
		input_index += 3;
	}

	if (input_index < byte_count) {
		const size_t remaining = byte_count - input_index;
		unsigned int value = (unsigned int)bytes[input_index] << 16;
		if (remaining == 2)
			value |= (unsigned int)bytes[input_index + 1] << 8;

		output[output_index++] = alphabet[(value >> 18) & 0x3f];
		output[output_index++] = alphabet[(value >> 12) & 0x3f];
		if (remaining == 2)
			output[output_index++] = alphabet[(value >> 6) & 0x3f];
	}

	output[output_index] = '\0';
	camera_platform_secure_zero(bytes, sizeof(bytes));
	return true;
}

static bool valid_token(const char *value, size_t minimum, size_t maximum)
{
	size_t length;

	if (!value)
		return false;

	length = strlen(value);
	if (length < minimum || length > maximum)
		return false;

	for (size_t index = 0; index < length; index++) {
		const char character = value[index];
		const bool alpha_numeric = (character >= 'a' && character <= 'z') ||
					   (character >= 'A' && character <= 'Z') ||
					   (character >= '0' && character <= '9');
		if (!alpha_numeric && character != '_' && character != '-')
			return false;
	}

	return true;
}

bool camera_config_generate(struct camera_config *config)
{
	if (!config)
		return false;

	memset(config, 0, sizeof(*config));
	return random_base64url(config->session_id, sizeof(config->session_id), 12) &&
	       random_base64url(config->source_id, sizeof(config->source_id), 12) &&
	       random_base64url(config->host_secret, sizeof(config->host_secret), 24) &&
	       random_base64url(config->guest_secret, sizeof(config->guest_secret), 24) &&
	       random_base64url(config->publisher_secret, sizeof(config->publisher_secret), 24) &&
	       random_base64url(config->viewer_secret, sizeof(config->viewer_secret), 24);
}

bool camera_config_is_valid(const struct camera_config *config)
{
	return config && valid_token(config->session_id, 10, 32) && valid_token(config->source_id, 8, 64) &&
	       valid_token(config->host_secret, 24, 80) && valid_token(config->guest_secret, 24, 80) &&
	       valid_token(config->publisher_secret, 24, 80) && valid_token(config->viewer_secret, 24, 80);
}

bool camera_build_pairing_url(const struct camera_config *config, char *url, size_t capacity)
{
	int written;

	if (!camera_config_is_valid(config) || !url || capacity == 0)
		return false;

	written = snprintf(url, capacity, CAMERA_ORIGIN "/pair#v=1&session=%s&source=%s&publisher=%s",
			   config->session_id, config->source_id, config->publisher_secret);
	return written > 0 && (size_t)written < capacity;
}

bool camera_build_receiver_url(const struct camera_config *config, char *url, size_t capacity)
{
	int written;

	if (!camera_config_is_valid(config) || !url || capacity == 0)
		return false;

	written = snprintf(url, capacity,
			   CAMERA_ORIGIN "/receive#v=1&session=%s&source=%s&publisher=%s&host=%s&guest=%s&viewer=%s",
			   config->session_id, config->source_id, config->publisher_secret, config->host_secret,
			   config->guest_secret, config->viewer_secret);
	return written > 0 && (size_t)written < capacity;
}
