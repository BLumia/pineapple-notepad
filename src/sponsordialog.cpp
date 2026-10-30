#include "sponsordialog.h"

#include "appsettings.h"
#include "licensefile.h"
#include "licenseverifier.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFile>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

SponsorDialog::SponsorDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SponsorDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon::fromTheme(u"donate"_s));

    populateAboutText();

    connect(ui->importButton, &QPushButton::clicked, this, &SponsorDialog::importLicense);
    connect(ui->removeButton, &QPushButton::clicked, this, &SponsorDialog::removeLicense);

    // Live-update the license tab whenever licenseContent changes
    // (covers both our own import path and external mutations).
    connect(AppSettings::self(), &AppSettings::licenseContentChanged,
            this, &SponsorDialog::refreshLicenseStatus);

    refreshLicenseStatus();
}

SponsorDialog::~SponsorDialog() = default;

void SponsorDialog::showLicenseTab()
{
    ui->tabs->setCurrentWidget(ui->licenseTab);
}

// ─── Startup reminder ────────────────────────────────────────────────

bool SponsorDialog::showStartupReminder(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(qApp->applicationDisplayName());

    auto *layout = new QVBoxLayout(&dlg);

    auto *label = new QLabel(&dlg);
    label->setTextFormat(Qt::MarkdownText);
    label->setOpenExternalLinks(true);
    // Match QTextBrowser's interaction (mouse-selectable + clickable links)
    // since QLabel defaults to links-only.
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setText(sponsorListText());
    label->setWordWrap(true);
    label->setMinimumWidth(500);
    layout->addWidget(label);

    auto *dontShowAgain = new QCheckBox(tr("Don't show again for this version"), &dlg);
    layout->addWidget(dontShowAgain);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
    auto *iAmSponsorBtn = new QPushButton(tr("I'm a Sponsor"), &dlg);
    buttonBox->addButton(iAmSponsorBtn, QDialogButtonBox::ActionRole);
    layout->addWidget(buttonBox);

    // Both buttons close this reminder; "I'm a Sponsor" additionally opens
    // the full SponsorDialog afterwards (after this dialog has actually
    // closed, so the two never overlap on screen).
    bool openFullDialog = false;
    QObject::connect(iAmSponsorBtn, &QPushButton::clicked, &dlg, [&dlg, &openFullDialog]() {
        openFullDialog = true;
        dlg.accept();
    });
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);

    dlg.exec();

    if (openFullDialog) {
        SponsorDialog fullDlg;
        fullDlg.showLicenseTab();
        fullDlg.exec();
    }
    return dontShowAgain->isChecked();
}

// ─── Tab 1: About Sponsorship ───────────────────────────────────────

