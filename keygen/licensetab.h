// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;

class LicenseTab : public QWidget {
    Q_OBJECT

public:
    explicit LicenseTab(QWidget* parent = nullptr);

private slots:
    void onEmailChanged();
    void onGenerate();

private:
    void updateStatus(const QString& text, bool isError = false);
    QString resolveLicensePath() const;

    QLineEdit* m_licenseeEdit   = nullptr;
    QLineEdit* m_emailEdit      = nullptr;
    QLineEdit* m_tagsEdit       = nullptr;
    QLabel*    m_filenameLabel  = nullptr;
    QLabel*    m_statusLabel    = nullptr;
};
