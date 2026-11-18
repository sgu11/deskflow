/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/ipc/IpcClient.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTest>
#include <QUuid>

using deskflow::gui::ipc::IpcClient;

class IpcClientTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void cancelRetryLeavesClientDisconnected();
  void retryConnectsWhenServerAppears();
  void reconnectDiscardsIncompleteFrame();
  void peerCloseDuringHandshakeReportsFailure();
  void missingHelloReportsFailure();
  void mismatchedCoreCanBeStopped();
};

void IpcClientTests::cancelRetryLeavesClientDisconnected()
{
  const auto name = QUuid::createUuid().toString(QUuid::Id128);
  IpcClient client(this, name, QStringLiteral("test"));
  QLocalServer server;
  QSignalSpy connections(&server, &QLocalServer::newConnection);
  QSignalSpy errors(client.findChild<QLocalSocket *>(), &QLocalSocket::errorOccurred);

  client.connectToServer();
  QTRY_VERIFY(!errors.isEmpty());
  client.disconnectFromServer();
  QVERIFY(server.listen(name));
  QVERIFY(!connections.wait(1000));
}

void IpcClientTests::retryConnectsWhenServerAppears()
{
  const auto name = QUuid::createUuid().toString(QUuid::Id128);
  IpcClient client(this, name, QStringLiteral("test"));
  QLocalServer server;
  QSignalSpy connected(&client, &IpcClient::connected);

  client.connectToServer();
  QVERIFY(server.listen(name));
  QTRY_VERIFY(server.hasPendingConnections());
  auto *peer = server.nextPendingConnection();
  QTRY_VERIFY(peer->canReadLine());
  QVERIFY(peer->readLine().startsWith("hello="));
  peer->write("hello=test\n");
  QTRY_COMPARE(connected.size(), 1);
  QVERIFY(client.isConnected());
}

void IpcClientTests::reconnectDiscardsIncompleteFrame()
{
  const auto name = QUuid::createUuid().toString(QUuid::Id128);
  IpcClient client(this, name, QStringLiteral("test"));
  QLocalServer server;
  QSignalSpy connected(&client, &IpcClient::connected);
  QSignalSpy oldDataRead(client.findChild<QLocalSocket *>(), &QLocalSocket::readyRead);

  QVERIFY(server.listen(name));
  client.connectToServer();
  QTRY_VERIFY(server.hasPendingConnections());
  auto *firstPeer = server.nextPendingConnection();
  QTRY_VERIFY(firstPeer->canReadLine());
  firstPeer->write("hello=test\n");
  QTRY_COMPARE(connected.size(), 1);
  oldDataRead.clear();
  firstPeer->write("unfinished");
  QTRY_COMPARE(oldDataRead.size(), 1);

  client.disconnectFromServer();
  client.connectToServer();
  QTRY_VERIFY(server.hasPendingConnections());
  auto *secondPeer = server.nextPendingConnection();
  QTRY_VERIFY(secondPeer->canReadLine());
  secondPeer->write("hello=test\n");
  QTRY_COMPARE(connected.size(), 2);
}

void IpcClientTests::peerCloseDuringHandshakeReportsFailure()
{
  const auto name = QUuid::createUuid().toString(QUuid::Id128);
  IpcClient client(this, name, QStringLiteral("test"));
  QLocalServer server;
  QSignalSpy failed(&client, &IpcClient::connectionFailed);

  QVERIFY(server.listen(name));
  client.connectToServer();
  QTRY_VERIFY(server.hasPendingConnections());
  auto *peer = server.nextPendingConnection();
  QTRY_VERIFY(peer->canReadLine());
  peer->disconnectFromServer();
  QTRY_COMPARE(failed.size(), 1);
  QVERIFY(!client.isConnected());
  client.connectToServer();
  QTRY_VERIFY(server.hasPendingConnections());
}

void IpcClientTests::missingHelloReportsFailure()
{
  const auto name = QUuid::createUuid().toString(QUuid::Id128);
  IpcClient client(this, name, QStringLiteral("test"));
  QLocalServer server;
  QSignalSpy failed(&client, &IpcClient::connectionFailed);

  QVERIFY(server.listen(name));
  client.connectToServer();
  QTRY_VERIFY(server.hasPendingConnections());
  auto *peer = server.nextPendingConnection();
  QTRY_VERIFY(peer->canReadLine());
  QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 7000);
  QVERIFY(!client.isConnected());
}

void IpcClientTests::mismatchedCoreCanBeStopped()
{
  class StoppableClient : public IpcClient
  {
  public:
    using IpcClient::IpcClient;
    using IpcClient::sendMessage;
  };

  const auto name = QUuid::createUuid().toString(QUuid::Id128);
  StoppableClient client(this, name, QStringLiteral("test"));
  QLocalServer server;
  QSignalSpy mismatch(&client, &IpcClient::versionMismatch);
  QSignalSpy shutdown(&client, &IpcClient::serverShutdown);
  QSignalSpy failed(&client, &IpcClient::connectionFailed);

  QVERIFY(server.listen(name));
  client.connectToServer();
  QTRY_VERIFY(server.hasPendingConnections());
  auto *peer = server.nextPendingConnection();
  QTRY_VERIFY(peer->canReadLine());
  peer->readLine();

  peer->write("versionMismatch=old");
  QSignalSpy read(client.findChild<QLocalSocket *>(), &QLocalSocket::readyRead);
  QTRY_COMPARE(read.size(), 1);
  QCOMPARE(mismatch.size(), 0);
  peer->write("\n");
  QTRY_COMPARE(mismatch.size(), 1);

  client.sendMessage(QStringLiteral("stop"));
  QTRY_VERIFY(peer->canReadLine());
  QCOMPARE(peer->readLine(), QByteArray("stop\n"));
  peer->write("bye\n");
  QTRY_COMPARE(shutdown.size(), 1);
  QCOMPARE(failed.size(), 0);
  QVERIFY(!client.isConnected());
}

QTEST_GUILESS_MAIN(IpcClientTests)
#include "IpcClientTests.moc"
