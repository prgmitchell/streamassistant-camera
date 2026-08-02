/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "platform-services.h"

#import <AppKit/AppKit.h>
#import <Security/Security.h>

bool camera_platform_random_bytes(void *buffer, size_t size)
{
    return buffer && SecRandomCopyBytes(kSecRandomDefault, size, buffer) == errSecSuccess;
}

bool camera_platform_open_url(const char *url)
{
    if (!url)
        return false;

    __block bool opened = false;
    void (^open_block)(void) = ^{
        NSString *value = [NSString stringWithUTF8String:url];
        NSURL *target = value ? [NSURL URLWithString:value] : nil;
        opened = target && [[NSWorkspace sharedWorkspace] openURL:target];
    };

    if ([NSThread isMainThread])
        open_block();
    else
        dispatch_sync(dispatch_get_main_queue(), open_block);

    return opened;
}

bool camera_platform_confirm_pairing_reset(void)
{
    __block bool confirmed = false;
    void (^show_prompt)(void) = ^{
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = @"Reset StreamAssistant Camera pairing?";
        alert.informativeText = @"The existing phone link will stop working.";
        alert.alertStyle = NSAlertStyleWarning;
        [alert addButtonWithTitle:@"Reset Pairing"];
        [alert addButtonWithTitle:@"Cancel"];
        confirmed = [alert runModal] == NSAlertFirstButtonReturn;
    };

    if ([NSThread isMainThread])
        show_prompt();
    else
        dispatch_sync(dispatch_get_main_queue(), show_prompt);

    return confirmed;
}

void camera_platform_secure_zero(void *buffer, size_t size)
{
    volatile unsigned char *bytes = buffer;
    while (bytes && size-- > 0)
        *bytes++ = 0;
}
