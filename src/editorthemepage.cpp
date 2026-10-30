#include "editorthemepage.h"
#include "sciedit.h"
#include "appsettings.h"
#include "sponsordialog.h"
#include "themeconfigstore.h"
#include "licenseverifier.h"

#include <KColorButton>
#include <KMessageWidget>

#include <QAction>
#include <QComboBox>
#include <KCollapsibleGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFormLayout>
#include <QVBoxLayout>

#include <Lexilla.h>

EditorThemePage::EditorThemePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::EditorThemeSettings)
{
    ui->setupUi(this);
    setupPreview();
    setupColorEditor();
    setupSponsorMessage();
}

QString EditorThemePage::selectedThemeId() const
{
    return ui->editorThemeCombo->currentData().toString().trimmed();
}

bool EditorThemePage::hasThemeChanged() const
{
    return selectedThemeId() != m_savedThemeId;
}

void EditorThemePage::syncToSaved()
{
    const QString currentId = AppSettings::editorTheme().trimmed().isEmpty()
        ? QStringLiteral("Pineapple Notepad Light")
        : AppSettings::editorTheme().trimmed();

    QComboBox *combo = ui->editorThemeCombo;
    const int idx = combo->findData(currentId);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);

    m_savedThemeId = currentId;
    if (m_previewEdit) {
        m_previewEdit->setThemeId(m_savedThemeId);
    }
}

void EditorThemePage::setupPreview()
{
    m_previewEdit = new SciEdit(this);
    m_previewEdit->setEditorFont(AppSettings::editorFont());
    m_previewEdit->setLineNumbersEnabled(AppSettings::showLineNumbers());
    m_previewEdit->setEdgeColumn(AppSettings::verticalEdgeColumn());
    m_previewEdit->setVerticalEdgeEnabled(AppSettings::verticalEdgeEnabled());
    m_previewEdit->setMarginWidthN(2, 0);
    m_previewEdit->setTabWidth(AppSettings::tabWidth());
    m_previewEdit->setIndentationGuides(AppSettings::indentGuidesEnabled() ? SC_IV_LOOKBOTH : SC_IV_NONE);

    const QByteArray previewText = R"(// Hello World in C++
#include <iostream>
#include <string>

using namespace std;

/*!
 * \brief The main function
 */
int main() {
    string name = "world";
    cout << "Hello, " << name << "!" << endl;
    for (int i = 0; i < 10; i++) {
        cout << i << endl;
    }
    return 0;
}
)";

    m_previewEdit->addText(previewText.size(), previewText.constData());

    Scintilla::ILexer5 *cppLexer = CreateLexer("cpp");
    m_previewEdit->setLexer(cppLexer, QStringLiteral("cpp"));
    m_previewEdit->colourise(0, -1);
    m_previewEdit->setReadOnly(true);

    ui->themeTabLayout->addWidget(m_previewEdit, 1);

    QComboBox *combo = ui->editorThemeCombo;
    combo->clear();
    const QList<QPair<QString, QString>> themes = ThemeConfigStore::availableThemes();
    for (const auto &p : themes) {
        combo->addItem(p.second, p.first);
    }

    syncToSaved();

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        const QString themeId = ui->editorThemeCombo->currentData().toString().trimmed();
        if (!themeId.isEmpty() && m_previewEdit) {
            m_previewEdit->setThemeId(themeId);
        }
        Q_EMIT themeSelectionChanged();
    });
}

// ─── Color Editor ───────────────────────────────────────────────────

void EditorThemePage::setupColorEditor()
{
    // Populate theme selector
    const QList<QPair<QString, QString>> themes = ThemeConfigStore::availableThemes();
    for (const auto &p : themes)
        ui->colorEditorThemeCombo->addItem(p.second, p.first);

    // Build color button rows inside the scroll area
    auto *scrollContent = new QWidget;
    auto *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(4, 4, 4, 4);

    const auto catalog = ThemeConfigStore::colorRoleCatalog();
    for (const auto &category : catalog) {
        auto *groupBox = new KCollapsibleGroupBox();
        groupBox->setTitle(category.name);
        auto *formLayout = new QFormLayout(groupBox);
        for (const auto &entry : category.entries) {
            auto *btn = new KColorButton(Qt::black);
            btn->setAlphaChannelEnabled(false);

            ColorRow row;
            row.role = entry.role;
            row.button = btn;
            m_colorRows.append(row);

            const int rowIdx = m_colorRows.size() - 1;
            connect(btn, &KColorButton::changed, this, [this, rowIdx](const QColor &c) {
                m_colorRows[rowIdx].currentColor = c;
            });

            formLayout->addRow(entry.displayName + ':', btn);
        }
        scrollLayout->addWidget(groupBox);
    }
    scrollLayout->addStretch();
    ui->colorScrollArea->setWidget(scrollContent);

    // Initial population
    populateColorEditor(ui->colorEditorThemeCombo->currentData().toString());

    // Connections
    connect(ui->colorEditorThemeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
        populateColorEditor(ui->colorEditorThemeCombo->currentData().toString());
    });
    connect(ui->saveColorsButton, &QPushButton::clicked, this, &EditorThemePage::onSaveClicked);
    connect(ui->resetColorsButton, &QPushButton::clicked, this, &EditorThemePage::onResetClicked);
}

