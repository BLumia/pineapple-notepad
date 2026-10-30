#include "licensetab.h"

#include "licensecrypto.h"
#include "licensefile.h"
#include "stringhelpers.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

LicenseTab::LicenseTab(QWidget* parent)
    : QWidget(parent) {
    auto* form = new QVBoxLayout(this);

    auto* licenseeLabel = new QLabel(tr("Licensee:"), this);
    form->addWidget(licenseeLabel);
    m_licenseeEdit = new QLineEdit(this);
    m_licenseeEdit->setPlaceholderText(u"e.g. sample user"_s);
    form->addWidget(m_licenseeEdit);

    auto* emailLabel = new QLabel(tr("Email:"), this);
    form->addWidget(emailLabel);
    m_emailEdit = new QLineEdit(this);
    m_emailEdit->setPlaceholderText(u"e.g. user@example.com"_s);
    form->addWidget(m_emailEdit);

    auto* tagsLabel = new QLabel(tr("Tags (comma-separated):"), this);
    form->addWidget(tagsLabel);
    m_tagsEdit = new QLineEdit(this);
    m_tagsEdit->setPlaceholderText(u"e.g. pro,theme-dark,beta"_s);
    form->addWidget(m_tagsEdit);

    form->addSpacing(8);

    m_filenameLabel = new QLabel(this);
    m_filenameLabel->setWordWrap(true);
    m_filenameLabel->setTextFormat(Qt::PlainText);
    form->addWidget(m_filenameLabel);

    form->addSpacing(8);

    auto* actionsRow = new QHBoxLayout();
    auto* generateButton = new QPushButton(tr("&Generate License"), this);
    generateButton->setDefault(true);
    actionsRow->addWidget(generateButton);
    actionsRow->addStretch();
    form->addLayout(actionsRow);

    form->addStretch();

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextFormat(Qt::PlainText);
    form->addWidget(m_statusLabel);

    connect(m_emailEdit, &QLineEdit::textChanged, this, &LicenseTab::onEmailChanged);
    connect(generateButton, &QPushButton::clicked, this, &LicenseTab::onGenerate);

    onEmailChanged();
    updateStatus(tr("Fill the fields and click \"Generate License\"."));
}

void LicenseTab::onEmailChanged() {
    const QString path = resolveLicensePath();
    m_filenameLabel->setText(tr("Output file:\n  %1").arg(path));
}

QString LicenseTab::resolveLicensePath() const {
    const QString email = m_emailEdit->text().trimmed();
    const QString base  = Helpers::sanitizeFilename(email);
    const QString dir   = Helpers::exeDir();
    return QDir(dir).absoluteFilePath(base + u".lic"_s);
}

void LicenseTab::onGenerate() {
    const QString licensee = m_licenseeEdit->text().trimmed();
    const QString email    = m_emailEdit->text().trimmed();
    const QString tagsRaw  = m_tagsEdit->text();

    if (licensee.isEmpty()) {
        updateStatus(tr("Licensee must not be empty."), /*error=*/true);
        return;
    }
    if (email.isEmpty()) {
        updateStatus(tr("Email must not be empty."), /*error=*/true);
        return;
    }

    // Parse tags by comma, drop empties.
    QStringList tags;
    for (const QString& raw : tagsRaw.split(u","_s, Qt::SkipEmptyParts)) {
        tags.append(raw.trimmed());
    }

    // Load private key (seed) from exe dir.
    const QString privPath = QDir(Helpers::exeDir()).absoluteFilePath(u"private_key.hex"_s);
    QFile privFile(privPath);
    if (!privFile.open(QIODevice::ReadOnly)) {
        updateStatus(tr("Cannot read private key:\n  %1\n\nHint: generate it in the \"Generate Public Key\" tab first.")
                         .arg(privPath), /*error=*/true);
        return;
    }
    const QString seedHex = QString::fromLatin1(privFile.readAll()).trimmed();
    privFile.close();
    const QByteArray seed = Helpers::fromHex(seedHex);
    if (seed.size() != LicenseCrypto::kSeedSize) {
        updateStatus(tr("Private key file is corrupt (expected 32 bytes):\n  %1").arg(privPath), /*error=*/true);
        return;
    }

    const QByteArray sk = LicenseCrypto::deriveSecretKey(seed);

    LicensePayload payload;
    payload.licensee = licensee;
    payload.email    = email;
    payload.issuedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    payload.tags     = tags;

    const QByteArray canonical = LicenseFile::canonicalBytes(payload);
    const QByteArray sig = LicenseCrypto::sign(sk, canonical);
    if (sig.size() != LicenseCrypto::kSignatureSize) {
        updateStatus(tr("Failed to sign the license."), /*error=*/true);
        return;
    }

    const QString outPath = resolveLicensePath();
    if (!LicenseFile::save(outPath, payload, sig)) {
        updateStatus(tr("Failed to write license file:\n  %1").arg(outPath), /*error=*/true);
        return;
    }

    updateStatus(tr("License written:\n  %1\n\nIssued at: %2").arg(outPath, payload.issuedAt));
}

void LicenseTab::updateStatus(const QString& text, bool isError) {
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(isError ? u"color: #c0392b;"_s : u"color: palette(text);"_s);
}
