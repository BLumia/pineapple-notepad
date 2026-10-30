#include "documentmanager.h"
#include "kshlexer.h"
#include "languageconfigstore.h"
#include "loglexer.h"
#include "sciedit.h"

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <Sonnet/Settings>

#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QLoggingCategory>
#include <QSet>

Q_LOGGING_CATEGORY(lcDocumentManager, "pnotepad.documentmanager")

#include <ILexer.h>
#include <Lexilla.h>
#include <Scintilla.h>

using namespace Qt::StringLiterals;

QString DocumentManager::normalizeFilePath(const QString &path)
{
    const QFileInfo fi(path);
    return fi.canonicalFilePath().isEmpty() ? fi.absoluteFilePath() : fi.canonicalFilePath();
}

int DocumentManager::findDocumentByFilePath(const QString &filePath) const
{
    const QString key = normalizeFilePath(filePath);
    if (key.isEmpty()) {
        return -1;
    }

    for (auto it = m_documents.constBegin(); it != m_documents.constEnd(); ++it) {
        const Document &doc = it.value();
        if (doc.filePath.isEmpty() || doc.untitled) {
            continue;
        }
        if (normalizeFilePath(doc.filePath) == key) {
            return doc.id;
        }
    }

    return -1;
}

int DocumentManager::findUntitledDocumentByTitle(const QString &title) const
{
    for (auto it = m_documents.constBegin(); it != m_documents.constEnd(); ++it) {
        const Document &doc = it.value();
        if (doc.untitled && doc.title == title) {
            return doc.id;
        }
    }
    return -1;
}

void DocumentManager::applyLexerToEditor(SciEdit *editor, const QString &language, const DocumentManager *docManager, bool isLargeFile, bool colouriseNow)
{
    if (!editor) {
        return;
    }

    if (language.isEmpty() || isLargeFile) {
        editor->setLexer(nullptr, QString(), colouriseNow);
        return;
    }

    const QString actualLexerName = docManager ? docManager->resolveLexerForLanguage(language) : language;
    const QString languageIdOverride = (actualLexerName == language) ? QString() : language;
    Scintilla::ILexer5 *lexer = nullptr;
    if (actualLexerName == QStringLiteral("logplus")) {
        lexer = createLogLexer();
    } else if (actualLexerName == QStringLiteral("ksyntaxhighlighting")) {
        lexer = createKshLexer();
        if (!languageIdOverride.isEmpty()) {
            lexer->PropertySet("lexer.ksyntaxhighlighting.definition",
                               languageIdOverride.toUtf8().constData());
        }
    } else {
        lexer = CreateLexer(actualLexerName.toStdString().c_str());
    }
    editor->setLexer(lexer, languageIdOverride, colouriseNow);
}

quint64 DocumentManager::s_recentTagCounter = 0;

DocumentManager::DocumentManager(QObject *parent)
    : QAbstractListModel(parent)
    , m_nextDocumentId(1)
{
}

DocumentManager::~DocumentManager()
{
}

int DocumentManager::createNewDocument()
{
    Document doc;
    doc.id = m_nextDocumentId++;
    doc.title = generateUntitledName();
    doc.filePath = QString();
    doc.content = QByteArray();
    doc.encoding = QStringLiteral("UTF-8");
    doc.codecName = QByteArrayLiteral("UTF-8");
    doc.encodingWriteBom = false;
    doc.eolMode = SC_EOL_LF;
    doc.eolMixed = false;
    doc.language = QString();
    doc.spellCheckEnabled = Sonnet::Settings().checkerEnabledByDefault();
    doc.contentModified = false;
    doc.encodingModified = false;
    doc.untitled = true;
    doc.recentTag = ++s_recentTagCounter;

    const int row = m_docIdOrder.size();
    beginInsertRows(QModelIndex(), row, row);
    m_docIdOrder.append(doc.id);
    m_documents[doc.id] = doc;
    endInsertRows();

    emit documentCreated(doc.id);
    return doc.id;
}