void EditorThemePage::populateColorEditor(const QString &themeId)
{
    const auto userOverrides = ThemeConfigStore::userColors(themeId);

    for (auto &row : m_colorRows) {
        const QColor effective = ThemeConfigStore::colorForRole(themeId, row.role);
        row.originalColor = effective;
        row.currentColor = effective;
        row.button->setColor(effective);

        row.button->setToolTip(userOverrides.contains(row.role)
                                   ? tr("Custom override (click to change)")
                                   : tr("Click to change"));
    }
}

void EditorThemePage::onSaveClicked()
{
    const QString themeId = ui->colorEditorThemeCombo->currentData().toString();

    QHash<QString, QColor> changed;
    for (const auto &row : std::as_const(m_colorRows)) {
        if (row.currentColor != row.originalColor)
            changed.insert(row.role, row.currentColor);
    }

    if (changed.isEmpty()) {
        QMessageBox::information(this, tr("No Changes"),
                                 tr("No colors have been modified."));
        return;
    }

    ThemeConfigStore::saveUserColors(themeId, changed);

    // Reapply to preview and all editors
    if (m_previewEdit)
        m_previewEdit->setThemeId(selectedThemeId());
    Q_EMIT themeColorsChanged();

    // Repopulate to refresh original colors
    populateColorEditor(themeId);
}

void EditorThemePage::onResetClicked()
{
    const QString themeId = ui->colorEditorThemeCombo->currentData().toString();

    const auto reply = QMessageBox::question(
        this, tr("Reset Theme Colors"),
        tr("Reset all color overrides for \"%1\"?\n"
           "This will remove all custom colors saved for this theme.")
            .arg(themeId),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    ThemeConfigStore::resetUserColors(themeId);

    if (m_previewEdit)
        m_previewEdit->setThemeId(selectedThemeId());
    Q_EMIT themeColorsChanged();

    populateColorEditor(themeId);
}

// ─── Sponsor message ────────────────────────────────────────────────

void EditorThemePage::setupSponsorMessage()
{
#ifdef SPONSOR_EDITION
    // Sponsor Edition: all features always unlocked, no gate UI needed.
    // m_sponsorMessage stays nullptr; saveColorsButton keeps its .ui default (enabled).
#else
    m_sponsorMessage = new KMessageWidget(this);
    m_sponsorMessage->setMessageType(KMessageWidget::Information);
    m_sponsorMessage->setWordWrap(true);
    m_sponsorMessage->setCloseButtonVisible(false);
    m_sponsorMessage->setIcon(QIcon::fromTheme("love"));
    m_sponsorMessage->setText(tr("This feature is currently only available to sponsors. "
                                 "Please consider becoming a sponsor."));
    // Action button inside the message - opens SponsorDialog on the license tab
    // so the user can import their .lic file without hunting through the menu.
    auto *iAmSponsorAction = new QAction(tr("I'm a Sponsor"), m_sponsorMessage);
    m_sponsorMessage->addAction(iAmSponsorAction);
    connect(iAmSponsorAction, &QAction::triggered, this, [this]() {
        SponsorDialog dialog(this);
        dialog.showLicenseTab();
        dialog.exec();
    });
    // Insert at the top of the color editor tab's layout (above the combo row).
    ui->colorEditorLayout->insertWidget(0, m_sponsorMessage);

    connect(AppSettings::self(), &AppSettings::licenseContentChanged,
            this, &EditorThemePage::updateSponsorRelatedUi);
    updateSponsorRelatedUi();
#endif
}

void EditorThemePage::updateSponsorRelatedUi()
{
#ifdef SPONSOR_EDITION
    // No-op in Sponsor Edition.
#else
    if (!m_sponsorMessage)
        return;

    // Per the design, feature availability is determined by actually
    // verifying the stored license content, NOT by the (weak) isActivated
    // hint. An empty or tampered licenseContent means the user is not a
    // verified sponsor: show the prompt and lock the save button.
    const QString licText = AppSettings::licenseContent();
    const bool isValid = !licText.isEmpty() && LicenseVerifier::verifyContent(licText);
    m_sponsorMessage->setVisible(!isValid);
    ui->saveColorsButton->setEnabled(isValid);
#endif
}
