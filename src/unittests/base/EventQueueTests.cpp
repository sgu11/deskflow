/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "EventQueueTests.h"

#include "base/EventQueue.h"

#include <QTest>

#include <chrono>
#include <memory>

void EventQueueTests::initTestCase()
{
  m_arch.init();
}

void EventQueueTests::clock_preservesFractionalSeconds()
{
  // Bracket the public clock with its monotonic source. Integer-second
  // truncation must fail regardless of whether a sleep crosses a second.
  const auto seconds = [] {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  };
  const double before = seconds();
  const double actual = Arch::time();
  const double after = seconds();
  QVERIFY2(actual >= before && actual <= after, "Arch::time lost subsecond precision");
}

void EventQueueTests::shortTimer_firesWithoutWaitingForWholeSecond()
{
  EventQueue events;
  const auto start = std::chrono::steady_clock::now();
  auto *timer = events.newOneShotTimer(0.010, nullptr);
  Event event;
  const bool received = events.getEvent(event, 0.25);
  const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  events.deleteTimer(timer);
  QVERIFY(received);
  QCOMPARE(event.getType(), EventTypes::Timer);
  QVERIFY2(elapsed >= 0.009 && elapsed < 0.25, "10ms timer did not fire within its subsecond deadline");
}

void EventQueueTests::dispatchEvent_noHandler_returnsFalse()
{
  EventQueue events;

  QVERIFY(!events.dispatchEvent(Event(EventTypes::ClientDisconnected, this)));
}

void EventQueueTests::dispatchEvent_noTypeHandler_dispatchesUnknownHandler()
{
  EventQueue events;
  bool fallbackCalled = false;
  events.addHandler(EventTypes::Unknown, this, [&fallbackCalled](const Event &) { fallbackCalled = true; });

  QVERIFY(events.dispatchEvent(Event(EventTypes::ClientDisconnected, this)));
  QVERIFY(fallbackCalled);
}

void EventQueueTests::dispatchEvent_handlerRemovesItself_keepsHandlerAliveUntilReturn()
{
  EventQueue events;
  auto handlerLifetime = std::make_shared<int>(1);
  std::weak_ptr<int> handlerLifetimeObserver = handlerLifetime;
  bool handlerAliveAfterRemoval = false;

  events.addHandler(
      EventTypes::ClientDisconnected, this,
      [this, &events, &handlerLifetimeObserver, &handlerAliveAfterRemoval, handlerLifetime](const Event &) {
        events.removeHandler(EventTypes::ClientDisconnected, this);
        handlerAliveAfterRemoval = handlerLifetime != nullptr && !handlerLifetimeObserver.expired();
      }
  );
  handlerLifetime.reset();

  QVERIFY(events.dispatchEvent(Event(EventTypes::ClientDisconnected, this)));
  QVERIFY(handlerAliveAfterRemoval);
  QVERIFY(handlerLifetimeObserver.expired());
}

QTEST_MAIN(EventQueueTests)
