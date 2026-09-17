/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXCursorController.h"
#include "platform/OSXServerCursorMotion.h"

#include <ApplicationServices/ApplicationServices.h>
#include <QStringList>
#include <QTest>
#include <unistd.h>

namespace {
class FakeCursor : public OSXCursorController::Backend
{
public:
  bool hideCursor() override
  {
    calls << "hide";
    if (hideSucceeds)
      ++depth;
    return hideSucceeds;
  }
  bool showCursor() override
  {
    calls << "show";
    if (showSucceeds)
      --depth;
    return showSucceeds;
  }
  void parkCursor(int64_t value) override
  {
    calls << "park";
    token = value;
  }
  void captureMouse(bool value) override
  {
    calls << (value ? "capture" : "release");
    captured = value;
  }
  QStringList calls;
  int depth = 0;
  int64_t token = 0;
  bool captured = false;
  bool hideSucceeds = true;
  bool showSucceeds = true;
};
} // namespace

class OSXCursorControllerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void parkingEvent_metadataSurvivesConstruction_withoutPostingInput()
  {
    FakeCursor backend;
    OSXCursorController cursor(backend, false);
    cursor.leave();
    auto event = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, CGPointMake(500, 500), kCGMouseButtonLeft);
    QVERIFY(event != nullptr);
    CGEventSetIntegerValueField(event, kCGEventSourceUserData, backend.token);
    CGEventSetIntegerValueField(event, kCGMouseEventDeltaX, 0);
    CGEventSetIntegerValueField(event, kCGMouseEventDeltaY, 0);
    const auto pid = CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID);
    const auto token = CGEventGetIntegerValueField(event, kCGEventSourceUserData);
    const auto dx = CGEventGetIntegerValueField(event, kCGMouseEventDeltaX);
    const auto dy = CGEventGetIntegerValueField(event, kCGMouseEventDeltaY);
    CFRelease(event); // Never post to the desktop.
    QCOMPARE(pid, static_cast<int64_t>(getpid()));
    QVERIFY(cursor.acceptsParkingEvent(token));
    QCOMPARE(dx, int64_t(0));
    QCOMPARE(dy, int64_t(0));
  }

  void serverLeave_neverInjects_andSettlesWithoutCapture()
  {
    FakeCursor backend;
    OSXCursorController cursor(backend, true);
    cursor.leave();
    QCOMPARE(backend.calls, QStringList({"hide", "release"}));
    const auto token = cursor.parkingToken();
    QVERIFY(cursor.acceptsParkingEvent(token));
    QVERIFY(cursor.parkingDelivered(token));
    cursor.settle(token);
    cursor.leave();
    QCOMPARE(backend.calls, QStringList({"hide", "release", "show", "hide"}));
    QCOMPARE(backend.depth, 1);
    QVERIFY(!backend.captured);
    cursor.enter();
    QVERIFY(!cursor.acceptsParkingEvent(token));
    cursor.settle(token);
    QCOMPARE(backend.depth, 0);
    QVERIFY(!backend.captured);
  }

  void serverMotion_preservesPhysicalDeltas_andRemovesLocalDrag()
  {
    for (const auto type :
         {kCGEventMouseMoved, kCGEventLeftMouseDragged, kCGEventRightMouseDragged, kCGEventOtherMouseDragged}) {
      auto event = CGEventCreateMouseEvent(nullptr, type, CGPointMake(0, 600), kCGMouseButtonLeft);
      QVERIFY(event != nullptr);
      CGEventSetIntegerValueField(event, kCGMouseEventDeltaX, -7);
      CGEventSetIntegerValueField(event, kCGMouseEventDeltaY, 3);
      const auto motion = OSXServerCursorMotion::redirect(event, CGPointMake(-960, 540));
      QCOMPARE(motion.dx, -7);
      QCOMPARE(motion.dy, 3);
      QCOMPARE(CGEventGetType(event), kCGEventMouseMoved);
      QCOMPARE(CGEventGetLocation(event).x, -960.0);
      QCOMPARE(CGEventGetLocation(event).y, 540.0);
      QCOMPARE(CGEventGetIntegerValueField(event, kCGMouseEventDeltaX), int64_t(0));
      QCOMPARE(CGEventGetIntegerValueField(event, kCGMouseEventDeltaY), int64_t(0));
      // No arbitrary delta threshold: fast physical motion must remain intact.
      CGEventSetIntegerValueField(event, kCGMouseEventDeltaX, 2400);
      QCOMPARE(OSXServerCursorMotion::redirect(event, CGPointMake(-960, 540)).dx, 2400);
      CFRelease(event); // Construction only; never inject input in unit tests.
    }
  }

  void clientLeave_doesNotCaptureHardware()
  {
    FakeCursor backend;
    OSXCursorController cursor(backend, false);
    cursor.leave();
    QCOMPARE(backend.calls, QStringList({"hide", "release", "park"}));
    QVERIFY(!backend.captured);
    cursor.enter();
    QCOMPARE(backend.depth, 0);
  }

  void fastReentry_rejectsOldMove_andOldCompletion()
  {
    FakeCursor backend;
    OSXCursorController cursor(backend, false);
    cursor.leave();
    const auto oldToken = backend.token;
    QVERIFY(cursor.parkingDelivered(oldToken));
    cursor.enter();
    QVERIFY(!cursor.acceptsParkingEvent(oldToken));
    cursor.settle(oldToken);
    QCOMPARE(backend.depth, 0);
    cursor.leave();
    QVERIFY(backend.token != oldToken);
    QVERIFY(!cursor.parkingDelivered(oldToken));
    const auto before = backend.calls;
    cursor.settle(oldToken);
    QCOMPARE(backend.calls, before);
    QVERIFY(cursor.acceptsParkingEvent(backend.token));
    QVERIFY(!cursor.acceptsParkingEvent(0));
  }

  void duplicateEvents_keepOneHideRequest()
  {
    FakeCursor backend;
    OSXCursorController cursor(backend, false);
    cursor.leave();
    const auto before = backend.calls;
    cursor.leave();
    QCOMPARE(backend.calls, before);
    cursor.settle(backend.token); // not delivered yet
    QCOMPARE(backend.calls, before);
    QVERIFY(cursor.parkingDelivered(backend.token));
    QVERIFY(!cursor.parkingDelivered(backend.token));
    cursor.settle(backend.token);
    QCOMPARE(backend.depth, 1);
    const auto settled = backend.calls;
    cursor.settle(backend.token);
    QVERIFY(!cursor.parkingDelivered(backend.token));
    QCOMPARE(backend.calls, settled);
    cursor.enter();
    cursor.enter();
    QCOMPARE(backend.depth, 0);
  }

  void failedHide_doesNotCreateUnownedShow()
  {
    FakeCursor backend;
    OSXCursorController cursor(backend, false);
    backend.hideSucceeds = false;
    cursor.leave();
    cursor.enter();
    QCOMPARE(backend.depth, 0);
    QVERIFY(!backend.calls.contains("show"));
  }

  void failedInitialHide_retriesAfterParking()
  {
    FakeCursor backend;
    OSXCursorController cursor(backend, false);
    backend.hideSucceeds = false;
    cursor.leave();
    QVERIFY(cursor.parkingDelivered(backend.token));
    backend.hideSucceeds = true;
    cursor.settle(backend.token);
    QCOMPARE(backend.depth, 1);
    QVERIFY(!backend.calls.contains("show"));
    cursor.enter();
    QCOMPARE(backend.depth, 0);
  }

  void failedShow_retainsOwnershipForRetry()
  {
    FakeCursor backend;
    OSXCursorController cursor(backend, false);
    cursor.leave();
    QVERIFY(cursor.parkingDelivered(backend.token));
    backend.showSucceeds = false;
    cursor.settle(backend.token);
    QCOMPARE(backend.depth, 1); // no extra hide after a failed show
    cursor.enter();
    QCOMPARE(backend.depth, 1);
    QVERIFY(!backend.captured);
    backend.showSucceeds = true;
    cursor.enter();
    QCOMPARE(backend.depth, 0);
  }

  void settleDelay_validatesWholeValue_andAcceptsImmediate()
  {
    QCOMPARE(OSXCursorController::settleDelay(nullptr), 0.010);
    QCOMPARE(OSXCursorController::settleDelay("0"), 0.0);
    QCOMPARE(OSXCursorController::settleDelay("100"), 0.1);
    QCOMPARE(OSXCursorController::settleDelay("2000"), 2.0);
    for (const char *bad : {"", "-1", "2001", "100oops", "nan", "inf"})
      QCOMPARE(OSXCursorController::settleDelay(bad), 0.010);
  }
};

QTEST_GUILESS_MAIN(OSXCursorControllerTests)
#include "OSXCursorControllerTests.moc"
