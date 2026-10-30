#include "verifytab.h"

#include "licensecrypto.h"
#include "licensefile.h"
#include "stringhelpers.h"

#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

VerifyTab::VerifyTab(QWidget* parent)
    : QWidget(parent) {
    auto* form = new QVBoxLayout(this);

    auto* pathLabel = new QLabel(tr("License file:"), this);
    form->addWidget(pathLabel);

    auto* pathRow = new QHBoxLayout();
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(u"path to a .lic file"_s);
    auto* browseButton = new QPushButton(tr("&Browse..."), this);
    pathRow->addWidget(m_pathEdit, /*stretch=*/1);
    pathRow->addWidget(browseButton);
    form->addLayout(pathRow);

    auto* actionsRow = new QHBoxLayout();
    auto* verifyButton = new QPushButton(tr("&Verify"), this);
    verifyButton->setDefault(true);
    actionsRow->addWidget(verifyButton);
    actionsRow->addStretch();
    form->addLayout(actionsRow);

    form->addSpacing(8);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setFont([this]() {
        QFont f = font();
        f.setFamily(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
        return f;
    }());
    form->addWidget(m_statusLabel);

    form->addStretch();

    connect(browseButton, &QPushButton::clicked, this, &VerifyTab::onBrowse);
    connect(verifyButton, &QPushButton::clicked, this, &VerifyTab::onVerify);

    // Default to the most recently written .lic in the exe directory, if any.
    const QString exeDir = Helpers::exeDir();
    const QStringList candidates = QDir(exeDir).entryList({u"*.lic"_s}, QDir::Files, QDir::Time);
    if (!candidates.isEmpty()) {
        m_pathEdit->setText(QDir(exeDir).absoluteFilePath(candidates.first()));
    }
    updateStatus(tr("Pick a .lic file and click \"Verify\"."));
}

void VerifyTab::onBrowse() {
    const QString startDir = m_pathEdit->text().isEmpty()
        ? Helpers::exeDir()
        : QFileInfo(m_pathEdit->text()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open License"), startDir,
                                                      tr("License files (*.lic);;All files (*)"));
    if (!path.isEmpty()) {
        m_pathEdit->setText(path);
    }
}

void VerifyTab::onVerify() {
    const QString licPath = m_pathEdit->text().trimmed();
    if (licPath.isEmpty()) {
        updateStatus(tr("Please pick a license file first."), /*error=*/true);
        return;
    }

    LicensePayload payload;
    QByteArray sig;
    if (!LicenseFile::load(licPath, &payload, &sig)) {
        updateStatus(tr("Failed to parse license file:\n  %1").arg(licPath), /*error=*/true);
        return;
    }

    // Load public key from exe dir.
    const QString pubPath = QDir(Helpers::exeDir()).absoluteFilePath(u"public_key.hex"_s);
    QFile pubFile(pubPath);
    if (!pubFile.open(QIODevice::ReadOnly)) {
        updateStatus(tr("Cannot read public key:\n  %1\n\nHint: generate it in the \"Generate Public Key\" tab first.")
                         .arg(pubPath), /*error=*/true);
        return;
    }
    const QString pubHex = QString::fromLatin1(pubFile.readAll()).trimmed();
    pubFile.close();
    const QByteArray pk = Helpers::fromHex(pubHex);
    if (pk.size() != LicenseCrypto::kPublicKeySize) {
        updateStatus(tr("Public key file is corrupt (expected 32 bytes):\n  %1").arg(pubPath), /*error=*/true);
        return;
    }

    const QByteArray canonical = LicenseFile::canonicalBytes(payload);
    const bool ok = LicenseCrypto::verify(sig, pk, canonical);

    QString tagsJoined = payload.tags.isEmpty() ? tr("(none)") : payload.tags.join(u", "_s);
    QString summary = tr(
        "File:        %1\n"
        "Signature:   %2\n"
        "\n"
        "Licensee:    %3\n"
        "Email:       %4\n"
        "Issued at:   %5\n"
        "Tags:        %6"
    ).arg(QFileInfo(licPath).fileName(),
          ok ? tr("VALID") : tr("INVALID"),
          payload.licensee,
          payload.email,
          payload.issuedAt,
          tagsJoined);

    updateStatus(summary, /*error=*/!ok);
}

void VerifyTab::updateStatus(const QString& text, bool isError) {
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(isError ? u"color: #c0392b;"_s : u"color: palette(text);"_s);
}
