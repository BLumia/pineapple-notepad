#include "userlanguageeditordialog.h"
#include "ui_userlanguageeditordialog.h"

#include "basestyles.h"
#include "kshlexer.h"
#include "languageconfigstore.h"

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>

#include <ILexer.h>
#include <Lexilla.h>
#include <Scintilla.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QHeaderView>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTreeWidgetItem>

using namespace Qt::StringLiterals;

namespace {

constexpr int kKindRole = Qt::UserRole + 1;
constexpr int kKeyRole = Qt::UserRole + 2;
constexpr int kDescriptionRole = Qt::UserRole + 3;

const QRegularExpression &idPattern()
{
    static const QRegularExpression re(u"^[A-Za-z0-9_+#-]+$"_s);
    return re;
}

// Per-keyword-set descriptions of a lexer, queried through
// ILexer5::DescribeWordListSets() (one '\n'-separated line per set, in set
// order); an empty list means the lexer has no keyword sets.
QStringList keywordSetDescriptions(const QString &lexerName)
{
    QStringList descriptions;
    Scintilla::ILexer5 *lexer = CreateLexer(lexerName.toUtf8().constData());
    if (!lexer) {
        return descriptions;
    }
    const char *wordListSets = lexer->DescribeWordListSets();
    if (wordListSets && *wordListSets) {
        descriptions = QString::fromUtf8(wordListSets).split(u'\n', Qt::SkipEmptyParts);
    }
    lexer->Release();
    return descriptions;
}

struct PropertyInfo {
    QString name;
    QString typeName;
    QString description;
    QString defaultValue;
};

// Properties declared by a lexer, queried through ILexer5 (PropertyNames /
// PropertyType / DescribeProperty / PropertyGet on a fresh instance, i.e. the
// default value); an empty vector means the lexer declares no properties.
QList<PropertyInfo> lexillaProperties(const QString &lexerName)
{
    QList<PropertyInfo> out;
    Scintilla::ILexer5 *lexer = CreateLexer(lexerName.toUtf8().constData());
    if (!lexer) {
        return out;
    }
    const char *names = lexer->PropertyNames();
    if (names && *names) {
        const QStringList nameList = QString::fromUtf8(names).split(u'\n', Qt::SkipEmptyParts);
        for (const QString &raw : nameList) {
            const QString name = raw.trimmed();
            if (name.isEmpty()) {
                continue;
            }
            PropertyInfo info;
            info.name = name;
            switch (lexer->PropertyType(name.toUtf8().constData())) {
            case SC_TYPE_BOOLEAN:
                info.typeName = u"boolean"_s;
                break;
            case SC_TYPE_INTEGER:
                info.typeName = u"integer"_s;
                break;
            case SC_TYPE_STRING:
                info.typeName = u"string"_s;
                break;
            default:
                info.typeName = u"unknown"_s;
                break;
            }
            const char *d = lexer->DescribeProperty(name.toUtf8().constData());
            info.description = d ? QString::fromUtf8(d) : QString();
            const char *v = lexer->PropertyGet(name.toUtf8().constData());
            info.defaultValue = (v && *v) ? QString::fromUtf8(v) : QString();
            out.append(info);
        }
    }
    lexer->Release();
    return out;
}

QString valuePreview(const QString &value)
{
    if (value.isEmpty()) {
        return QString();
    }
    QString flat = value;
    flat.replace(u'\n', u' ');
    flat.replace(u'\t', u' ');
    if (flat.size() > 80) {
        flat = flat.left(80) + u"\u2026"_s;
    }
    return flat;
}

// Single-line, length-capped description for the tree column; the full text
// stays available via the tooltip and the description label.
QString descriptionPreview(const QString &description)
{
    if (description.isEmpty()) {
        return QString();
    }
    QString flat = description;
    flat.replace(u'\n', u' ');
    flat.replace(u'\t', u' ');
    if (flat.size() > 160) {
        flat = flat.left(160) + u"\u2026"_s;
    }
    return flat;
}

QStringList splitListField(const QString &text)
{
    QStringList out;
    static const QRegularExpression separator(u"[,\\s]+"_s);
    const QStringList parts = text.split(separator, Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            out.append(trimmed);
        }
    }
    return out;
}

} // namespace

