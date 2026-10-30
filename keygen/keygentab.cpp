#include "keygentab.h"

#include "licensecrypto.h"
#include "stringhelpers.h"

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

KeygenTab::KeygenTab(QWidget* parent)
    : QWidget(parent) {
    auto* form = new QVBoxLayout(this);

    auto* seedLabel = new QLabel(tr("Seed (32 bytes, 64 hex chars):"), this);
    form->addWidget(seedLabel);

    m_seedEdit = new QLineEdit(this);
    m_seedEdit->setPlaceholderText(u"e.g. 00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"_s);
    m_seedEdit->setMaxLength(64);   // exactly 32 bytes in hex
    m_seedEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    form->addWidget(m_seedEdit);

    auto* seedButtonsRow = new QHBoxLayout();
    auto* randomButton = new QPushButton(tr("&Random Seed"), this);
    auto* generateButton = new QPushButton(tr("Generate &Public Key"), this);
    auto* saveButton = new QPushButton(tr("&Save to Files"), this);
    generateButton->setDefault(true);
    seedButtonsRow->addWidget(randomButton);
    seedButtonsRow->addWidget(generateButton);
    seedButtonsRow->addWidget(saveButton);
    seedButtonsRow->addStretch();
    form->addLayout(seedButtonsRow);

    form->addSpacing(12);

    auto* pubkeyLabel = new QLabel(tr("Public key (derived):"), this);
    form->addWidget(pubkeyLabel);

    m_pubkeyEdit = new QLineEdit(this);
    m_pubkeyEdit->setReadOnly(true);
    m_pubkeyEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    form->addWidget(m_pubkeyEdit);

    form->addStretch();

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextFormat(Qt::PlainText);
    form->addWidget(m_statusLabel);

    connect(randomButton,  &QPushButton::clicked, this, &KeygenTab::onGenerateRandomSeed);
    connect(generateButton, &QPushButton::clicked, this, &KeygenTab::onGeneratePublicKey);
    connect(saveButton,    &QPushButton::clicked, this, &KeygenTab::onSaveToFile);

    updateStatus(tr("Enter a 32-byte hex seed or click \"Random Seed\"."));
}

void KeygenTab::onGenerateRandomSeed() {
    const QByteArray seed = LicenseCrypto::generateRandomSeed();
    m_seedEdit->setText(Helpers::toHex(seed));
    updateStatus(tr("Generated a fresh random seed."));
}

void KeygenTab::onGeneratePublicKey() {
    const QString seedInput = m_seedEdit->text();
    if (!Helpers::isValidSeedHex(seedInput)) {
        updateStatus(tr("Invalid seed: must be exactly 64 hex characters."), /*error=*/true);
        return;
    }
    const QByteArray seed = Helpers::fromHex(seedInput);
    const QByteArray pk   = LicenseCrypto::derivePublicKey(seed);
    if (pk.isEmpty()) {
        updateStatus(tr("Failed to derive public key."), /*error=*/true);
        return;
    }
    m_pubkeyEdit->setText(Helpers::toHex(pk));
    updateStatus(tr("Public key derived. Click \"Save to Files\" to write to disk."));
}

void KeygenTab::onSaveToFile() {
    const QString seedInput = m_seedEdit->text();
    if (!Helpers::isValidSeedHex(seedInput)) {
        updateStatus(tr("Invalid seed: must be exactly 64 hex characters."), /*error=*/true);
        return;
    }
    const QByteArray seed = Helpers::fromHex(seedInput);
    const QByteArray pk   = LicenseCrypto::derivePublicKey(seed);
    if (pk.isEmpty()) {
        updateStatus(tr("Failed to derive public key."), /*error=*/true);
        return;
    }
    m_pubkeyEdit->setText(Helpers::toHex(pk));

    const QString dir      = Helpers::exeDir();
    const QString privPath = QDir(dir).absoluteFilePath(u"private_key.hex"_s);
    const QString pubPath  = QDir(dir).absoluteFilePath(u"public_key.hex"_s);

    QFile privFile(privPath);
    if (!privFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        updateStatus(tr("Cannot write %1: %2").arg(privPath, privFile.errorString()), /*error=*/true);
        return;
    }
    privFile.write(Helpers::toHex(seed).toLatin1());
    privFile.close();

    QFile pubFile(pubPath);
    if (!pubFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        updateStatus(tr("Cannot write %1: %2").arg(pubPath, pubFile.errorString()), /*error=*/true);
        return;
    }
    pubFile.write(Helpers::toHex(pk).toLatin1());
    pubFile.close();

    updateStatus(tr("Saved:\n  %1\n  %2").arg(privPath, pubPath));
}

void KeygenTab::updateStatus(const QString& text, bool isError) {
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(isError ? u"color: #c0392b;"_s : u"color: palette(text);"_s);
}
