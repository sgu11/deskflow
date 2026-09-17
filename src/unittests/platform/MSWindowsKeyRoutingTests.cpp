/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsKeyRouting.h"

#include <QTest>

#ifdef Q_OS_WIN
#include <Windows.h>
static_assert(VK_HANGUL == 0x15 && VK_KANA == 0x15);
static_assert(VK_CAPITAL == 0x14 && VK_NUMLOCK == 0x90 && VK_SCROLL == 0x91);
#endif

class MSWindowsKeyRoutingTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void localInputPassesThrough()
  {
    for (uint32_t key = 0; key < 256; ++key)
      QVERIFY(!deskflow::windows::suppressLocalKey(false, key));
  }

  void remoteImeAndOrdinaryKeysAreSuppressed()
  {
    // Hangul/Kana, Hanja, Space, both Shifts, and an ordinary letter.
    for (uint32_t key : {0x15u, 0x19u, 0x20u, 0xa0u, 0xa1u, 0x41u})
      QVERIFY(deskflow::windows::suppressLocalKey(true, key));
  }

  void remoteLedLockKeysPassThrough()
  {
    for (uint32_t key : {0x14u, 0x90u, 0x91u})
      QVERIFY(!deskflow::windows::suppressLocalKey(true, key));
  }

  void returningToServerRestoresImeRouting()
  {
    QVERIFY(!deskflow::windows::suppressLocalKey(false, 0x15));
    QVERIFY(deskflow::windows::suppressLocalKey(true, 0x15));
    QVERIFY(deskflow::windows::suppressLocalKey(true, 0x15));
    QVERIFY(!deskflow::windows::suppressLocalKey(false, 0x15));
  }
};

QTEST_GUILESS_MAIN(MSWindowsKeyRoutingTests)
#include "MSWindowsKeyRoutingTests.moc"