UserLanguageEditorDialog::UserLanguageEditorDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::UserLanguageEditorDialog)
{
    m_ui->setupUi(this);
    setWindowTitle(tr("New Custom Language"));
    setupUiExtra();
}

UserLanguageEditorDialog::UserLanguageEditorDialog(const QString &editLanguageId, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::UserLanguageEditorDialog)
    , m_editLanguageId(editLanguageId)
{
    m_ui->setupUi(this);
    if (LanguageConfigStore::isBuiltInLanguage(m_editLanguageId)) {
        // Editing a built-in language creates an override entry.
        setWindowTitle(tr("Override Language"));
    } else {
        setWindowTitle(tr("Edit Custom Language"));
    }
    setupUiExtra();
    loadFromExistingEntry();
}

UserLanguageEditorDialog::~UserLanguageEditorDialog()
{
    delete m_ui;
}

QString UserLanguageEditorDialog::languageId() const
{
    return m_editLanguageId;
}

void UserLanguageEditorDialog::setupUiExtra()
{
    // Base lexer candidates: all lexers with a known style mapping.
    const QStringList lexers = BaseStyles::knownLexers();
    for (const QString &lexer : lexers) {
        m_ui->baseLexerCombo->addItem(lexer, lexer);
    }
    if (!m_ui->baseLexerCombo->count()) {
        m_ui->baseLexerCombo->addItem(u"cpp"_s, u"cpp"_s);
    }
    // Default selection for create mode: cpp. Edit mode overrides this with
    // the saved base lexer in loadFromExistingEntry().
    const int defaultIndex = m_ui->baseLexerCombo->findData(u"cpp"_s);
    m_ui->baseLexerCombo->setCurrentIndex(defaultIndex >= 0 ? defaultIndex : 0);

    // All columns user-resizable; initial widths are content-fitted once per
    // dialog setup (see fitTreeColumns) and then left to the user.
    m_ui->valueTree->header()->setStretchLastSection(false);
    m_ui->valueTree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_ui->splitter->setSizes({330, 220});

    connect(m_ui->baseLexerCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        onBaseLexerChanged();
    });
    connect(m_ui->valueTree, &QTreeWidget::itemSelectionChanged,
            this, &UserLanguageEditorDialog::onTreeSelectionChanged);
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &UserLanguageEditorDialog::validateAndAccept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rebuildValueTree(m_ui->baseLexerCombo->currentData().toString());
    fitTreeColumns();

    if (!m_editLanguageId.isEmpty()) {
        m_ui->idEdit->setReadOnly(true);
        if (LanguageConfigStore::isBuiltInLanguage(m_editLanguageId)) {
            // Override mode: replacing the engine of a built-in language would
            // break its style mapping inheritance, so the lexer is fixed.
            m_ui->baseLexerCombo->setEnabled(false);
            m_ui->baseLexerCombo->setToolTip(
                tr("The base lexer of a built-in language cannot be changed"));
        }
    }
}

// Content-fits all columns, capping the description column so long texts do
// not dominate the view. Called on dialog initialization and whenever the base
// lexer changes (the item content differs greatly between lexers); widths
// stay untouched otherwise.
void UserLanguageEditorDialog::fitTreeColumns()
{
    for (int c = 0; c < 3; ++c) {
        m_ui->valueTree->resizeColumnToContents(c);
    }
    if (m_ui->valueTree->columnWidth(2) > 400) {
        m_ui->valueTree->setColumnWidth(2, 400);
    }
}

void UserLanguageEditorDialog::onBaseLexerChanged()
{
    rebuildValueTree(m_ui->baseLexerCombo->currentData().toString());
    fitTreeColumns();
}