int DocumentManager::openDocument(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcDocumentManager) << "Failed to open file:" << filePath;
        return -1;
    }
    file.close();

    Document doc;
    doc.id = m_nextDocumentId++;
    doc.title = QFileInfo(filePath).fileName();
    doc.filePath = filePath;
    doc.content = QByteArray();
    doc.encoding = QStringLiteral("UTF-8");
    doc.codecName = QByteArrayLiteral("UTF-8");
    doc.encodingWriteBom = false;
    doc.language = detectLanguageFromExtension(filePath);
    doc.eolMode = SC_EOL_LF;
    doc.eolMixed = false;
    doc.spellCheckEnabled = Sonnet::Settings().checkerEnabledByDefault();
    doc.contentModified = false;
    doc.encodingModified = false;
    doc.untitled = false;
    doc.recentTag = ++s_recentTagCounter;

    const int row = m_docIdOrder.size();
    beginInsertRows(QModelIndex(), row, row);
    m_docIdOrder.append(doc.id);
    m_documents[doc.id] = doc;
    endInsertRows();

    emit documentOpened(doc.id, filePath);
    return doc.id;
}

bool DocumentManager::saveDocument(int docId)
{
    if (!m_documents.contains(docId)) {
        return false;
    }

    Document &doc = m_documents[docId];
    if (doc.filePath.isEmpty()) {
        return false; // require to use saveAs
    }

    const bool wasModified = doc.contentModified || doc.encodingModified;
    if (!doc.save()) {
        return false;
    }

    emitDataChangedForRoles(docId, { Qt::DecorationRole });
    emit documentSaved(docId, doc.filePath);
    if (wasModified) {
        emit documentModified(docId, false);
    }

    return true;
}

bool DocumentManager::saveDocumentAs(int docId, const QString &filePath)
{
    if (!m_documents.contains(docId)) {
        return false;
    }

    Document &doc = m_documents[docId];

    const bool wasModified = doc.contentModified || doc.encodingModified;
    const QString oldTitle = doc.title;

    if (!doc.saveAs(filePath)) {
        return false;
    }

    doc.language = detectLanguageFromExtension(filePath);

    emitDataChangedForRoles(docId, { Qt::DisplayRole, Qt::ToolTipRole, Qt::DecorationRole });
    emit documentSaved(docId, doc.filePath);
    if (wasModified) {
        emit documentModified(docId, false);
    }
    if (doc.title != oldTitle) {
        emit documentTitleChanged(docId, doc.title);
    }

    return true;
}

void DocumentManager::closeDocument(int docId)
{
    auto it = m_documents.find(docId);
    if (it == m_documents.end())
        return;
    const int row = m_docIdOrder.indexOf(docId);
    if (row >= 0) {
        beginRemoveRows(QModelIndex(), row, row);
        m_docIdOrder.removeAt(row);
        m_documents.erase(it);
        endRemoveRows();
    }
    emit documentClosed(docId);
}

QString DocumentManager::getDocumentTitle(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].title;
    }
    return QString();
}

QString DocumentManager::getDocumentFilePath(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].filePath;
    }
    return QString();
}

QString DocumentManager::getDocumentEncoding(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].encoding;
    }
    return "UTF-8";
}

Document *DocumentManager::getDocument(int docId)
{
    auto it = m_documents.find(docId);
    if (it == m_documents.end()) {
        return nullptr;
    }
    return &it.value();
}

const Document *DocumentManager::getDocument(int docId) const
{
    auto it = m_documents.find(docId);
    if (it == m_documents.end()) {
        return nullptr;
    }
    return &it.value();
}

QString DocumentManager::getDocumentLanguage(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].language;
    }
    return QString();
}

int DocumentManager::getDocumentEolMode(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].eolMode;
    }
#ifdef Q_OS_WIN
    return SC_EOL_CRLF;
#else
    return SC_EOL_LF;
#endif
}

bool DocumentManager::isDocumentEolMixed(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].eolMixed;
    }
    return false;
}

int DocumentManager::getDocumentIndentTabWidth(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].indentTabWidth;
    }
    return 4;
}

bool DocumentManager::getDocumentIndentUseTabs(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].indentUseTabs;
    }
    return false;
}

bool DocumentManager::getDocumentSpellCheckEnabled(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].spellCheckEnabled;
    }
    return false;
}

bool DocumentManager::isDocumentModified(int docId) const
{
    if (m_documents.contains(docId)) {
        const Document &doc = m_documents[docId];
        return doc.contentModified || doc.encodingModified;
    }
    return false;
}

bool DocumentManager::isDocumentUntitled(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].untitled;
    }
    return true;
}

void DocumentManager::setDocumentModified(int docId, bool modified)
{
    if (m_documents.contains(docId)) {
        Document &doc = m_documents[docId];
        const bool before = doc.contentModified || doc.encodingModified;
        doc.contentModified = modified;
        const bool after = doc.contentModified || doc.encodingModified;
        if (before != after) {
            emitDataChangedForRoles(docId, { Qt::DecorationRole });
            emit documentModified(docId, after);
        }
    }
}

