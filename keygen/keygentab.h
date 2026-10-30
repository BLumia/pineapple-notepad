// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;

class KeygenTab : public QWidget {
    Q_OBJECT

public:
    explicit KeygenTab(QWidget* parent = nullptr);

private slots:
    void onGenerateRandomSeed();
    void onGeneratePublicKey();
    void onSaveToFile();

private:
    void updateStatus(const QString& text, bool isError = false);

    QLineEdit* m_seedEdit    = nullptr;
    QLineEdit* m_pubkeyEdit  = nullptr;
    QLabel*    m_statusLabel = nullptr;
};