void UserLanguageEditorDialog::rebuildValueTree(const QString &lexerName)
{
    storeCurrentValue();
    m_swappingValues = true;
    m_ui->valueTree->clear();
    m_ui->valueEdit->clear();
    m_ui->valueEdit->setEnabled(false);
    m_ui->descriptionLabel->clear();
    m_currentKind = EditableKind::None;
    m_currentKey.clear();

    // Keyword sets section, following the base lexer's declaration order.
    const QStringList kwDescriptions = keywordSetDescriptions(lexerName);
    auto *kwSection = new QTreeWidgetItem(m_ui->valueTree);
    kwSection->setText(0, tr("Keyword sets (%1)").arg(kwDescriptions.size()));
    kwSection->setFlags(Qt::ItemIsEnabled);
    for (int i = 0; i < kwDescriptions.size(); ++i) {
        const QString &description = kwDescriptions.at(i);
        auto *item = new QTreeWidgetItem(kwSection);
        item->setText(0, tr("Keyword set %1").arg(i));
        item->setData(0, kKindRole, int(EditableKind::KeywordSet));
        item->setData(0, kKeyRole, QString::number(i));
        item->setData(0, kDescriptionRole, description);
        item->setText(2, descriptionPreview(description));
        item->setText(1, valuePreview(m_keywordSets.value(i)));
        if (!description.isEmpty()) {
            item->setToolTip(0, description);
            item->setToolTip(2, description);
        }
    }
    kwSection->setExpanded(true);

    // Properties section, following the base lexer's declaration order.
    const QList<PropertyInfo> props = lexillaProperties(lexerName);
    auto *propSection = new QTreeWidgetItem(m_ui->valueTree);
    propSection->setText(0, tr("Properties (%1)").arg(props.size()));
    propSection->setFlags(Qt::ItemIsEnabled);
    for (const PropertyInfo &info : props) {
        auto *item = new QTreeWidgetItem(propSection);
        item->setText(0, info.name);
        item->setData(0, kKindRole, int(EditableKind::Property));
        item->setData(0, kKeyRole, info.name);
        item->setData(0, kDescriptionRole, info.description);
        item->setText(1, valuePreview(m_properties.value(info.name)));
        item->setText(2, descriptionPreview(info.description));
        QStringList tooltip;
        if (!info.description.isEmpty()) {
            tooltip << info.description;
        }
        tooltip << tr("Type: %1").arg(info.typeName);
        if (!info.defaultValue.isEmpty()) {
            tooltip << tr("Default: %1").arg(info.defaultValue);
        }
        item->setToolTip(0, tooltip.join(u'\n'));
        item->setToolTip(2, tooltip.join(u'\n'));
    }
    propSection->setExpanded(true);

    m_swappingValues = false;
}

void UserLanguageEditorDialog::onTreeSelectionChanged()
{
    if (m_swappingValues) {
        return;
    }
    // The editor still holds the previous item's value; commit it first.
    storeCurrentValue();
    showValueForItem(m_ui->valueTree->currentItem());
}

void UserLanguageEditorDialog::storeCurrentValue()
{
    if (m_swappingValues || m_currentKind == EditableKind::None) {
        return;
    }
    const QString content = m_ui->valueEdit->toPlainText();
    bool hasValue = false;
    if (m_currentKind == EditableKind::KeywordSet) {
        if (!content.trimmed().isEmpty()) {
            m_keywordSets.insert(m_currentKey.toInt(), content);
            hasValue = true;
        } else {
            m_keywordSets.remove(m_currentKey.toInt());
        }
    } else {
        if (!content.isEmpty()) {
            m_properties.insert(m_currentKey, content);
            hasValue = true;
        } else {
            m_properties.remove(m_currentKey);
        }
    }

    if (QTreeWidgetItem *item = findEditableItem(m_currentKind, m_currentKey)) {
        item->setText(1, valuePreview(hasValue ? content : QString()));
    }
}

void UserLanguageEditorDialog::showValueForItem(QTreeWidgetItem *item)
{
    m_swappingValues = true;
    const int kind = item ? item->data(0, kKindRole).toInt() : -1;
    if (kind < 0) {
        m_currentKind = EditableKind::None;
        m_currentKey.clear();
        m_ui->valueEdit->clear();
        m_ui->valueEdit->setEnabled(false);
        m_ui->descriptionLabel->clear();
    } else {
        m_currentKind = static_cast<EditableKind>(kind);
        m_currentKey = item->data(0, kKeyRole).toString();
        m_ui->valueEdit->setPlainText(m_currentKind == EditableKind::KeywordSet
                                          ? m_keywordSets.value(m_currentKey.toInt())
                                          : m_properties.value(m_currentKey));
        m_ui->valueEdit->setEnabled(true);
        m_ui->descriptionLabel->setText(item->data(0, kDescriptionRole).toString());
    }
    m_swappingValues = false;
}

