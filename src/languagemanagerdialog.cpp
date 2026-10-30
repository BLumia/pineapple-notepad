#include "languagemanagerdialog.h"
#include "ui_languagemanagerdialog.h"

#include "languageconfigstore.h"
#include "userlanguageeditordialog.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidgetItem>

using namespace Qt::StringLiterals;

namespace {
constexpr int kIdRole = Qt::UserRole + 1;
}

LanguageManagerDialog::LanguageManagerDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::LanguageManagerDialog)
{
    m_ui->setupUi(this);

    m_ui->languageTree->header()->setStretchLastSection(false);
    // The name column stretches to fill the view; the other columns are
    // user-resizable and start at their content width.
    m_ui->languageTree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_ui->languageTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_ui->languageTree->sortByColumn(0, Qt::AscendingOrder);

    m_newButton = m_ui->buttonBox->addButton(tr("New..."), QDialogButtonBox::ActionRole);
    m_editButton = m_ui->buttonBox->addButton(tr("Edit..."), QDialogButtonBox::ActionRole);
    m_resetButton = m_ui->buttonBox->addButton(tr("Reset"), QDialogButtonBox::ActionRole);
    m_deleteButton = m_ui->buttonBox->addButton(tr("Delete"), QDialogButtonBox::ActionRole);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_newButton, &QPushButton::clicked, this, &LanguageManagerDialog::onNew);
    connect(m_editButton, &QPushButton::clicked, this, &LanguageManagerDialog::onEdit);
    connect(m_resetButton, &QPushButton::clicked, this, &LanguageManagerDialog::onReset);
    connect(m_deleteButton, &QPushButton::clicked, this, &LanguageManagerDialog::onDelete);

    connect(m_ui->searchEdit, &QLineEdit::textChanged, this, &LanguageManagerDialog::applyFilter);
    connect(m_ui->languageTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
                m_currentId = current ? current->data(0, kIdRole).toString() : QString();
                updateButtons();
            });
    connect(m_ui->languageTree, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem *, int) {
                onEdit();
            });

    loadLanguages();
    // Content-fit the interactive columns once; later reloads keep the widths
    // the user chose (the stretch column always fills the remaining space).
    for (int c = 1; c < 4; ++c) {
        m_ui->languageTree->resizeColumnToContents(c);
    }
}

LanguageManagerDialog::~LanguageManagerDialog()
{
    delete m_ui;
}

void LanguageManagerDialog::loadLanguages()
{
    m_ui->languageTree->clear();
    m_currentId.clear();

    const auto &config = LanguageConfigStore::parsedLanguageConfig();
    for (auto it = config.infos.constBegin(); it != config.infos.constEnd(); ++it) {
        const LanguageInfo &info = it.value();
        if (info.effectiveLexer == u"ksyntaxhighlighting"_s) {
            continue;
        }

        const QString name = info.displayName.isEmpty() ? info.id : info.displayName;
        QString status;
        if (LanguageConfigStore::isOverridden(info.id)) {
            status = tr("Overridden");
        } else if (info.userDefined) {
            status = tr("User defined");
        } else {
            status = tr("Built-in");
        }

        auto *item = new QTreeWidgetItem(m_ui->languageTree);
        item->setText(0, name);
        item->setText(1, info.id);
        item->setText(2, info.effectiveLexer);
        item->setText(3, status);
        item->setData(0, kIdRole, info.id);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    }

    applyFilter(m_ui->searchEdit->text());
    updateButtons();
}

void LanguageManagerDialog::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < m_ui->languageTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_ui->languageTree->topLevelItem(i);
        const bool match = needle.isEmpty()
            || item->text(0).contains(needle, Qt::CaseInsensitive)
            || item->text(1).contains(needle, Qt::CaseInsensitive)
            || item->text(2).contains(needle, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void LanguageManagerDialog::updateButtons()
{
    const bool hasSelection = !m_currentId.isEmpty();
    m_editButton->setEnabled(hasSelection);
    m_resetButton->setEnabled(hasSelection && LanguageConfigStore::isOverridden(m_currentId));
    m_deleteButton->setEnabled(hasSelection
                               && LanguageConfigStore::isUserDefined(m_currentId)
                               && !LanguageConfigStore::isBuiltInLanguage(m_currentId));
}

void LanguageManagerDialog::onNew()
{
    UserLanguageEditorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        // The editor already persisted and reloaded the catalog.
        emit languagesChanged();
        loadLanguages();
    }
}

void LanguageManagerDialog::onEdit()
{
    if (m_currentId.isEmpty()) {
        return;
    }
    // Editing a built-in language opens the editor in override mode.
    UserLanguageEditorDialog dlg(m_currentId, this);
    if (dlg.exec() == QDialog::Accepted) {
        emit languagesChanged();
        loadLanguages();
    }
}

void LanguageManagerDialog::onReset()
{
    if (m_currentId.isEmpty() || !LanguageConfigStore::isOverridden(m_currentId)) {
        return;
    }
    const QString name = LanguageConfigStore::displayName(m_currentId);
    const auto answer = QMessageBox::question(this, tr("Reset Language"),
        tr("Remove your customization of \"%1\" and restore the built-in definition?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    QJsonObject userLanguages = LanguageConfigStore::loadUserLanguagesObject();
    if (userLanguages.contains(m_currentId)) {
        userLanguages.remove(m_currentId);
        LanguageConfigStore::saveUserLanguagesObject(userLanguages);
    }
    LanguageConfigStore::reloadUserLanguages();
    emit languagesChanged();
    loadLanguages();
}

void LanguageManagerDialog::onDelete()
{
    if (m_currentId.isEmpty() || !LanguageConfigStore::isUserDefined(m_currentId)
        || LanguageConfigStore::isBuiltInLanguage(m_currentId)) {
        return;
    }
    const QString name = LanguageConfigStore::displayName(m_currentId);
    const auto answer = QMessageBox::question(this, tr("Delete Language"),
        tr("Delete user defined language \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    QJsonObject userLanguages = LanguageConfigStore::loadUserLanguagesObject();
    if (userLanguages.contains(m_currentId)) {
        userLanguages.remove(m_currentId);
        LanguageConfigStore::saveUserLanguagesObject(userLanguages);
    }
    LanguageConfigStore::reloadUserLanguages();
    emit languagesChanged();
    loadLanguages();
}
