/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>

namespace deskflow::windows {

// Win32 virtual-key values, kept independent of Windows.h for portable tests.
// Apply after forwarding the event, and after the fake-local-input bypass.
constexpr bool suppressLocalKey(bool relaying, uint32_t virtualKey)
{
  constexpr uint32_t capsLock = 0x14;
  constexpr uint32_t numLock = 0x90;
  constexpr uint32_t scrollLock = 0x91;
  // Only LED lock keys pass through while controlling a remote screen.
  // VK_HANGUL/VK_KANA (0x15) must not change the server's input mode.
  return relaying && virtualKey != capsLock && virtualKey != numLock && virtualKey != scrollLock;
}

} // namespace deskflow::windows
