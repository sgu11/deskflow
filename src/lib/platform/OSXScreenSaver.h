/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2004 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/IScreenSaver.h"

class IEventQueue;
class EventQueueTimer;

//! OSX screen saver implementation
class OSXScreenSaver : public IScreenSaver
{
public:
  OSXScreenSaver(IEventQueue *events, void *eventTarget);
  ~OSXScreenSaver();

  // Monitor session locking on the server. Clients do not publish lock state.
  void watchLock(bool notify);

  // IScreenSaver overrides
  void enable() override;
  void disable() override;
  void activate() override;
  void deactivate() override;
  bool isActive() const override;

private:
  void checkLock();

private:
  // the target for the events we generate
  void *m_eventTarget;

  bool m_enabled;
  void *m_screenSaverController;
  void *m_autoReleasePool;
  EventQueueTimer *m_lockTimer = nullptr;
  bool m_locked = false;
  IEventQueue *m_events;
};
