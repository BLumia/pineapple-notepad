// SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#include "singleapplicationmanager.h"

#include <QVariant>
#include <QDataStream>
#include <QLocalServer>
#include <QLocalSocket>

SingleApplicationManager::SingleApplicationManager(QString applicationKey, QObject *parent)
    : QObject(parent)
    , m_applicationKey(applicationKey)
{

}

void SingleApplicationManager::on_localSocket_newConnection()
{
    QScopedPointer<QLocalSocket> socket(m_localServer->nextPendingConnection());
    if (socket) {
        socket->waitForReadyRead(500);

        QDataStream dataStream(socket.data());
        QVariant data;
        dataStream.startTransaction();
        dataStream >> data;
        dataStream.commitTransaction();
        emit dataReached(data);
    }
}

void SingleApplicationManager::createSingleInstance()
{
    m_localServer = new QLocalServer(this);

    connect(m_localServer, &QLocalServer::newConnection, this, &SingleApplicationManager::on_localSocket_newConnection);

    if (!m_localServer->listen(m_applicationKey)) {
        if (m_localServer->serverError() == QAbstractSocket::AddressInUseError) {
            QLocalServer::removeServer(m_applicationKey);
            m_localServer->listen(m_applicationKey);
        }
    }
}

bool SingleApplicationManager::checkSingleInstance(QVariant data)
{
    QLocalSocket socket;
    socket.connectToServer(m_applicationKey);
    if (socket.waitForConnected(500)) {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out << data;

        socket.write(block);
        socket.waitForBytesWritten();
        socket.flush();
        socket.disconnectFromServer();

        return true;
    }

    return false;
}
