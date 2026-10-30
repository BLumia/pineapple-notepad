#include "languageselectordialog.h"
#include "ui_languageselectordialog.h"

#include "apppaths.h"
#include "appsettings.h"
#include "kshlexer.h"
#include "languageconfigstore.h"

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>

#include <QHeaderView>
#include <QPushButton>
#include <QShowEvent>
#include <QStandardItemModel>

using namespace Qt::StringLiterals;

namespace {

constexpr int kEntryRole = Qt::UserRole + 1;   // encoded menu entry
constexpr int kIdRole = Qt::UserRole + 2;      // language id
constexpr int kSourceRole = Qt::UserRole + 3;  // LanguageEntrySource
constexpr int kNameRole = Qt::UserRole + 4;    // display name (filtering)

bool isCustomKshDefinition(const KSyntaxHighlighting::Definition &def)
{
    const QString filePath = def.filePath();
    const QString customBase = AppPaths::appDataSubdirPath(u"."_s);
    return filePath.startsWith(customBase, Qt::CaseInsensitive);
}

QString extensionListString(const KSyntaxHighlighting::Definition &def)
{
    const auto exts = def.extensions();
    QStringList cleaned;
    cleaned.reserve(exts.size());
    for (const QString &ext : exts) {
        QString e = ext;
        if (e.startsWith(u"*."_s)) {
            e = e.mid(2);
        }
        cleaned.append(e);
    }
    return cleaned.join(u", "_s);
}

// The effective (default-materialized-on-write) "Language" menu entry list.
QStringList effectiveMenuEntries()
{
    QStringList entries = AppSettings::languageMenuEntries();
    if (entries.isEmpty()) {
        const auto defaults = LanguageConfigStore::defaultMenuEntries();
        entries.reserve(defaults.size());
        for (const auto &entry : defaults) {
            entries.append(entry.encoded);
        }
    }
    return entries;
}

} // namespace

LanguageSelectorFilterProxyModel::LanguageSelectorFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setRecursiveFilteringEnabled(true);
}

void LanguageSelectorFilterProxyModel::setFilterText(const QString &text)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
    m_filterText = text;
    endFilterChange();
#else
    m_filterText = text;
    invalidateFilter();
#endif
}

void LanguageSelectorFilterProxyModel::setCustomOnly(bool customOnly)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    beginFilterChange();
    m_customOnly = customOnly;
    endFilterChange();
#else
    m_customOnly = customOnly;
    invalidateFilter();
#endif
}

bool LanguageSelectorFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QStandardItemModel *sourceModel = qobject_cast<QStandardItemModel *>(this->sourceModel());
    if (!sourceModel) {
        return false;
    }

    QStandardItem *item = sourceModel->itemFromIndex(sourceModel->index(sourceRow, 0, sourceParent));
    if (!item) {
        return false;
    }

    // Section item (top-level)
    if (!item->parent()) {
        for (int i = 0; i < item->rowCount(); ++i) {
            if (filterAcceptsRow(i, sourceModel->indexFromItem(item))) {
                return true;
            }
        }
        return false;
    }

    // Language item (child of section): the name column holds the roles.
    QStandardItem *nameItem = item->parent()->child(item->row(), 1);
    if (!nameItem) {
        return false;
    }

    const auto source = static_cast<LanguageEntrySource>(nameItem->data(kSourceRole).toInt());
    const QString id = nameItem->data(kIdRole).toString();

    if (m_customOnly) {
        if (source == LanguageEntrySource::Catalog) {
            if (!LanguageConfigStore::isUserDefined(id)) {
                return false;
            }
        } else {
            const auto def = kshutils::repository().definitionForName(id);
            if (!def.isValid() || !isCustomKshDefinition(def)) {
                return false;
            }
        }
    }

    if (!m_filterText.isEmpty()) {
        const QString name = nameItem->data(kNameRole).toString();
        const bool matchesName = name.contains(m_filterText, Qt::CaseInsensitive)
                                  || id.contains(m_filterText, Qt::CaseInsensitive);
        QStandardItem *extItem = nameItem->parent()->child(nameItem->row(), 2);
        const bool matchesExt = extItem && extItem->text().contains(m_filterText, Qt::CaseInsensitive);
        if (!matchesName && !matchesExt) {
            return false;
        }
    }

    return true;
}