bool DocumentManager::isDocumentMonitored(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].isMonitoringOn;
    }
    return false;
}

bool DocumentManager::isDocumentReadOnly(int docId) const
{
    if (m_documents.contains(docId)) {
        return m_documents[docId].readOnly;
    }
    return false;
}

void DocumentManager::setDocumentReadOnly(int docId, bool readOnly)
{
    auto it = m_documents.find(docId);
    if (it == m_documents.end())
        return;
    Document &doc = it.value();
    if (doc.readOnly == readOnly)
        return;
    doc.readOnly = readOnly;
    emitDataChangedForRoles(docId, { Qt::DecorationRole });
    emit documentReadOnlyChanged(docId, readOnly);
}

void DocumentManager::touchDocument(int docId)
{
    auto it = m_documents.find(docId);
    if (it == m_documents.end())
        return;
    it.value().recentTag = ++s_recentTagCounter;
    emitDataChangedForRoles(docId, { RecentTagRole });
}

void DocumentManager::setDocumentEncodingModified(int docId, bool modified)
{
    if (!m_documents.contains(docId)) {
        return;
    }

    Document &doc = m_documents[docId];
    const bool before = doc.contentModified || doc.encodingModified;
    doc.encodingModified = modified;
    const bool after = doc.contentModified || doc.encodingModified;
    if (before != after) {
        emitDataChangedForRoles(docId, { Qt::DecorationRole });
        emit documentModified(docId, after);
    }
}

void DocumentManager::setDocumentEncodingDetails(int docId, const QString &displayName, const QByteArray &codecName, bool writeBom)
{
    if (!m_documents.contains(docId)) {
        return;
    }

    Document &doc = m_documents[docId];
    doc.encoding = displayName;
    doc.codecName = codecName;
    doc.encodingWriteBom = writeBom;
}

void DocumentManager::setDocumentLanguage(int docId, const QString &language)
{
    if (m_documents.contains(docId)) {
        m_documents[docId].language = language;
    }
}

void DocumentManager::setDocumentEolMode(int docId, int eolMode, bool mixed)
{
    if (!m_documents.contains(docId)) {
        return;
    }
    Document &doc = m_documents[docId];
    doc.eolMode = eolMode;
    doc.eolMixed = mixed;
}

void DocumentManager::setDocumentIndentTabWidth(int docId, int width)
{
    if (!m_documents.contains(docId)) {
        return;
    }
    m_documents[docId].indentTabWidth = width;
}

void DocumentManager::setDocumentIndentUseTabs(int docId, bool useTabs)
{
    if (!m_documents.contains(docId)) {
        return;
    }
    m_documents[docId].indentUseTabs = useTabs;
}

void DocumentManager::setDocumentSpellCheckEnabled(int docId, bool enabled)
{
    auto it = m_documents.find(docId);
    if (it == m_documents.end()) {
        return;
    }
    Document &doc = it.value();
    if (doc.spellCheckEnabled == enabled) {
        return;
    }
    doc.spellCheckEnabled = enabled;
    // Apply to every editor currently displaying this document (multi-view).
    for (const EditorRef &ref : doc.editorReferences()) {
        if (ref.editor) {
            ref.editor->setSpellCheckEnabled(enabled);
        }
    }
}

void DocumentManager::setDocumentFilePath(int docId, const QString &filePath)
{
    if (!m_documents.contains(docId)) {
        return;
    }
    Document &doc = m_documents[docId];
    doc.filePath = filePath;
    doc.title = QFileInfo(filePath).fileName();
    doc.language = detectLanguageFromExtension(filePath);
    emitDataChangedForRoles(docId, { Qt::DisplayRole, Qt::ToolTipRole });
    emit documentTitleChanged(docId, doc.title);
}

QList<int> DocumentManager::getAllDocumentIds() const
{
    return m_docIdOrder;
}

int DocumentManager::getDocumentCount() const
{
    return m_docIdOrder.size();
}

int DocumentManager::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_docIdOrder.size();
}