QTreeWidgetItem *UserLanguageEditorDialog::findEditableItem(EditableKind kind, const QString &key) const
{
    for (int s = 0; s < m_ui->valueTree->topLevelItemCount(); ++s) {
        QTreeWidgetItem *section = m_ui->valueTree->topLevelItem(s);
        for (int i = 0; i < section->childCount(); ++i) {
            QTreeWidgetItem *item = section->child(i);
            if (item->data(0, kKindRole).toInt() == int(kind)
                && item->data(0, kKeyRole).toString() == key) {
                return item;
            }
        }
    }
    return nullptr;
}

void UserLanguageEditorDialog::loadFromExistingEntry()
{
    QString displayName;
    QString baseLexer;
    QStringList extensions;
    QStringList filenames;
    QString singleLineComment;
    QString multiLineStart;
    QString multiLineEnd;

    if (LanguageConfigStore::isBuiltInLanguage(m_editLanguageId)) {
        // Override mode: start from the effective (merged) values so saving
        // materializes a full replacement entry.
        const LanguageInfo info = LanguageConfigStore::parsedLanguageConfig()
                                      .infos.value(m_editLanguageId);
        const LanguageSpec *spec = LanguageConfigStore::languageSpec(m_editLanguageId);
        displayName = info.displayName;
        baseLexer = info.effectiveLexer;
        extensions = info.extensions;
        filenames = info.filenames;
        if (spec) {
            singleLineComment = spec->singleLineComment;
            multiLineStart = spec->multiLineComment.start;
            multiLineEnd = spec->multiLineComment.end;
            for (auto it = spec->keywords.constBegin(); it != spec->keywords.constEnd(); ++it) {
                m_keywordSets.insert(it.key(), QString::fromUtf8(it.value()));
            }
            for (auto it = spec->properties.constBegin(); it != spec->properties.constEnd(); ++it) {
                m_properties.insert(QString::fromUtf8(it.key()), QString::fromUtf8(it.value()));
            }
        }
    } else {
        const QJsonObject obj = LanguageConfigStore::loadUserLanguagesObject()
                                    .value(m_editLanguageId).toObject();
        if (obj.isEmpty()) {
            return;
        }
        displayName = obj.value(u"displayName"_s).toString();
        baseLexer = obj.value(u"lexer"_s).toString();

        const QJsonArray extensionsArr = obj.value(u"extensions"_s).toArray();
        for (const QJsonValue &v : extensionsArr) {
            extensions.append(v.toString());
        }
        const QJsonArray filenamesArr = obj.value(u"filenames"_s).toArray();
        for (const QJsonValue &v : filenamesArr) {
            filenames.append(v.toString());
        }
        singleLineComment = obj.value(u"singleLineComment"_s).toString();
        const QJsonObject ml = obj.value(u"multiLineComment"_s).toObject();
        multiLineStart = ml.value(u"start"_s).toString();
        multiLineEnd = ml.value(u"end"_s).toString();

        const QJsonObject keywordsObj = obj.value(u"keywords"_s).toObject();
        for (auto it = keywordsObj.begin(); it != keywordsObj.end(); ++it) {
            bool ok = false;
            const int set = it.key().toInt(&ok);
            if (ok && it.value().isString()) {
                m_keywordSets.insert(set, it.value().toString());
            }
        }
        const QJsonObject propertiesObj = obj.value(u"properties"_s).toObject();
        for (auto it = propertiesObj.begin(); it != propertiesObj.end(); ++it) {
            if (it.value().isString()) {
                m_properties.insert(it.key(), it.value().toString());
            }
        }
    }

    m_ui->idEdit->setText(m_editLanguageId);
    m_ui->nameEdit->setText(displayName);

    const int lexerIndex = m_ui->baseLexerCombo->findData(baseLexer);
    if (lexerIndex >= 0) {
        m_ui->baseLexerCombo->setCurrentIndex(lexerIndex);
    } else {
        // Unknown lexer (e.g. from a hand-edited file): add it to the list so
        // the original value survives an unrelated edit.
        m_ui->baseLexerCombo->insertItem(0, baseLexer, baseLexer);
        m_ui->baseLexerCombo->setCurrentIndex(0);
    }

    m_ui->extensionEdit->setText(extensions.join(u", "_s));
    m_ui->filenameEdit->setText(filenames.join(u", "_s));
    m_ui->singleLineCommentEdit->setText(singleLineComment);
    m_ui->multiLineStartEdit->setText(multiLineStart);
    m_ui->multiLineEndEdit->setText(multiLineEnd);

    rebuildValueTree(m_ui->baseLexerCombo->currentData().toString());
    fitTreeColumns();
}

