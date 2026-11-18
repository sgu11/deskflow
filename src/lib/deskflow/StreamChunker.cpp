/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2013 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/StreamChunker.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/ClipboardChunk.h"

static constexpr size_t g_chunkSize = 512 * 1024;

void StreamChunker::sendClipboard(
    std::string_view data, ClipboardID id, uint32_t sequence, IEventQueue *events, void *eventTarget
)
{
  // send first message (data size)
  const size_t size = data.size();
  std::string dataSize = QString::number(size).toStdString();
  ClipboardChunk *sizeMessage = ClipboardChunk::start(id, sequence, dataSize);

  events->addEvent(Event(EventTypes::ClipboardSending, eventTarget, sizeMessage));

  // Preserve the data frame for an empty clipboard.
  do {
    const auto chunk = data.substr(0, g_chunkSize);
    auto *dataChunk = ClipboardChunk::data(id, sequence, chunk);
    events->addEvent(Event(EventTypes::ClipboardSending, eventTarget, dataChunk));
    data.remove_prefix(chunk.size());
  } while (!data.empty());

  // send last message
  ClipboardChunk *end = ClipboardChunk::end(id, sequence);

  events->addEvent(Event(EventTypes::ClipboardSending, eventTarget, end));

  LOG_DEBUG("sent clipboard size=%zu", size);
}
