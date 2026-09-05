/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

extern "C" {
#include "signaling-metadata.h"
#include "platform-services.h"
}

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <QWebSocket>

#include <cstring>
#include <new>

#ifdef STREAMASSISTANT_METADATA_TEST_ENDPOINT
const char *signaling_metadata_test_endpoint();
#endif

struct signaling_metadata_client final : QThread {
	struct camera_config config;
	signaling_metadata_callback_t callback;
	void *context;

	~signaling_metadata_client() override { camera_platform_secure_zero(&config, sizeof(config)); }

	void run() override
	{
		QWebSocket socket(QStringLiteral("https://camera.streamassistant.app"));
		socket.setMaxAllowedIncomingMessageSize(4096);
		socket.setMaxAllowedIncomingFrameSize(4096);
		QTimer reconnect;
		reconnect.setSingleShot(true);
		uint32_t attempts = 0;
		bool terminal = false;
#ifdef STREAMASSISTANT_METADATA_TEST_ENDPOINT
		QUrl url(QString::fromUtf8(signaling_metadata_test_endpoint()));
#else
		QUrl url(QStringLiteral("wss://streamassistant.app/signaling"));
#endif
		QUrlQuery query;
		query.addQueryItem(QStringLiteral("session"), QString::fromUtf8(config.session_id));
		url.setQuery(query);
		const auto connect = [&] {
			if (!isInterruptionRequested() && !terminal)
				socket.open(QNetworkRequest(url));
		};
		const auto retry = [&] {
			if (isInterruptionRequested() || terminal || reconnect.isActive())
				return;
			reconnect.start(static_cast<int>(signaling_metadata_reconnect_delay_ms(attempts)));
			if (attempts < UINT32_MAX)
				++attempts;
		};
		QObject::connect(&reconnect, &QTimer::timeout, &socket, connect);
		QObject::connect(&socket, &QWebSocket::disconnected, &reconnect, retry);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
		QObject::connect(&socket, &QWebSocket::errorOccurred, &reconnect, retry);
#else
		QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), &reconnect, retry);
#endif
		QObject::connect(&socket, &QWebSocket::connected, &reconnect, [&] {
			const QJsonObject join{
				{QStringLiteral("type"), QStringLiteral("media-metadata-join")},
				{QStringLiteral("sessionId"), QString::fromUtf8(config.session_id)},
				{QStringLiteral("sourceId"), QString::fromUtf8(config.source_id)},
				{QStringLiteral("secret"), QString::fromUtf8(config.viewer_secret)},
				{QStringLiteral("peerId"), QStringLiteral("metadata_") + QString::fromUtf8(config.source_id)},
			};
			socket.sendTextMessage(QString::fromUtf8(QJsonDocument(join).toJson(QJsonDocument::Compact)));
		});
		QObject::connect(&socket, &QWebSocket::textMessageReceived, &reconnect, [&](const QString &text) {
			if (isInterruptionRequested() || terminal)
				return;
			const QByteArray message = text.toUtf8();
			if (signaling_metadata_is_terminal_error(message.constData())) {
				terminal = true;
				reconnect.stop();
				socket.abort();
				return;
			}
			uint32_t width;
			uint32_t height;
			if (signaling_metadata_parse_dimensions(message.constData(), config.source_id, &width, &height)) {
				attempts = 0;
				callback(width, height, context);
			}
		});
		if (!isInterruptionRequested()) {
			connect();
			exec();
		}
		reconnect.stop();
		socket.abort();
	}
};

struct signaling_metadata_client *signaling_metadata_start(const struct camera_config *config,
							   signaling_metadata_callback_t callback, void *context)
{
	if (!camera_config_is_valid(config) || !callback)
		return nullptr;
	auto *client = new (std::nothrow) signaling_metadata_client;
	if (!client)
		return nullptr;
	client->config = *config;
	client->callback = callback;
	client->context = context;
	client->start();
	return client;
}

void signaling_metadata_stop(struct signaling_metadata_client *client)
{
	if (!client)
		return;
	client->requestInterruption();
	client->quit();
	if (QThread::currentThread() == client) {
		QObject::connect(client, &QThread::finished, client, &QObject::deleteLater);
		return;
	}
	client->wait();
	delete client;
}

bool signaling_metadata_matches(const struct signaling_metadata_client *client, const struct camera_config *config)
{
	return client && config && std::strcmp(client->config.session_id, config->session_id) == 0 &&
		std::strcmp(client->config.source_id, config->source_id) == 0 &&
		std::strcmp(client->config.viewer_secret, config->viewer_secret) == 0;
}
