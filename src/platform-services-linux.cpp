/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

extern "C" {
#include "platform-services.h"
}

#include <QApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QThread>
#include <QUrl>

#include <cerrno>
#include <sys/random.h>

bool camera_platform_random_bytes(void *buffer, size_t size)
{
	if (!buffer)
		return false;
	auto *bytes = static_cast<unsigned char *>(buffer);
	while (size > 0) {
		const ssize_t count = getrandom(bytes, size, 0);
		if (count < 0 && errno == EINTR)
			continue;
		if (count <= 0)
			return false;
		bytes += count;
		size -= static_cast<size_t>(count);
	}
	return true;
}

bool camera_platform_open_url(const char *url)
{
	if (!url || !qApp)
		return false;
	const QUrl target = QUrl::fromEncoded(url, QUrl::StrictMode);
	if (!target.isValid() || target.scheme() != QStringLiteral("https"))
		return false;
	bool opened = false;
	const auto open = [&] { opened = QDesktopServices::openUrl(target); };
	if (QThread::currentThread() == qApp->thread())
		open();
	else
		QMetaObject::invokeMethod(qApp, open, Qt::BlockingQueuedConnection);
	return opened;
}

bool camera_platform_confirm_pairing_reset(void)
{
	if (!qApp)
		return false;
	bool confirmed = false;
	const auto prompt = [&] {
		confirmed = QMessageBox::question(nullptr, QStringLiteral("Reset StreamAssistant Camera pairing"),
			QStringLiteral("The existing phone link will stop working. Generate a new pairing link?"),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
	};
	if (QThread::currentThread() == qApp->thread())
		prompt();
	else
		QMetaObject::invokeMethod(qApp, prompt, Qt::BlockingQueuedConnection);
	return confirmed;
}

void camera_platform_secure_zero(void *buffer, size_t size)
{
	auto *bytes = static_cast<volatile unsigned char *>(buffer);
	while (bytes && size-- > 0)
		*bytes++ = 0;
}
