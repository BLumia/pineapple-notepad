// SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#ifndef SINGLEAPPLICATIONMANAGER_H
#define SINGLEAPPLICATIONMANAGER_H

#include <QObject>

QT_BEGIN_NAMESPACE
class QLocalServer;
QT_END_NAMESPACE

class SingleApplicationManager : public QObject
{
    Q_OBJECT
public:
    explicit SingleApplicationManager(QString applicationKey, QObject *parent = nullptr);

    void createSingleInstance();
    bool checkSingleInstance(QVariant data);

signals:
    void dataReached(QVariant data);

private slots:
    void on_localSocket_newConnection();

private:

    QString m_applicationKey;
    QLocalServer * m_localServer;

};

#endif // SINGLEAPPLICATIONMANAGER_H
