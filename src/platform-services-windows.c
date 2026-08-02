/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "platform-services.h"

#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>

bool camera_platform_random_bytes(void *buffer, size_t size)
{
	return buffer && size <= ULONG_MAX &&
	       BCryptGenRandom(NULL, buffer, (ULONG)size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

bool camera_platform_open_url(const char *url)
{
	wchar_t wide_url[1024];
	const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url, -1, wide_url,
					       (int)(sizeof(wide_url) / sizeof(wide_url[0])));
	HINSTANCE result;

	if (length == 0)
		return false;

	result = ShellExecuteW(NULL, L"open", wide_url, NULL, NULL, SW_SHOWNORMAL);
	return (INT_PTR)result > 32;
}

bool camera_platform_confirm_pairing_reset(void)
{
	return MessageBoxW(NULL, L"The existing phone link will stop working. Generate a new pairing link?",
			   L"Reset StreamAssistant Camera pairing", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES;
}

void camera_platform_secure_zero(void *buffer, size_t size)
{
	if (buffer)
		SecureZeroMemory(buffer, size);
}
