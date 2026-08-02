/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "camera-config.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(bool condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		failures++;
	}
}

int main(void)
{
	struct camera_config first;
	struct camera_config second;
	char pairing_url[CAMERA_URL_CAPACITY];
	char receiver_url[CAMERA_URL_CAPACITY];

	check(camera_config_generate(&first), "generate first configuration");
	check(camera_config_generate(&second), "generate second configuration");
	check(camera_config_is_valid(&first), "first configuration validates");
	check(camera_config_is_valid(&second), "second configuration validates");
	check(strcmp(first.session_id, second.session_id) != 0, "session rotation changes the room");
	check(strcmp(first.publisher_secret, second.publisher_secret) != 0, "publisher rotation changes the secret");
	check(camera_build_pairing_url(&first, pairing_url, sizeof(pairing_url)), "build pairing URL");
	check(camera_build_receiver_url(&first, receiver_url, sizeof(receiver_url)), "build receiver URL");
	check(strstr(pairing_url, "https://camera.streamassistant.app/pair#v=1") == pairing_url,
	      "pairing URL uses the public camera origin");
	check(strstr(pairing_url, first.publisher_secret) != NULL, "pairing URL includes the publisher secret");
	check(strstr(pairing_url, first.host_secret) == NULL, "pairing URL excludes the host secret");
	check(strstr(pairing_url, first.viewer_secret) == NULL, "pairing URL excludes the viewer secret");
	check(strstr(receiver_url, first.host_secret) != NULL, "receiver URL includes the host secret");
	check(strstr(receiver_url, first.viewer_secret) != NULL, "receiver URL includes the viewer secret");

	if (failures == 0)
		printf("All StreamAssistant Camera configuration tests passed.\n");
	return failures == 0 ? 0 : 1;
}
