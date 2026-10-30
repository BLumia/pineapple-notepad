#include "settingsdialog.h"
#include "editorthemepage.h"
#include "appsettings.h"

#include <KMessageBox>
#include <KStandardGuiItem>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QPushButton>

SettingsDialog::SettingsDialog(QWidget *parent, const QString &name, KCoreConfigSkeleton *config)
    : KConfigDialog(parent, name, config)
{
}

QSize SettingsDialog::sizeHint() const
{
    return {850, 550};
}

void SettingsDialog::setEditorThemePage(EditorThemePage *page)
{
    m_editorThemePage = page;
    if (m_editorThemePage) {
        connect(m_editorThemePage, &EditorThemePage::themeSelectionChanged, this, [this]() {
            updateButtons();
        });
    }
}

bool SettingsDialog::hasChanged()
{
    if (m_editorThemePage && m_editorThemePage->hasThemeChanged()) {
        return true;
    }
    return KConfigDialog::hasChanged();
}

void SettingsDialog::updateSettings()
{
    bool themeChanged = m_editorThemePage && m_editorThemePage->hasThemeChanged();
    if (themeChanged) {
        AppSettings::setEditorTheme(m_editorThemePage->selectedThemeId());
        AppSettings::self()->save();
        m_editorThemePage->syncToSaved();
    }
    KConfigDialog::updateSettings();
    if (themeChanged) {
        Q_EMIT settingsChanged(objectName());
        updateButtons();
    }
}

void SettingsDialog::updateWidgets()
{
    if (m_editorThemePage) {
        m_editorThemePage->syncToSaved();
    }
    KConfigDialog::updateWidgets();
}

bool SettingsDialog::hasUnsavedChanges() const
{
    if (const_cast<SettingsDialog *>(this)->hasChanged()) {
        return true;
    }
    if (auto *applyBtn = buttonBox()->button(QDialogButtonBox::Apply)) {
        return applyBtn->isEnabled();
    }
    return false;
}

void SettingsDialog::closeEvent(QCloseEvent *event)
{
    if (hasUnsavedChanges()) {
        event->ignore();
        const auto response = KMessageBox::warningTwoActionsCancel(
            this,
            tr("You have unsaved changes. Do you want to apply the changes or discard them?"),
            tr("Warning"),
            KStandardGuiItem::save(),
            KStandardGuiItem::discard(),
            KStandardGuiItem::cancel());
        if (response == KMessageBox::PrimaryAction) {
            buttonBox()->button(QDialogButtonBox::Apply)->click();
            accept();
        } else if (response == KMessageBox::SecondaryAction) {
            buttonBox()->button(QDialogButtonBox::Cancel)->click();
        }
    } else {
        KConfigDialog::closeEvent(event);
    }
}
