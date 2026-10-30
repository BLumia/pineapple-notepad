// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;

class VerifyTab : public QWidget {
    Q_OBJECT

public:
    explicit VerifyTab(QWidget* parent = nullptr);

private slots:
    void onBrowse();
    void onVerify();

private:
    void updateStatus(const QString& text, bool isError = false);

    QLineEdit* m_pathEdit   = nullptr;
    QLabel*    m_statusLabel = nullptr;
};