QString SponsorDialog::sponsorListText()
{
    QString msg = tr(R"(Thanks for using Pineapple Notepad (version %1). Pineapple Notepad was made possible by our generous sponsors:

%2

You can view the list of sponsors in the About dialog too. If you like this application, please consider [becoming a sponsor!](https://pineapple-notepad.blumia.net/sponsor))");

    QString sponsors;
    const int namePerLine = 3;
    int nameCountInLine = 0;
    sponsors += QLatin1String("<table width=\"100%\" style='margin: 5'>");
    for (const auto &sponsor : KAboutData::applicationData().credits()) {
        if (nameCountInLine == 0) sponsors += QLatin1String("<tr>");
        nameCountInLine++;
        sponsors += QLatin1String("<td style=\"width: 33%;\">") % sponsor.name() % QLatin1String("</td>");
        if (nameCountInLine % namePerLine == 0) {
            sponsors += QLatin1String("</tr>");
            nameCountInLine = 0;
        }
    }
    if (nameCountInLine != 0) sponsors += QLatin1String("</tr>");
    sponsors += QLatin1String("</table>");
    return msg.arg(qApp->applicationVersion()).arg(sponsors);
}

void SponsorDialog::populateAboutText()
{
    QString msg = tr(R"(%1

---

You can learn more about sponsor benefits at [GitHub](https://github.com/BLumia/pineapple-notepad)
or [Gitee](https://gitee.com/BLumia/pineapple-notepad) from project README, and donate to us via
[Ko-fi](https://ko-fi.com/blumia), [Afdian](https://afdian.com/a/BLumia) or
[Itch.io](https://blumia.itch.io/pineapple-notepad). We also have an Open Source Sponsor Goal as well,
if we reach the sponsor goal, this application will be release as an Open-Source Software. So please
consider becoming a sponsor!

%2 Please switch to the next tab for details.)");
    ui->aboutBrowser->setMarkdown(msg.arg(sponsorListText())
                                     .arg(tr("As a special thanks to our supporters, sponsors could unlock some nice-to-have features with a simple license file.")));
}

// ─── Tab 2: License / I'm a Sponsor ─────────────────────────────────

void SponsorDialog::refreshLicenseStatus()
{
    LicensePayload payload;
    const QString licText = AppSettings::licenseContent();
    const bool valid = !licText.isEmpty()
                       && LicenseVerifier::verifyContent(licText, &payload);

    if (valid) {
        ui->statusLabel->setText(
            tr("Your sponsor license is active. Thank you for supporting Pineapple Notepad!"));
        ui->licenseeValue->setText(payload.licensee);
        ui->emailValue->setText(payload.email);
        ui->issuedAtValue->setText(payload.issuedAt);
        ui->tagsValue->setText(payload.tags.join(u", "_s));
        ui->licenseInfoWidget->show();
        ui->importButton->setText(tr("Replace License File..."));
        ui->removeButton->show();
    } else {
        const QStringList lines = {
#ifdef SPONSOR_EDITION
            tr("You are using the Sponsor Edition of Pineapple Notepad, "
            "which means all features are available without requiring a sponsor license.\n"),
            tr("The following license management functions are there just in case if you want "
            "to switch between different version so license information will be preserved. "
            "Feel free to \"activate\" your sponsor license whenever you want.\n"),
#else
            tr("As a special thanks to our supporters, sponsors could unlock some nice-to-have features with a simple license file."),
            tr("\nSponsor license could unlock the following features:\n"),
            tr("- Theme color editor for editing area\n"),
            tr("- Customizable editor context menu\n"),
#endif // SPONSOR_EDITION
            tr("No sponsor license is currently active, use the button below to import your .lic file."),
            tr("Not sure where to download the license file or need help? [Please click here](https://github.com/BLumia/pineapple-notepad/wiki/Sponsor-License).")
        };
        ui->statusLabel->setText(lines.join('\n'));
        ui->statusLabel->setTextFormat(Qt::MarkdownText);
        ui->statusLabel->setOpenExternalLinks(true);
        ui->licenseInfoWidget->hide();
        ui->importButton->setText(tr("Import License File..."));
        ui->removeButton->hide();
    }
}

void SponsorDialog::importLicense()
{
    const QString startDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QFileDialog::Options options;
    if (!AppSettings::useNativeFileDialog()) {
        options |= QFileDialog::DontUseNativeDialog;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Sponsor License"), startDir,
        tr("License Files (*.lic);;All Files (*)"), nullptr, options);
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Import Failed"),
                             tr("Could not open the selected file:\n%1").arg(f.errorString()));
        return;
    }
    const QByteArray rawText = f.readAll();
    f.close();

    LicensePayload payload;
    if (!LicenseVerifier::verifyContent(rawText, &payload)) {
        QMessageBox::warning(this, tr("Import Failed"),
                             tr("The selected file is not a valid sponsor license, "
                                "or the file is corrupted."));
        return;
    }

    // Persist raw .lic text (so feature gating can re-verify on demand) and
    // flip the activation hint (used only for trivial UI state). Setting
    // licenseContent fires licenseContentChanged -> refreshLicenseStatus().
    AppSettings::setLicenseContent(QString::fromUtf8(rawText));
    AppSettings::setIsActivated(true);
    AppSettings::self()->save();
}

void SponsorDialog::removeLicense()
{
    // Confirm before wiping - this deactivates all sponsor features.
    const auto reply = QMessageBox::question(
        this, tr("Remove License"),
        tr("Remove the current sponsor license? "
           "Sponsor-exclusive features will be locked until you import a valid license again."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    AppSettings::setLicenseContent(QString());
    AppSettings::setIsActivated(false);
    AppSettings::self()->save();
}
