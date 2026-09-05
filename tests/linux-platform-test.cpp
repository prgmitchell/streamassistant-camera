/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

extern "C" {
#include "camera-config.h"
#include "platform-services.h"
#include "signaling-metadata.h"
}

#include <QApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QThread>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <functional>

static QByteArray endpoint;
const char *signaling_metadata_test_endpoint() { return endpoint.constData(); }
static int failures;

static void check(bool condition, const char *message)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		++failures;
	}
}

static bool wait_for(const std::function<bool()> &condition, int timeout = 3000)
{
	QElapsedTimer timer;
	timer.start();
	while (!condition() && timer.elapsed() < timeout) {
		QCoreApplication::processEvents();
		QThread::msleep(5);
	}
	return condition();
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	check(!camera_platform_random_bytes(nullptr, 16), "reject null random buffer");
	char sensitive[] = "private pairing credential";
	camera_platform_secure_zero(sensitive, sizeof(sensitive));
	const char zeros[sizeof(sensitive)] = {};
	check(std::memcmp(sensitive, zeros, sizeof(sensitive)) == 0, "erase pairing credentials");
	check(!camera_platform_open_url(nullptr), "reject null pairing URL");
	check(!camera_platform_open_url("file:///etc/passwd"), "reject non-HTTPS pairing URL");
	QTimer::singleShot(0, [] {
		for (QWidget *widget : QApplication::topLevelWidgets())
			if (auto *prompt = qobject_cast<QMessageBox *>(widget))
				prompt->done(QMessageBox::No);
	});
	check(!camera_platform_confirm_pairing_reset(), "cancel keeps existing pairing");

	struct camera_config config;
	check(camera_config_generate(&config), "generate Linux pairing credentials");
	QWebSocketServer server(QStringLiteral("metadata-test"), QWebSocketServer::NonSecureMode);
	if (!server.listen(QHostAddress::LocalHost, 0))
		return 1;
	endpoint = "ws://127.0.0.1:" + QByteArray::number(server.serverPort()) + "/signaling";
	int connections = 0;
	bool joined = false;
	QWebSocket *peer = nullptr;
	QObject::connect(&server, &QWebSocketServer::newConnection, &server, [&] {
		++connections;
		peer = server.nextPendingConnection();
		peer->setParent(&server);
		check(peer->origin() == QStringLiteral("https://camera.streamassistant.app"), "send Camera origin");
		check(peer->requestUrl().query().contains(QString::fromUtf8(config.session_id)), "send session query");
		QObject::connect(peer, &QWebSocket::textMessageReceived, &server, [&](const QString &message) {
			const auto join = QJsonDocument::fromJson(message.toUtf8()).object();
			check(join.value("type") == QStringLiteral("media-metadata-join"), "join metadata channel");
			check(join.value("secret") == QString::fromUtf8(config.viewer_secret), "use viewer credential");
			check(join.value("sourceId") == QString::fromUtf8(config.source_id), "join requested source");
			joined = true;
		});
	});
	std::atomic<int> received{0};
	const auto callback = [](uint32_t width, uint32_t height, void *context) {
		if (width == 1080 && height == 1920)
			static_cast<std::atomic<int> *>(context)->fetch_add(1);
	};
	check(!signaling_metadata_start(nullptr, callback, &received), "reject invalid metadata config");
	auto *client = signaling_metadata_start(&config, callback, &received);
	check(client != nullptr, "start metadata transport");
	check(signaling_metadata_matches(client, &config), "recognize existing pairing");
	auto changed = config;
	changed.viewer_secret[0] = changed.viewer_secret[0] == 'a' ? 'b' : 'a';
	check(!signaling_metadata_matches(client, &changed), "detect changed viewer credential");
	check(wait_for([&] { return joined; }), "complete WebSocket join");
	if (joined) {
		peer->sendTextMessage(QStringLiteral("{\"type\":\"media-dimensions\",\"sourceId\":\"other\",\"width\":1080,\"height\":1920}"));
		QJsonObject dimensions{{"type", "media-dimensions"}, {"sourceId", QString::fromUtf8(config.source_id)},
			{"width", 1080}, {"height", 1920}};
		peer->sendTextMessage(QString::fromUtf8(QJsonDocument(dimensions).toJson(QJsonDocument::Compact)));
		check(wait_for([&] { return received.load() == 1; }), "deliver only matching source dimensions");
		joined = false;
		peer->close();
		check(wait_for([&] { return connections == 2 && joined; }, 4500), "reconnect after disconnect");
		peer->sendTextMessage(QStringLiteral("{\"type\":\"media-source-error\",\"code\":\"authentication-failed\",\"retryable\":false}"));
		check(!wait_for([&] { return connections > 2; }, 2400), "stop reconnecting after terminal authentication error");
	}
	QElapsedTimer stop_timer;
	stop_timer.start();
	signaling_metadata_stop(client);
	check(stop_timer.elapsed() < 1000, "stop metadata without blocking OBS");
	for (int index = 0; index < 20; ++index) {
		client = signaling_metadata_start(&config, callback, &received);
		signaling_metadata_stop(client);
	}
	server.close();
	std::printf("Linux platform tests: %d failure(s)\n", failures);
	return failures == 0 ? 0 : 1;
}
