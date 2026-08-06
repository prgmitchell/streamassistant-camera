/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "signaling-metadata.h"

#if defined(_WIN32) && !defined(STREAMASSISTANT_METADATA_PARSE_ONLY)
#include <windows.h>
#include <winhttp.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIGNALING_HOST L"streamassistant.app"
#define SIGNALING_ORIGIN L"Origin: https://camera.streamassistant.app\r\n"
#define MESSAGE_CAPACITY 4096
#define RECONNECT_BASE_DELAY_MS 2000U
#define RECONNECT_MAX_DELAY_MS 300000U

#if defined(_WIN32) && !defined(STREAMASSISTANT_METADATA_PARSE_ONLY)

struct signaling_metadata_client {
	struct camera_config config;
	signaling_metadata_callback_t callback;
	void *context;
	HANDLE stop_event;
	HANDLE thread;
	CRITICAL_SECTION handle_lock;
	HINTERNET websocket;
};

static bool stopped(const struct signaling_metadata_client *client)
{
	return WaitForSingleObject(client->stop_event, 0) == WAIT_OBJECT_0;
}

#endif

static bool supported_resolution(uint32_t width, uint32_t height)
{
	return (width == 1280 && height == 720) || (width == 720 && height == 1280) ||
	       (width == 1920 && height == 1080) || (width == 1080 && height == 1920);
}

static bool parse_number_field(const char *message, const char *field, uint32_t *value)
{
	char pattern[64];
	const char *position;
	char *end = NULL;
	unsigned long parsed;

	if (snprintf(pattern, sizeof(pattern), "\"%s\"", field) <= 0)
		return false;

	position = strstr(message, pattern);
	if (!position)
		return false;
	position += strlen(pattern);
	while (*position == ' ' || *position == '\t')
		position++;
	if (*position++ != ':')
		return false;
	while (*position == ' ' || *position == '\t')
		position++;

	parsed = strtoul(position, &end, 10);
	if (!end || end == position || parsed > UINT32_MAX)
		return false;

	*value = (uint32_t)parsed;
	return true;
}

static bool string_field_equals(const char *message, const char *field, const char *expected)
{
	char pattern[64];
	const char *position;
	const char *end;
	size_t expected_length;

	if (snprintf(pattern, sizeof(pattern), "\"%s\"", field) <= 0)
		return false;

	position = strstr(message, pattern);
	if (!position)
		return false;
	position += strlen(pattern);
	while (*position == ' ' || *position == '\t')
		position++;
	if (*position++ != ':')
		return false;
	while (*position == ' ' || *position == '\t')
		position++;
	if (*position++ != '"')
		return false;

	end = strchr(position, '"');
	if (!end)
		return false;
	expected_length = strlen(expected);
	return (size_t)(end - position) == expected_length && memcmp(position, expected, expected_length) == 0;
}

static bool boolean_field_equals(const char *message, const char *field, bool expected)
{
	char pattern[64];
	const char *position;
	const char *literal = expected ? "true" : "false";

	if (snprintf(pattern, sizeof(pattern), "\"%s\"", field) <= 0)
		return false;

	position = strstr(message, pattern);
	if (!position)
		return false;
	position += strlen(pattern);
	while (*position == ' ' || *position == '\t')
		position++;
	if (*position++ != ':')
		return false;
	while (*position == ' ' || *position == '\t')
		position++;

	const size_t literal_length = strlen(literal);
	const char next = position[literal_length];
	return strncmp(position, literal, literal_length) == 0 &&
	       (next == ',' || next == '}' || next == ' ' || next == '\t' || next == '\r' || next == '\n');
}

bool signaling_metadata_parse_dimensions(const char *message, const char *source_id, uint32_t *width, uint32_t *height)
{
	uint32_t parsed_width;
	uint32_t parsed_height;

	if (!message || !source_id || !width || !height || !string_field_equals(message, "type", "media-dimensions") ||
	    !string_field_equals(message, "sourceId", source_id) ||
	    !parse_number_field(message, "width", &parsed_width) ||
	    !parse_number_field(message, "height", &parsed_height) ||
	    !supported_resolution(parsed_width, parsed_height))
		return false;

	*width = parsed_width;
	*height = parsed_height;
	return true;
}