LanguageSelectorDialog::LanguageSelectorDialog(bool defaultCustomOnly, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::LanguageSelectorDialog)
    , m_defaultCustomOnly(defaultCustomOnly)
{
    m_ui->setupUi(this);

    m_model = new QStandardItemModel(this);
    m_model->setColumnCount(3);
    m_model->setHeaderData(0, Qt::Horizontal, QString());
    m_model->setHeaderData(1, Qt::Horizontal, tr("Name"));
    m_model->setHeaderData(2, Qt::Horizontal, tr("Extensions"));

    m_proxyModel = new LanguageSelectorFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    m_ui->treeView->setModel(m_proxyModel);
    m_ui->treeView->header()->setStretchLastSection(true);
    m_ui->treeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui->treeView->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_ui->treeView->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_ui->treeView->sortByColumn(1, Qt::AscendingOrder);

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    m_pinButton = m_ui->buttonBox->addButton(tr("Pin"), QDialogButtonBox::ActionRole);
    m_pinButton->setEnabled(false);

    connect(m_ui->searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxyModel->setFilterText(text);
        m_ui->treeView->expandAll();
    });
    connect(m_ui->customOnlyCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_proxyModel->setCustomOnly(checked);
        m_ui->treeView->expandAll();
    });
    connect(m_ui->treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &LanguageSelectorDialog::onSelectionChanged);
    connect(m_ui->treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &) {
        if (m_ui->buttonBox->button(QDialogButtonBox::Ok)->isEnabled()) {
            accept();
        }
    });
    connect(m_pinButton, &QPushButton::clicked, this, &LanguageSelectorDialog::togglePinForSelection);
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &LanguageSelectorDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

LanguageSelectorDialog::~LanguageSelectorDialog()
{
    delete m_ui;
}

QString LanguageSelectorDialog::selectedLanguageId() const
{
    if (m_selectedEntry.isEmpty()) {
        return QString();
    }
    const auto entry = LanguageConfigStore::decodeMenuEntry(m_selectedEntry);
    return entry.valid ? entry.languageId : QString();
}

void LanguageSelectorDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    loadEntries();
    m_ui->searchEdit->clear();
    m_ui->customOnlyCheckBox->setChecked(m_defaultCustomOnly);
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    m_selectedEntry.clear();
    m_ui->searchEdit->setFocus();
}

void LanguageSelectorDialog::loadEntries()
{
    m_model->removeRows(0, m_model->rowCount());
    m_selectedEntry.clear();

    const QStringList pinned = effectiveMenuEntries();

    auto addLanguageRow = [&](QStandardItem *sectionItem, const QString &encodedEntry,
                              const QString &id, LanguageEntrySource source,
                              const QString &displayName, const QString &extensions) {
        auto *pinItem = new QStandardItem();
        pinItem->setEditable(false);
        pinItem->setSelectable(false);
        pinItem->setData(pinned.contains(encodedEntry) ? u"\u2605"_s : QString(), Qt::DisplayRole);

        auto *nameItem = new QStandardItem(displayName);
        nameItem->setData(encodedEntry, kEntryRole);
        nameItem->setData(id, kIdRole);
        nameItem->setData(int(source), kSourceRole);
        nameItem->setData(displayName, kNameRole);
        nameItem->setEditable(false);

        auto *extItem = new QStandardItem(extensions);
        extItem->setEditable(false);
        extItem->setSelectable(false);

        sectionItem->appendRow({pinItem, nameItem, extItem});
    };

    // Section rows: a placeholder in the pin column (column 0) that owns the
    // child rows, and the section title in the name column (column 1). The
    // title doubles as the sort key: sorting by the name column must produce
    // a unique key per section, otherwise sections with equal (empty) keys
    // reshuffle whenever the filter is applied and cleared.
    auto addSection = [this](const QString &title) -> QStandardItem * {
        auto *placeholder = new QStandardItem();
        placeholder->setEditable(false);
        placeholder->setSelectable(false);

        auto *titleItem = new QStandardItem(title);
        titleItem->setFlags(Qt::ItemIsEnabled);
        titleItem->setEditable(false);
        titleItem->setSelectable(false);

        m_model->appendRow({placeholder, titleItem});
        return placeholder;
    };

    // "Lexilla" section: catalog entries whose engine is not the KSH lexer.
    const auto &config = LanguageConfigStore::parsedLanguageConfig();
    QStringList catalogIds;
    catalogIds.reserve(config.infos.size());
    for (auto it = config.infos.constBegin(); it != config.infos.constEnd(); ++it) {
        if (it.value().effectiveLexer != u"ksyntaxhighlighting"_s) {
            catalogIds.append(it.key());
        }
    }
    catalogIds.sort(Qt::CaseInsensitive);
    if (!catalogIds.isEmpty()) {
        QStandardItem *lexillaSection = addSection(tr("Lexilla"));

        for (const QString &id : std::as_const(catalogIds)) {
            const LanguageInfo &info = config.infos.value(id);
            const QString name = info.displayName.isEmpty() ? info.id : info.displayName;
            addLanguageRow(lexillaSection,
                           LanguageConfigStore::encodeMenuEntry(false, id),
                           id, LanguageEntrySource::Catalog,
                           name, info.extensions.join(u", "_s));
        }
    }

    // KSH sections, grouped by translated section name.
    const auto defs = kshutils::repository().definitions();
    QHash<QString, QStandardItem *> sectionItems;
    for (const auto &def : defs) {
        if (def.isHidden()) {
            continue;
        }

        const QString section = def.translatedSection();
        if (!sectionItems.contains(section)) {
            sectionItems.insert(section, addSection(section));
        }

        addLanguageRow(sectionItems.value(section),
                       LanguageConfigStore::encodeMenuEntry(true, def.name()),
                       def.name(), LanguageEntrySource::Ksh,
                       def.translatedName(), extensionListString(def));
    }

    m_ui->treeView->expandAll();
}

