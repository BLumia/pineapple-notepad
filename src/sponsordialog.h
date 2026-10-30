#pragma once

#include <QDialog>

#include "ui_sponsordialog.h"

// Dialog opened from the "Sponsor" menu action. Hosts two tabs:
//   1. "About Sponsorship" — credits and links (formerly a QMessageBox popup).
//   2. "I'm a Sponsor"     — license import / activation info.
//
// The license tab content is driven by AppSettings::licenseContent() plus
// signature verification against the embedded public key (LicenseVerifier).
// Whenever licenseContent changes (e.g. after a successful import or removal),
// the tab refreshes itself automatically via the licenseContentChanged signal.
class SponsorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SponsorDialog(QWidget *parent = nullptr);
    ~SponsorDialog() override;

    // Renders the "thanks + sponsor list" block (used both inside the About
    // tab and by the lightweight startup reminder built in main.cpp).
    static QString sponsorListText();

    // Show the lightweight sponsor reminder used once per app version at
    // startup. Displays only the sponsor list (plus a "Don't show again for
    // this version" checkbox); the full SponsorDialog is reachable via its
    // "I'm a Sponsor" button (which opens the license tab). Returns true if
    // the user ticked "Don't show again for this version".
    static bool showStartupReminder(QWidget *parent = nullptr);

    // Switch to the "I'm a Sponsor" tab (e.g. when the dialog is opened from
    // the editor theme page's "I'm a Sponsor" KMessageWidget action).
    void showLicenseTab();

private slots:
    void importLicense();
    void removeLicense();

private:
    void populateAboutText();
    void refreshLicenseStatus();

    Ui::SponsorDialog *ui;
};
