/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <ApplicationServices/ApplicationServices.h>
#include <cstdint>

// Separate the incoming device delta from the event delivered to local apps.
// Never warp or post another event here: that can feed relocation back into
// the input stream. Returning the modified event lets Dock observe hover exit.
struct OSXServerCursorMotion
{
  int32_t dx;
  int32_t dy;

  static OSXServerCursorMotion redirect(CGEventRef event, CGPoint anchor)
  {
    const OSXServerCursorMotion motion{
        static_cast<int32_t>(CGEventGetIntegerValueField(event, kCGMouseEventDeltaX)),
        static_cast<int32_t>(CGEventGetIntegerValueField(event, kCGMouseEventDeltaY))
    };
    // Remote button drags must not become local drags.
    CGEventSetType(event, kCGEventMouseMoved);
    CGEventSetLocation(event, anchor);
    CGEventSetIntegerValueField(event, kCGMouseEventDeltaX, 0);
    CGEventSetIntegerValueField(event, kCGMouseEventDeltaY, 0);
    return motion;
  }
};