bool signaling_metadata_is_terminal_error(const char *message)
{
	return message && string_field_equals(message, "type", "media-source-error") &&
	       (string_field_equals(message, "code", "authentication-failed") ||
		boolean_field_equals(message, "retryable", false));
}

uint32_t signaling_metadata_reconnect_delay_ms(uint32_t attempt)
{
	if (attempt >= 8U)
		return RECONNECT_MAX_DELAY_MS;

	const uint32_t delay = RECONNECT_BASE_DELAY_MS << attempt;
	return delay < RECONNECT_MAX_DELAY_MS ? delay : RECONNECT_MAX_DELAY_MS;
}

#if defined(_WIN32) && !defined(STREAMASSISTANT_METADATA_PARSE_ONLY)

enum metadata_connection_result {
	METADATA_CONNECTION_RETRY,
	METADATA_CONNECTION_RETRY_AFTER_STABLE,
	METADATA_CONNECTION_STOP,
};

static bool utf8_to_wide(const char *input, wchar_t *output, size_t capacity)
{
	return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, output, (int)capacity) > 0;
}

static void set_websocket(struct signaling_metadata_client *client, HINTERNET websocket)
{
	EnterCriticalSection(&client->handle_lock);
	client->websocket = websocket;
	LeaveCriticalSection(&client->handle_lock);
}

static bool release_websocket_ownership(struct signaling_metadata_client *client, HINTERNET websocket)
{
	bool owned = false;

	EnterCriticalSection(&client->handle_lock);
	if (client->websocket == websocket) {
		client->websocket = NULL;
		owned = true;
	}
	LeaveCriticalSection(&client->handle_lock);
	return owned;
}

static enum metadata_connection_result receive_messages(struct signaling_metadata_client *client, HINTERNET websocket)
{
	char message[MESSAGE_CAPACITY + 1];
	size_t total = 0;
	bool received_dimensions = false;

	while (!stopped(client)) {
		DWORD received = 0;
		WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
		const DWORD result = WinHttpWebSocketReceive(websocket, message + total,
							     (DWORD)(MESSAGE_CAPACITY - total), &received, &type);

		if (result != ERROR_SUCCESS || type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
			return received_dimensions ? METADATA_CONNECTION_RETRY_AFTER_STABLE : METADATA_CONNECTION_RETRY;
		if (type != WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE &&
		    type != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)
			continue;

		total += received;
		if (total >= MESSAGE_CAPACITY)
			return received_dimensions ? METADATA_CONNECTION_RETRY_AFTER_STABLE : METADATA_CONNECTION_RETRY;
		if (type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE)
			continue;

		message[total] = '\0';
		if (signaling_metadata_is_terminal_error(message))
			return METADATA_CONNECTION_STOP;
		uint32_t width;
		uint32_t height;
		if (signaling_metadata_parse_dimensions(message, client->config.source_id, &width, &height)) {
			received_dimensions = true;
			client->callback(width, height, client->context);
		}
		total = 0;
	}

	return METADATA_CONNECTION_STOP;
}

static enum metadata_connection_result run_connection(struct signaling_metadata_client *client)
{
	HINTERNET session = NULL;
	HINTERNET connection = NULL;
	HINTERNET request = NULL;
	HINTERNET websocket = NULL;
	char path[128];
	wchar_t wide_path[128];
	char join_message[512];
	DWORD status = 0;
	DWORD status_size = sizeof(status);
	enum metadata_connection_result result = METADATA_CONNECTION_RETRY;

	if (snprintf(path, sizeof(path), "/signaling?session=%s", client->config.session_id) <= 0 ||
	    !utf8_to_wide(path, wide_path, _countof(wide_path)))
		goto cleanup;

	session = WinHttpOpen(L"StreamAssistant Camera/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
			      WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)
		goto cleanup;
	WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);

	connection = WinHttpConnect(session, SIGNALING_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!connection || stopped(client))
		goto cleanup;