void UserLanguageEditorDialog::validateAndAccept()
{
    const QString id = m_ui->idEdit->text().trimmed();
    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Language Id"), tr("The language id must not be empty."));
        return;
    }
    if (!idPattern().match(id).hasMatch()) {
        QMessageBox::warning(this, tr("Invalid Language Id"),
            tr("The language id may only contain letters, digits and the characters '_', '+', '-', '#'."));
        return;
    }
    if (m_editLanguageId.isEmpty()) {
        if (LanguageConfigStore::isBuiltInLanguage(id)) {
            QMessageBox::warning(this, tr("Language Id Already Exists"),
                tr("The language id \"%1\" is a built-in language. Use \"Manage Languages...\" to customize it instead.").arg(id));
            return;
        }
        if (LanguageConfigStore::isUserDefined(id)) {
            QMessageBox::warning(this, tr("Language Id Already Exists"),
                tr("The language id \"%1\" is already used by a user defined language.").arg(id));
            return;
        }
        if (kshutils::repository().definitionForName(id).isValid()) {
            QMessageBox::warning(this, tr("Language Id Already Exists"),
                tr("The language id \"%1\" conflicts with an existing syntax highlighting definition.").arg(id));
            return;
        }
    }

    storeCurrentValue();

    QJsonObject obj;
    obj.insert(u"lexer"_s, m_ui->baseLexerCombo->currentData().toString());
    const QString displayName = m_ui->nameEdit->text().trimmed();
    if (!displayName.isEmpty()) {
        obj.insert(u"displayName"_s, displayName);
    }

    const QStringList extensions = splitListField(m_ui->extensionEdit->text());
    if (!extensions.isEmpty()) {
        QJsonArray arr;
        for (const QString &ext : extensions) {
            arr.append(ext);
        }
        obj.insert(u"extensions"_s, arr);
    }

    const QStringList filenames = splitListField(m_ui->filenameEdit->text());
    if (!filenames.isEmpty()) {
        QJsonArray arr;
        for (const QString &fileName : filenames) {
            arr.append(fileName);
        }
        obj.insert(u"filenames"_s, arr);
    }

    const QString singleLineComment = m_ui->singleLineCommentEdit->text();
    if (!singleLineComment.isEmpty()) {
        obj.insert(u"singleLineComment"_s, singleLineComment);
    }

    const QString mlStart = m_ui->multiLineStartEdit->text();
    const QString mlEnd = m_ui->multiLineEndEdit->text();
    if (!mlStart.isEmpty() && !mlEnd.isEmpty()) {
        QJsonObject ml;
        ml.insert(u"start"_s, mlStart);
        ml.insert(u"end"_s, mlEnd);
        obj.insert(u"multiLineComment"_s, ml);
    }

    if (!m_keywordSets.isEmpty()) {
        QJsonObject keywordsObj;
        for (auto it = m_keywordSets.constBegin(); it != m_keywordSets.constEnd(); ++it) {
            keywordsObj.insert(QString::number(it.key()), it.value());
        }
        obj.insert(u"keywords"_s, keywordsObj);
    }

    if (!m_properties.isEmpty()) {
        QJsonObject propertiesObj;
        for (auto it = m_properties.constBegin(); it != m_properties.constEnd(); ++it) {
            propertiesObj.insert(it.key(), it.value());
        }
        obj.insert(u"properties"_s, propertiesObj);
    }

    QJsonObject userLanguages = LanguageConfigStore::loadUserLanguagesObject();
    userLanguages.insert(id, obj);
    if (!LanguageConfigStore::saveUserLanguagesObject(userLanguages)) {
        QMessageBox::warning(this, tr("Save Error"),
            tr("Failed to write:\n%1").arg(LanguageConfigStore::userLanguagesFilePath()));
        return;
    }

    LanguageConfigStore::reloadUserLanguages();
    m_editLanguageId = id;
    accept();
}

void UserLanguageEditorDialog::accept()
{
    QDialog::accept();
}