QVariant DocumentManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    const int docId = docIdAt(index.row());
    if (docId < 0)
        return {};
    const Document *doc = getDocument(docId);
    if (!doc)
        return {};

    switch (role) {
    case Qt::DisplayRole:
        return doc->title;
    case Qt::DecorationRole:
        return iconForDoc(*doc);
    case Qt::ToolTipRole:
        return doc->filePath.isEmpty() ? doc->title : doc->filePath;
    case Qt::FontRole: {
        QFont font;
        font.setBold(docId == m_currentDocId);
        return font;
    }
    case DocIdRole:
        return docId;
    case DocPtrRole:
        return QVariant::fromValue(static_cast<void *>(const_cast<Document *>(doc)));
    case RecentTagRole:
        return doc->recentTag;
    }
    return {};
}

Qt::ItemFlags DocumentManager::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

int DocumentManager::rowForDocId(int docId) const
{
    return m_docIdOrder.indexOf(docId);
}

QModelIndex DocumentManager::indexForDocId(int docId) const
{
    const int row = rowForDocId(docId);
    return row >= 0 ? index(row, 0) : QModelIndex();
}

int DocumentManager::docIdAt(int row) const
{
    if (row < 0 || row >= m_docIdOrder.size())
        return -1;
    return m_docIdOrder.at(row);
}

void DocumentManager::setCurrentDocId(int docId)
{
    if (m_currentDocId == docId)
        return;
    const int oldId = m_currentDocId;
    m_currentDocId = docId;
    if (oldId >= 0)
        emitDataChangedForRoles(oldId, { Qt::FontRole });
    if (docId >= 0)
        emitDataChangedForRoles(docId, { Qt::FontRole });
}

void DocumentManager::moveDocument(int from, int to)
{
    if (from < 0 || from >= m_docIdOrder.size() || to < 0 || to >= m_docIdOrder.size() || from == to)
        return;
    const int dest = to > from ? to + 1 : to;
    if (beginMoveRows(QModelIndex(), from, from, QModelIndex(), dest)) {
        m_docIdOrder.move(from, to);
        endMoveRows();
    }
}

QIcon DocumentManager::iconForDoc(const Document &doc) const
{
    if (doc.isMonitoringOn)
        return QIcon::fromTheme(u"view-visible"_s);
    if (doc.readOnly)
        return QIcon::fromTheme(u"object-locked"_s, QIcon::fromTheme(u"lock"_s));
    if (doc.contentModified || doc.encodingModified)
        return QIcon(u":/icons/icons/document-save-modified.svg"_s);
    return QIcon::fromTheme(u"document-save"_s);
}

void DocumentManager::emitDataChangedForRoles(int docId, const QVector<int> &roles)
{
    const QModelIndex idx = indexForDocId(docId);
    if (idx.isValid())
        emit dataChanged(idx, idx, roles);
}

QString DocumentManager::generateUntitledName()
{
    // Reuse the lowest number not shown by any open document (e.g. after
    // "Untitled-2" is closed, the next new document becomes "Untitled-2"
    // again instead of growing the counter forever). Titles of saved
    // documents are checked as well so tab titles stay unique, since
    // untitled documents are looked up by title.
    const QString format = tr("Untitled-%1");
    QSet<QString> usedTitles;
    for (auto it = m_documents.constBegin(); it != m_documents.constEnd(); ++it) {
        usedTitles.insert(it.value().title);
    }
    int counter = 1;
    while (usedTitles.contains(format.arg(counter))) {
        ++counter;
    }
    return format.arg(counter);
}

QString DocumentManager::detectLanguageFromExtension(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    const QString fileName = fileInfo.fileName();
    const QString extension = fileInfo.suffix().toLower();

    const LanguageAssociations &assoc = LanguageConfigStore::associations();
    const QString lexerFromFile = assoc.filenameToLanguage.value(fileName.toLower());
    if (!lexerFromFile.isEmpty()) {
        return lexerFromFile;
    }

    const QString lexerFromExt = assoc.extensionToLanguage.value(extension);
    if (!lexerFromExt.isEmpty()) {
        return lexerFromExt;
    }

    const auto kshDef = kshutils::repository().definitionForFileName(fileName);
    if (kshDef.isValid()) {
        return kshDef.name();
    }
    return QString();
}

QString DocumentManager::resolveLexerForLanguage(const QString &language) const
{
    if (language.isEmpty()) {
        return QString();
    }
    const LanguageAssociations &assoc = LanguageConfigStore::associations();
    const QString actual = assoc.languageToLexer.value(language);
    if (!actual.isEmpty()) {
        return actual;
    }

    const auto kshDef = kshutils::repository().definitionForName(language);
    if (kshDef.isValid()) {
        return QStringLiteral("ksyntaxhighlighting");
    }
    return language;
}