	request = WinHttpOpenRequest(connection, L"GET", wide_path, NULL, WINHTTP_NO_REFERER,
				     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!request || !WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0) ||
	    !WinHttpSendRequest(request, SIGNALING_ORIGIN, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
	    !WinHttpReceiveResponse(request, NULL) ||
	    !WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				 WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX) ||
	    status != HTTP_STATUS_SWITCH_PROTOCOLS || stopped(client))
		goto cleanup;

	websocket = WinHttpWebSocketCompleteUpgrade(request, 0);
	if (!websocket)
		goto cleanup;
	WinHttpCloseHandle(request);
	request = NULL;
	set_websocket(client, websocket);

	const int written = snprintf(join_message, sizeof(join_message),
				     "{\"type\":\"media-metadata-join\",\"sessionId\":\"%s\",\"sourceId\":\"%s\","
				     "\"secret\":\"%s\",\"peerId\":\"metadata_%s\"}",
				     client->config.session_id, client->config.source_id, client->config.viewer_secret,
				     client->config.source_id);
	if (written <= 0 || (size_t)written >= sizeof(join_message) ||
	    WinHttpWebSocketSend(websocket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, join_message,
				 (DWORD)written) != ERROR_SUCCESS)
		goto cleanup;

	result = receive_messages(client, websocket);

cleanup:
	if (websocket) {
		if (release_websocket_ownership(client, websocket))
			WinHttpCloseHandle(websocket);
	}
	if (request)
		WinHttpCloseHandle(request);
	if (connection)
		WinHttpCloseHandle(connection);
	if (session)
		WinHttpCloseHandle(session);
	return result;
}

static DWORD WINAPI metadata_thread(void *context)
{
	struct signaling_metadata_client *client = context;
	uint32_t reconnect_attempt = 0;

	while (!stopped(client)) {
		const enum metadata_connection_result result = run_connection(client);
		if (result == METADATA_CONNECTION_STOP || stopped(client))
			break;
		if (result == METADATA_CONNECTION_RETRY_AFTER_STABLE)
			reconnect_attempt = 0;

		const DWORD delay = signaling_metadata_reconnect_delay_ms(reconnect_attempt);
		if (reconnect_attempt < UINT32_MAX)
			reconnect_attempt++;
		if (WaitForSingleObject(client->stop_event, delay) == WAIT_OBJECT_0)
			break;
	}

	return 0;
}

struct signaling_metadata_client *signaling_metadata_start(const struct camera_config *config,
							   signaling_metadata_callback_t callback, void *context)
{
	struct signaling_metadata_client *client;

	if (!camera_config_is_valid(config) || !callback)
		return NULL;

	client = calloc(1, sizeof(*client));
	if (!client)
		return NULL;

	memcpy(&client->config, config, sizeof(client->config));
	client->callback = callback;
	client->context = context;
	InitializeCriticalSection(&client->handle_lock);
	client->stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (!client->stop_event)
		goto fail;

	client->thread = CreateThread(NULL, 0, metadata_thread, client, 0, NULL);
	if (!client->thread)
		goto fail;

	return client;

fail:
	if (client->stop_event)
		CloseHandle(client->stop_event);
	DeleteCriticalSection(&client->handle_lock);
	SecureZeroMemory(client, sizeof(*client));
	free(client);
	return NULL;
}

void signaling_metadata_stop(struct signaling_metadata_client *client)
{
	if (!client)
		return;

	SetEvent(client->stop_event);
	EnterCriticalSection(&client->handle_lock);
	if (client->websocket) {
		WinHttpCloseHandle(client->websocket);
		client->websocket = NULL;
	}
	LeaveCriticalSection(&client->handle_lock);
	WaitForSingleObject(client->thread, INFINITE);

	CloseHandle(client->thread);
	CloseHandle(client->stop_event);
	DeleteCriticalSection(&client->handle_lock);
	SecureZeroMemory(client, sizeof(*client));
	free(client);
}

bool signaling_metadata_matches(const struct signaling_metadata_client *client, const struct camera_config *config)
{
	return client && config && strcmp(client->config.session_id, config->session_id) == 0 &&
	       strcmp(client->config.source_id, config->source_id) == 0 &&
	       strcmp(client->config.viewer_secret, config->viewer_secret) == 0;
}

#endif
