/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "signaling-metadata.h"

#include <stdio.h>

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
	const char *source_id = "source_123";
	uint32_t width = 0;
	uint32_t height = 0;

	check(signaling_metadata_parse_dimensions(
		      "{\"type\":\"media-dimensions\",\"sourceId\":\"source_123\",\"width\":1280,\"height\":720}",
		      source_id, &width, &height),
	      "parse supported dimensions");
	check(width == 1280 && height == 720, "return parsed dimensions");
	check(signaling_metadata_parse_dimensions(
		      "{\"type\":\"media-dimensions\",\"sourceId\":\"source_123\",\"width\":1080,\"height\":1920}",
		      source_id, &width, &height),
	      "parse portrait dimensions");
	check(width == 1080 && height == 1920, "return parsed portrait dimensions");
	check(!signaling_metadata_parse_dimensions(
		      "{\"type\":\"media-dimensions\",\"sourceId\":\"source_123\",\"width\":2560,\"height\":1440}",
		      source_id, &width, &height),
	      "reject retired 1440p dimensions");
	check(!signaling_metadata_parse_dimensions(
		      "{\"type\":\"media-dimensions\",\"sourceId\":\"source_123\",\"width\":3840,\"height\":2160}",
		      source_id, &width, &height),
	      "reject retired 4K dimensions");
	check(!signaling_metadata_parse_dimensions(
		      "{\"type\":\"media-dimensions\",\"sourceId\":\"other_123\",\"width\":1280,\"height\":720}",
		      source_id, &width, &height),
	      "reject another source");
	check(!signaling_metadata_parse_dimensions(
		      "{\"type\":\"media-dimensions\",\"sourceId\":\"source_123\",\"width\":3000,\"height\":2000}",
		      source_id, &width, &height),
	      "reject an unsupported resolution");
	check(!signaling_metadata_parse_dimensions(
		      "{\"type\":\"media-offer\",\"sourceId\":\"source_123\",\"width\":1280,\"height\":720}",
		      source_id, &width, &height),
	      "reject another signaling message");

	if (failures == 0)
		printf("All StreamAssistant Camera signaling metadata tests passed.\n");
	return failures == 0 ? 0 : 1;
}
