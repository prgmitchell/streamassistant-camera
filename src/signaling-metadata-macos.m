/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "signaling-metadata.h"

#import <Foundation/Foundation.h>

#include <stdlib.h>
#include <string.h>

static void *metadata_queue_key = &metadata_queue_key;

@interface StreamAssistantMetadataConnection : NSObject {
    struct camera_config _config;
    signaling_metadata_callback_t _callback;
    void *_context;
    dispatch_queue_t _queue;
    NSURLSession *_session;
    NSURLSessionWebSocketTask *_task;
    dispatch_block_t _reconnect;
    uint32_t _reconnectAttempts;
    BOOL _stopped;
    BOOL _terminalFailure;
}

- (instancetype)initWithConfig:(const struct camera_config *)config
                      callback:(signaling_metadata_callback_t)callback
                       context:(void *)context;
- (void)start;
- (void)stop;
- (BOOL)matchesConfig:(const struct camera_config *)config;
- (void)closeTransport;
- (void)scheduleReconnect;
- (void)receiveNext;
- (void)connect;

@end

@implementation StreamAssistantMetadataConnection

- (instancetype)initWithConfig:(const struct camera_config *)config
                      callback:(signaling_metadata_callback_t)callback
                       context:(void *)context
{
    self = [super init];
    if (self) {
        memcpy(&_config, config, sizeof(_config));
        _callback = callback;
        _context = context;
        _queue = dispatch_queue_create("app.streamassistant.camera.metadata", DISPATCH_QUEUE_SERIAL);
        dispatch_queue_set_specific(_queue, metadata_queue_key, metadata_queue_key, NULL);
    }
    return self;
}

- (void)closeTransport
{
    [_task cancelWithCloseCode:NSURLSessionWebSocketCloseCodeGoingAway reason:nil];
    [_session invalidateAndCancel];
    _task = nil;
    _session = nil;
}

- (void)scheduleReconnect
{
    if (_stopped || _terminalFailure || _reconnect)
        return;

    [self closeTransport];
    uint32_t delay_ms = signaling_metadata_reconnect_delay_ms(_reconnectAttempts);
    if (_reconnectAttempts < UINT32_MAX)
        _reconnectAttempts++;
    __weak StreamAssistantMetadataConnection *weak_self = self;
    _reconnect = dispatch_block_create(0, ^{
        StreamAssistantMetadataConnection *strong_self = weak_self;
        if (!strong_self)
            return;
        strong_self->_reconnect = nil;
        [strong_self connect];
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t) delay_ms * NSEC_PER_MSEC), _queue, _reconnect);
}

- (void)receiveNext
{
    if (_stopped || !_task)
        return;

    __weak StreamAssistantMetadataConnection *weak_self = self;
    [_task receiveMessageWithCompletionHandler:^(NSURLSessionWebSocketMessage *message, NSError *error) {
        StreamAssistantMetadataConnection *strong_self = weak_self;
        if (!strong_self)
            return;

        dispatch_async(strong_self->_queue, ^{
            if (strong_self->_stopped)
                return;
            if (error || message.type != NSURLSessionWebSocketMessageTypeString || !message.string) {
                [strong_self scheduleReconnect];
                return;
            }

            uint32_t width = 0;
            uint32_t height = 0;
            const char *text = message.string.UTF8String;
            if (text && signaling_metadata_is_terminal_error(text)) {
                strong_self->_terminalFailure = YES;
                [strong_self closeTransport];
                return;
            }
            if (text && signaling_metadata_parse_dimensions(text, strong_self->_config.source_id, &width, &height)) {
                strong_self->_reconnectAttempts = 0;
                strong_self->_callback(width, height, strong_self->_context);
            }
            [strong_self receiveNext];
        });
    }];
}

- (void)connect
{
    if (_stopped)
        return;

    [self closeTransport];
    NSString *url_string =
        [NSString stringWithFormat:@"wss://streamassistant.app/signaling?session=%s", _config.session_id];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:url_string]];
    [request setValue:@"https://camera.streamassistant.app" forHTTPHeaderField:@"Origin"];

    NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    _session = [NSURLSession sessionWithConfiguration:configuration];
    _task = [_session webSocketTaskWithRequest:request];
    [_task resume];

    NSString *join = [NSString stringWithFormat:@"{\"type\":\"media-metadata-join\",\"sessionId\":\"%s\","
                                                @"\"sourceId\":\"%s\",\"secret\":\"%s\",\"peerId\":\"metadata_%s\"}",
                                                _config.session_id, _config.source_id, _config.viewer_secret,
                                                _config.source_id];
    NSURLSessionWebSocketMessage *message = [[NSURLSessionWebSocketMessage alloc] initWithString:join];
    __weak StreamAssistantMetadataConnection *weak_self = self;
    [_task sendMessage:message completionHandler:^(NSError *error) {
        StreamAssistantMetadataConnection *strong_self = weak_self;
        if (!strong_self)
            return;

        dispatch_async(strong_self->_queue, ^{
            if (strong_self->_stopped)
                return;
            if (error)
                [strong_self scheduleReconnect];
            else
                [strong_self receiveNext];
        });
    }];
}

- (void)start
{
    __weak StreamAssistantMetadataConnection *weak_self = self;
    dispatch_async(_queue, ^{
        StreamAssistantMetadataConnection *strong_self = weak_self;
        [strong_self connect];
    });
}

- (void)stop
{
    void (^stop_block)(void) = ^{
        if (self->_stopped)
            return;
        self->_stopped = YES;
        if (self->_reconnect) {
            dispatch_block_cancel(self->_reconnect);
            self->_reconnect = nil;
        }
        [self closeTransport];
    };

    if (dispatch_get_specific(metadata_queue_key))
        stop_block();
    else
        dispatch_sync(_queue, stop_block);
}

- (BOOL)matchesConfig:(const struct camera_config *)config
{
    return config && strcmp(_config.session_id, config->session_id) == 0 &&
           strcmp(_config.source_id, config->source_id) == 0 &&
           strcmp(_config.viewer_secret, config->viewer_secret) == 0;
}

@end

struct signaling_metadata_client {
    void *connection;
};

struct signaling_metadata_client *signaling_metadata_start(const struct camera_config *config,
                                                           signaling_metadata_callback_t callback, void *context)
{
    if (!camera_config_is_valid(config) || !callback)
        return NULL;

    struct signaling_metadata_client *client = calloc(1, sizeof(*client));
    if (!client)
        return NULL;

    StreamAssistantMetadataConnection *connection = [[StreamAssistantMetadataConnection alloc] initWithConfig:config
                                                                                                     callback:callback
                                                                                                      context:context];
    if (!connection) {
        free(client);
        return NULL;
    }

    client->connection = (__bridge_retained void *) connection;
    [connection start];
    return client;
}

void signaling_metadata_stop(struct signaling_metadata_client *client)
{
    if (!client)
        return;

    StreamAssistantMetadataConnection *connection =
        (__bridge_transfer StreamAssistantMetadataConnection *) client->connection;
    [connection stop];
    client->connection = NULL;
    free(client);
}

bool signaling_metadata_matches(const struct signaling_metadata_client *client, const struct camera_config *config)
{
    if (!client || !client->connection)
        return false;
    StreamAssistantMetadataConnection *connection = (__bridge StreamAssistantMetadataConnection *) client->connection;
    return [connection matchesConfig:config];
}
