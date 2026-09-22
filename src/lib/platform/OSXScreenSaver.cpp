/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#import "platform/OSXScreenSaver.h"

#import "base/IEventQueue.h"
#import "base/Log.h"
#import "platform/OSXScreenSaverUtil.h"

#include <CoreGraphics/CGSession.h>

//
// OSXScreenSaver
//

OSXScreenSaver::OSXScreenSaver(IEventQueue *events, void *eventTarget)
    : m_eventTarget(eventTarget),
      m_enabled(true),
      m_events(events)
{
  m_autoReleasePool = screenSaverUtilCreatePool();
  m_screenSaverController = screenSaverUtilCreateController();
}

OSXScreenSaver::~OSXScreenSaver()
{
  watchLock(false);
  //    screenSaverUtilReleaseController(m_screenSaverController);
  screenSaverUtilReleasePool(m_autoReleasePool);
}

void OSXScreenSaver::enable()
{
  m_enabled = true;
  screenSaverUtilEnable(m_screenSaverController);
}

void OSXScreenSaver::disable()
{
  m_enabled = false;
  screenSaverUtilDisable(m_screenSaverController);
}

void OSXScreenSaver::activate()
{
  screenSaverUtilActivate(m_screenSaverController);
}

void OSXScreenSaver::deactivate()
{
  screenSaverUtilDeactivate(m_screenSaverController, m_enabled);
}

bool OSXScreenSaver::isActive() const
{
  return (screenSaverUtilIsActive(m_screenSaverController) != 0);
}

void OSXScreenSaver::watchLock(bool notify)
{
  if (m_lockTimer != nullptr) {
    m_events->removeHandler(EventTypes::Timer, m_lockTimer);
    m_events->deleteTimer(m_lockTimer);
    m_lockTimer = nullptr;
  }
  m_locked = false;
  if (notify) {
    // Poll on the core event thread, including when started while locked.
    // ScreenSaverEngine lifetime does not represent modern macOS locking.
    m_lockTimer = m_events->newTimer(0.25, nullptr);
    m_events->addHandler(EventTypes::Timer, m_lockTimer, [this](const auto &) { checkLock(); });
  }
}

void OSXScreenSaver::checkLock()
{
  const auto session = CGSessionCopyCurrentDictionary();
  if (session == nullptr) {
    // Loss of the Quartz session is not evidence of an unlock.
    return;
  }
  // macOS omits this key while unlocked. It is not a public SDK constant;
  // native lock/unlock acceptance is required when upgrading macOS.
  const auto value = CFDictionaryGetValue(session, CFSTR("CGSSessionScreenIsLocked"));
  const bool locked = value != nullptr && CFEqual(value, kCFBooleanTrue);
  CFRelease(session);
  if (locked == m_locked) {
    return;
  }
  m_locked = locked;
  LOG_INFO("macOS session %s; synchronizing clients", locked ? "locked" : "unlocked");
  m_events->addEvent(
      Event(locked ? EventTypes::PrimaryScreenSaverActivated : EventTypes::PrimaryScreenSaverDeactivated, m_eventTarget)
  );
}