void LanguageSelectorDialog::onSelectionChanged()
{
    const auto indexes = m_ui->treeView->selectionModel()->selectedIndexes();
    m_selectedEntry.clear();
    if (!indexes.isEmpty()) {
        const QModelIndex proxyIndex = indexes.first();
        const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        QStandardItem *item = m_model->itemFromIndex(sourceIndex);
        if (item && item->parent()) {
            m_selectedEntry = item->data(kEntryRole).toString();
        }
    }

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!m_selectedEntry.isEmpty());
    updatePinButton();
}

void LanguageSelectorDialog::updatePinButton()
{
    if (m_selectedEntry.isEmpty()) {
        m_pinButton->setEnabled(false);
        m_pinButton->setText(tr("Pin"));
        return;
    }

    const bool pinned = isEntryPinned(m_selectedEntry);
    m_pinButton->setEnabled(true);
    m_pinButton->setToolTip(tr("Pin or unpin from Language Menu"));
    m_pinButton->setIcon(QIcon::fromTheme(pinned ? "favorite-favorited" : "favorite"));
    m_pinButton->setText(pinned ? tr("Unpin") : tr("Pin"));
}

bool LanguageSelectorDialog::isEntryPinned(const QString &encodedEntry) const
{
    return effectiveMenuEntries().contains(encodedEntry);
}

void LanguageSelectorDialog::setEntryPinned(const QString &encodedEntry, bool pinned)
{
    // Materialize the default list on first edit, then apply the delta.
    QStringList entries = effectiveMenuEntries();
    if (pinned) {
        if (!entries.contains(encodedEntry)) {
            entries.append(encodedEntry);
        }
    } else {
        entries.removeAll(encodedEntry);
    }
    AppSettings::setLanguageMenuEntries(entries);
    AppSettings::self()->save();

    emit languageMenuChanged();
}

void LanguageSelectorDialog::togglePinForSelection()
{
    if (m_selectedEntry.isEmpty()) {
        return;
    }
    const bool pinned = isEntryPinned(m_selectedEntry);
    setEntryPinned(m_selectedEntry, !pinned);

    // Refresh the pin markers in place.
    const QStringList pinnedEntries = effectiveMenuEntries();
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QStandardItem *sectionItem = m_model->item(i);
        if (!sectionItem) {
            continue;
        }
        for (int j = 0; j < sectionItem->rowCount(); ++j) {
            QStandardItem *nameItem = sectionItem->child(j, 1);
            if (!nameItem) {
                continue;
            }
            const QString encoded = nameItem->data(kEntryRole).toString();
            QStandardItem *pinItem = sectionItem->child(j, 0);
            if (pinItem) {
                pinItem->setData(pinnedEntries.contains(encoded) ? u"\u2605"_s : QString(),
                                 Qt::DisplayRole);
            }
        }
    }
    updatePinButton();
}

void LanguageSelectorDialog::accept()
{
    if (!m_selectedEntry.isEmpty()) {
        QDialog::accept();
    }
}
