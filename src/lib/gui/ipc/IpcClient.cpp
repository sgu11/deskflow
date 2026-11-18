/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "IpcClient.h"

#include "common/VersionInfo.h"

#include <QDebug>
#include <QLocalSocket>

#include <chrono>
#include <utility>

using namespace std::chrono_literals;

namespace deskflow::gui::ipc {

IpcClient::IpcClient(QObject *parent, const QString &socketName, const QString &typeName)
    : QObject(parent),
      m_socket{new QLocalSocket(this)},
      m_socketName(socketName), // NOSONAR - Qt memory
      m_typeName(typeName)
{
  connect(m_socket, &QLocalSocket::disconnected, this, &IpcClient::handleDisconnected);
  connect(m_socket, &QLocalSocket::errorOccurred, this, &IpcClient::handleErrorOccurred);
  connect(m_socket, &QLocalSocket::readyRead, this, &IpcClient::handleReadyRead);
  connect(m_socket, &QLocalSocket::connected, this, &IpcClient::handleConnectionSuccess);

  m_connectionTimer.setSingleShot(true);
  connect(&m_connectionTimer, &QTimer::timeout, this, [this] {
    if (m_state == State::RetryPending) {
      attemptConnection();
    } else if (m_state == State::AwaitingHello) {
      qWarning().noquote() << QStringLiteral("%1 ipc server did not respond to hello").arg(m_typeName);
      failConnection();
    }
  });
}

void IpcClient::connectToServer()
{
  if (m_state == State::Connecting || m_state == State::AwaitingHello) {
    qWarning().noquote() << QStringLiteral("%1 ipc client already connecting to server").arg(m_typeName);
    return;
  }

  if (m_state != State::Unconnected || m_socket->state() != QLocalSocket::UnconnectedState) {
    qDebug().noquote() << QStringLiteral("%1 ipc client not in unconnected state, disconnecting").arg(m_typeName);
    disconnectFromServer();
  }

  m_retryCount = 0;
  attemptConnection();
}

void IpcClient::attemptConnection()
{
  if (const int retryLimit = 10; m_retryCount >= retryLimit) {
    qWarning().noquote() << QStringLiteral("%1 ipc client failed to connect after %2 attempts")
                                .arg(m_typeName, QString::number(retryLimit));
    m_state = State::Unconnected;
    Q_EMIT connectionFailed();
    return;
  }

  if (m_retryCount == 0) {
    qDebug().noquote() << QStringLiteral("%1 ipc client connecting to server: %2").arg(m_typeName, m_socketName);
  } else {
    qDebug().noquote() << QStringLiteral("%1 ipc client retrying connection, attempt: %2")
                              .arg(m_typeName, QString::number(m_retryCount + 1));
  }

  m_state = State::Connecting;
  m_retryCount++;

  m_socket->connectToServer(m_socketName);
}

void IpcClient::disconnectFromServer()
{
  m_state = State::Disconnecting;
  m_connectionTimer.stop();
  m_readBuffer.clear();
  qDebug().noquote() << QStringLiteral("%1 ipc client disconnecting from server").arg(m_typeName);
  m_socket->abort();
  m_state = State::Unconnected;
}

void IpcClient::handleDisconnected()
{
  if (m_state == State::AwaitingHello || m_state == State::Connected) {
    qDebug().noquote() << QStringLiteral("%1 ipc client disconnected from server").arg(m_typeName);
    failConnection();
  }
}

void IpcClient::handleErrorOccurred()
{
  if (m_state == State::Connecting) {
    qWarning().noquote()
        << QStringLiteral("%1 ipc client failed to connect: %2").arg(m_typeName, m_socket->errorString());
    m_state = State::RetryPending;
    m_connectionTimer.start(500ms);
    return;
  }

  qWarning().noquote() << QStringLiteral("%1 ipc client error: %2").arg(m_typeName, m_socket->errorString());

  if (m_state == State::AwaitingHello || m_state == State::Connected) {
    failConnection();
  }
}

void IpcClient::handleReadyRead()
{
  QByteArray data = std::exchange(m_readBuffer, {});
  data.append(m_socket->readAll());

  qsizetype start = 0;
  while (m_state == State::AwaitingHello || m_state == State::Connected) {
    const auto end = data.indexOf('\n', start);
    if (end < 0) {
      m_readBuffer = data.mid(start);
      return;
    }
    const auto message = QString::fromUtf8(data.constData() + start, end - start);
    start = end + 1;
    qDebug().noquote() << QStringLiteral("%1 ipc client message: %2").arg(m_typeName, message);
    const auto parts = message.split('=');

    if (m_state == State::AwaitingHello) {
      handleHandshakeMessage(parts);
      continue;
    }

    if (parts.at(0) == QStringLiteral("bye")) {
      qDebug().noquote() << QStringLiteral("%1 ipc server is shutting down").arg(m_typeName);
      disconnectFromServer();
      Q_EMIT serverShutdown();
      return;
    }

    processCommand(parts.at(0), parts);
  }
}

void IpcClient::handleHandshakeMessage(const QStringList &parts)
{
  if (parts.at(0) == QStringLiteral("error")) {
    const auto detail = parts.size() >= 2 ? parts.at(1) : QStringLiteral("unknown");
    qCritical().noquote() << QStringLiteral("%1 ipc server rejected connection: %2").arg(m_typeName, detail);
    failConnection();
    return;
  }

  const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);

  if (parts.at(0) == QStringLiteral("versionMismatch")) {
    const auto serverVersion = parts.size() >= 2 ? parts.at(1) : QStringLiteral("unknown");
    qWarning().noquote()
        << QStringLiteral("%1 ipc version mismatch (client: %2, server: %3)").arg(m_typeName, versionId, serverVersion);
    m_connectionTimer.stop();
    m_state = State::Connected;
    Q_EMIT versionMismatch();
    return;
  }

  if (parts.at(0) != QStringLiteral("hello")) {
    return;
  }

  if (parts.size() < 2) {
    qCritical().noquote() << QStringLiteral("%1 ipc server hello missing version").arg(m_typeName);
    failConnection();
    return;
  }

  m_connectionTimer.stop();
  m_state = State::Connected;
  qDebug().noquote() << QStringLiteral("%1 ipc client connected").arg(m_typeName);
  Q_EMIT connected();
}

void IpcClient::failConnection()
{
  disconnectFromServer();
  Q_EMIT connectionFailed();
}

void IpcClient::handleConnectionSuccess()
{
  m_state = State::AwaitingHello;
  m_connectionTimer.start(5s);
  const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
  m_socket->write(QStringLiteral("hello=%1\n").arg(versionId).toUtf8());
  qDebug().noquote() << QStringLiteral("%1 ipc client sent hello with version: %2").arg(m_typeName, versionId);
}

void IpcClient::sendMessage(const QString &message)
{
  if (m_state != State::Connected) {
    qWarning().noquote() << QStringLiteral("%1 cannot send command, ipc client not connected").arg(m_typeName);
    return;
  }

  m_socket->write(message.toUtf8() + "\n");
  qDebug().noquote() << QStringLiteral("%1 ipc client sent message: %2").arg(m_typeName, message);
}

} // namespace deskflow::gui::ipc
