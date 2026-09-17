/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>

// Owns one balanced Quartz hide request. All operations run on the Deskflow
// event thread; only acceptsParkingEvent() is also read by the Quartz tap.
class OSXCursorController
{
public:
  class Backend
  {
  public:
    virtual ~Backend() = default;
    virtual bool hideCursor() = 0;
    virtual bool showCursor() = 0;
    virtual void parkCursor(int64_t token) = 0;
    virtual void captureMouse(bool capture) = 0;
  };

  OSXCursorController(Backend &backend, bool primary) : m_backend(backend), m_primary(primary)
  {
  }

  void leave()
  {
    if (m_away)
      return;
    m_away = true;
    m_parking = Parking::Pending;
    m_token = kParkingTag | (++m_generation & 0xffffffffULL);
    if (!m_hidden)
      m_hidden = m_backend.hideCursor();
    m_backend.captureMouse(m_primary);
    // A real, tagged move dismisses hover. A warp alone generates no event.
    m_backend.parkCursor(m_token.load());
  }

  void enter()
  {
    m_token = 0; // Reject late moves and completions before showing the cursor.
    m_away = false;
    m_parking = Parking::Settled;
    if (m_hidden && m_backend.showCursor())
      m_hidden = false;
    m_backend.captureMouse(false);
  }

  bool parkingDelivered(int64_t token)
  {
    if (!acceptsParkingEvent(token) || m_parking != Parking::Pending)
      return false;
    m_parking = Parking::Delivered;
    return true;
  }

  void settle(int64_t token)
  {
    if (!acceptsParkingEvent(token) || m_parking != Parking::Delivered)
      return;
    m_parking = Parking::Settled;
    // Reassert after the move traverses our tap, without accumulating hides.
    // A successful API call is not proof that the Dock has finished rendering.
    if (m_hidden) {
      if (!m_backend.showCursor())
        return;
      m_hidden = false;
    }
    m_hidden = m_backend.hideCursor();
    // In particular, delayed settling must not release a server's capture.
  }

  bool acceptsParkingEvent(int64_t token) const
  {
    return isParkingToken(token) && token == m_token.load();
  }

  static bool isParkingToken(int64_t token)
  {
    return (token & 0xffffffff00000000ULL) == kParkingTag;
  }

  static double settleDelay(const char *raw)
  {
    if (raw != nullptr) {
      char *end = nullptr;
      const double ms = std::strtod(raw, &end);
      if (end != raw && *end == '\0' && std::isfinite(ms) && ms >= 0 && ms <= 2000)
        return ms / 1000;
    }
    return 0.010;
  }

private:
  enum class Parking
  {
    Pending,
    Delivered,
    Settled
  };
  static constexpr int64_t kParkingTag = 0x44464c5700000000LL;
  Backend &m_backend;
  const bool m_primary;
  bool m_away = false;
  bool m_hidden = false;
  Parking m_parking = Parking::Settled;
  uint64_t m_generation = 0;
  std::atomic<int64_t> m_token{0};
};
