#include "sessionmanager.h"

#include "appsettings.h"
#include "sciedit.h"
#include "kshlexer.h"
#include "editorviewmanager.h"
#include "documentmanager.h"
#include "apppaths.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QDateTime>

#include <Scintilla.h>

Q_LOGGING_CATEGORY(lcSessionManager, "pnotepad.sessionmanager")

using namespace Qt::Literals::StringLiterals;

SessionManager::SessionManager(DocumentManager *documentManager, QObject *parent)
    : QObject(parent)
    , m_documentManager(documentManager)
{
}

QString SessionManager::sessionDirectoryPath()
{
    return AppPaths::appDataSubdirPath(QStringLiteral("session"));
}

// TODO: this is currently unused, maybe we need to add an option to allow full backup on closed
//       (i.e. clear all existing backup files and create new ones). This might be helpful to
//       regenerate backup file's file name with a more proper timestamp and reflex the file name.
void SessionManager::clearBackupDirectoryFiles()
{
    const QString dirPath = sessionDirectoryPath();
    QDir d(dirPath);
    const QFileInfoList entries = d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        QFile::remove(fi.absoluteFilePath());
    }
}

QString SessionManager::backupFilePathForDoc(int docId, const QString &baseName)
{
    if (m_backupFileNames.contains(docId)) {
        return m_backupFileNames.value(docId);
    }
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss"));
    const QString name = QStringLiteral("%1@%2@%3").arg(baseName, timestamp).arg(docId);
    const QString fullPath = QDir(sessionDirectoryPath()).absoluteFilePath(name);
    m_backupFileNames.insert(docId, fullPath);
    return fullPath;
}

void SessionManager::removeBackupForDoc(int docId)
{
    auto it = m_backupFileNames.find(docId);
    if (it == m_backupFileNames.end()) {
        return;
    }
    QFile::remove(it.value());
    m_backupFileNames.erase(it);
}

KConfigGroup SessionManager::sessionConfigGroup()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    return KConfigGroup(config, QStringLiteral("Session"));
}

void SessionManager::saveSession(EditorViewManager *viewManager, bool writeKConfig)
{
    qCDebug(lcSessionManager) << "Saving session...";
    backupSnapshots(viewManager);
    if (writeKConfig) {
        writeSessionToKConfig(viewManager);
    }
}

void SessionManager::writeSessionToKConfig(EditorViewManager *viewManager)
{
    KConfigGroup group = sessionConfigGroup();
    group.deleteGroup();
    group = sessionConfigGroup();

    const QVector<int> docIds = viewManager->documentIds();
    const int activeDocId = viewManager->currentDocumentId();
    int storedCount = 0;
    int activeStoredIndex = 0;

    for (int docId : docIds) {
        if (docId < 0) {
            continue;
        }

        SciEdit *editor = viewManager->editorForDocId(docId);
        if (!editor) {
            continue;
        }

        const QString filePath = m_documentManager->getDocumentFilePath(docId);
        const bool isUntitled = m_documentManager->isDocumentUntitled(docId);
        const bool isModified = m_documentManager->isDocumentModified(docId);
        const bool isUntouchedEmptyUntitled = isUntitled && !isModified && editor->textLength() == 0;
        if (isUntouchedEmptyUntitled) {
            continue;
        }

        if (docId == activeDocId) {
            activeStoredIndex = storedCount;
        }

        const QString keyPrefix = QStringLiteral("doc%1_").arg(storedCount);
        const int cursorPos = static_cast<int>(editor->currentPos());
        const int firstVisibleLine = static_cast<int>(editor->docLineFromVisible(editor->firstVisibleLine()));
        const QString language = m_documentManager->getDocumentLanguage(docId);
        const QString lexerLanguage = QString::fromUtf8(editor->lexerLanguage());
        const int eolMode = static_cast<int>(editor->eOLMode());

        const QString snapshotFile = m_backupFileNames.value(docId);
        const bool hasSnapshot = !snapshotFile.isEmpty();

        group.writeEntry(keyPrefix + QStringLiteral("path"), filePath);
        group.writeEntry(keyPrefix + QStringLiteral("isUntitled"), isUntitled);
        group.writeEntry(keyPrefix + QStringLiteral("isModified"), isModified);
        group.writeEntry(keyPrefix + QStringLiteral("cursorPos"), cursorPos);
        group.writeEntry(keyPrefix + QStringLiteral("firstVisibleLine"), firstVisibleLine);
        group.writeEntry(keyPrefix + QStringLiteral("language"), language);
        group.writeEntry(keyPrefix + QStringLiteral("lexerLanguage"), lexerLanguage);
        group.writeEntry(keyPrefix + QStringLiteral("eolMode"), eolMode);
        group.writeEntry(keyPrefix + QStringLiteral("hasSnapshot"), hasSnapshot);
        group.writeEntry(keyPrefix + QStringLiteral("snapshotFile"), snapshotFile);

        if (hasSnapshot && !isUntitled && !filePath.isEmpty()) {
            const QFileInfo fi(filePath);
            if (fi.exists()) {
                const QDateTime lastModified = fi.lastModified();
                if (lastModified.isValid()) {
                    group.writeEntry(keyPrefix + QStringLiteral("lastModifiedTimestamp"), lastModified.toMSecsSinceEpoch());
                }
            }
        }

        ++storedCount;
    }

    group.writeEntry(QStringLiteral("count"), storedCount);
    group.writeEntry(QStringLiteral("activeIndex"), (storedCount > 0) ? qBound(0, activeStoredIndex, storedCount - 1) : 0);
    group.sync();
}

void SessionManager::backupSnapshots(EditorViewManager *viewManager)
{
    qCDebug(lcSessionManager) << "Backup snapshots...";

    const QVector<int> docIds = viewManager->documentIds();
    QSet<int> activeDocIds;

    for (int docId : docIds) {
        if (docId < 0) {
            continue;
        }

        SciEdit *editor = viewManager->editorForDocId(docId);
        if (!editor) {
            continue;
        }

        const bool isUntitled = m_documentManager->isDocumentUntitled(docId);
        const bool isModified = m_documentManager->isDocumentModified(docId);
        const bool needsBackup = isUntitled || isModified;
        if (!needsBackup) {
            continue;
        }

        const bool isUntouchedEmptyUntitled = isUntitled && !isModified && editor->textLength() == 0;
        if (isUntouchedEmptyUntitled) {
            continue;
        }

        activeDocIds.insert(docId);

        QString baseName;
        if (isUntitled) {
            baseName = m_documentManager->getDocumentTitle(docId);
            if (baseName.isEmpty()) {
                baseName = QStringLiteral("new");
            }
        } else {
            const QString filePath = m_documentManager->getDocumentFilePath(docId);
            baseName = QFileInfo(filePath).fileName();
        }

        const QString backupFile = backupFilePathForDoc(docId, baseName);

        const QByteArray bytes = editor->getText(editor->textLength());
        QSaveFile f(backupFile);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(bytes);
            f.commit();
        }
    }

    auto it = m_backupFileNames.begin();
    while (it != m_backupFileNames.end()) {
        if (!activeDocIds.contains(it.key())) {
            QFile::remove(it.value());
            it = m_backupFileNames.erase(it);
        } else {
            ++it;
        }
    }

    if (AppSettings::saveSessionOnBackup()) {
        writeSessionToKConfig(viewManager);
    }
}

bool SessionManager::restoreSession(EditorViewManager *viewManager, QWidget *parentWidget,
                                   std::function<int(const QString &)> openFunc,
                                   std::function<int()> newFunc)
{
    const KConfigGroup group = sessionConfigGroup();
    const int count = group.readEntry(QStringLiteral("count"), 0);
    if (count <= 0) {
        return false;
    }

    int opened = 0;
    QSet<QString> openedFileKeys;
    const QString dirPath = sessionDirectoryPath();
    QDir sessionDir(dirPath);

    for (int i = 0; i < count; ++i) {
        const QString keyPrefix = QStringLiteral("doc%1_").arg(i);
        const QString filePath = group.readEntry(keyPrefix + QStringLiteral("path"), QString());
        const bool hasSnapshot = group.readEntry(keyPrefix + QStringLiteral("hasSnapshot"), false);
        const QString snapshotFile = group.readEntry(keyPrefix + QStringLiteral("snapshotFile"), QString());
        const bool isUntitled = group.readEntry(keyPrefix + QStringLiteral("isUntitled"), false);
        const bool wasModified = group.readEntry(keyPrefix + QStringLiteral("isModified"), false);
        const int cursorPos = group.readEntry(keyPrefix + QStringLiteral("cursorPos"), 0);
        const int firstVisibleLine = group.readEntry(keyPrefix + QStringLiteral("firstVisibleLine"), 0);
        const QString language = group.readEntry(keyPrefix + QStringLiteral("language"), QString());
        const QString lexerLanguage = group.readEntry(keyPrefix + QStringLiteral("lexerLanguage"), QString());
        const int eolMode = group.readEntry(keyPrefix + QStringLiteral("eolMode"), -1);
        const qint64 savedLastModifiedTimestamp = group.readEntry(keyPrefix + QStringLiteral("lastModifiedTimestamp"), -1LL);

        const bool hasBackup = hasSnapshot && !snapshotFile.isEmpty() &&
            QFile::exists(QFileInfo(snapshotFile).isAbsolute() ? snapshotFile : sessionDir.filePath(snapshotFile));

        int docId = -1;
        bool externalModified = false;
        QFileInfo fileInfo(filePath);

        if (!filePath.isEmpty() && fileInfo.exists()) {
            const QString fileKey = DocumentManager::normalizeFilePath(filePath);
            if (openedFileKeys.contains(fileKey)) {
                continue;
            }
            openedFileKeys.insert(fileKey);

            if (hasBackup && savedLastModifiedTimestamp > 0) {
                const QDateTime currentLastModified = fileInfo.lastModified();
                if (currentLastModified.isValid()) {
                    const qint64 currentTimestamp = currentLastModified.toMSecsSinceEpoch();
                    if (currentTimestamp != savedLastModifiedTimestamp) {
                        externalModified = true;
                    }
                }
            }

            docId = openFunc(fileKey);
            if (docId >= 0) {
                Q_EMIT recentFileOpened(fileKey);
            }
        } else if (hasBackup || isUntitled) {
            docId = newFunc();
            if (docId >= 0 && !filePath.isEmpty()) {
                m_documentManager->setDocumentFilePath(docId, filePath);
            }
        } else {
            continue;
        }

        if (docId < 0) {
            continue;
        }

        SciEdit *editor = viewManager->editorForDocId(docId);
        if (!editor) {
            continue;
        }

        bool useBackup = hasBackup;
        bool shouldMarkModified = wasModified;
        if (hasBackup && externalModified) {
            const QString title = tr("Session Restore");
            const QString fileName = fileInfo.fileName();
            const QString message = tr("The file \"%1\" has been modified externally since the last session.\n\n"
                "Do you want to reload it from disk? Your unsaved changes from the previous session will be lost.")
                .arg(fileName);
            const QMessageBox::StandardButton reply = QMessageBox::question(
                parentWidget, title, message,
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );
            if (reply == QMessageBox::Yes) {
                useBackup = false;
                shouldMarkModified = false;
                Document *doc = m_documentManager->getDocument(docId);
                if (doc) {
                    doc->lastKnownDiskModifiedAt = fileInfo.lastModified();
                }
            }
        }

        if (useBackup) {
            const QString backupPath = QFileInfo(snapshotFile).isAbsolute()
                ? snapshotFile
                : sessionDir.filePath(snapshotFile);
            QFile f(backupPath);
            if (f.open(QIODevice::ReadOnly)) {
                const QByteArray bytes = f.readAll();
                editor->setUndoCollection(false);
                editor->clearAll();
                if (!bytes.isEmpty()) {
                    editor->addText(bytes.size(), bytes.constData());
                }
                editor->setUndoCollection(true);
                editor->emptyUndoBuffer();
                m_backupFileNames.insert(docId, backupPath);
            }
        }

        if (eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF || eolMode == SC_EOL_CR) {
            editor->setEOLMode(eolMode);
        }

        if (language.isEmpty()) {
            editor->setLexer(nullptr, QString(), false);
            m_documentManager->setDocumentLanguage(docId, QString());
        } else if (lexerLanguage == QStringLiteral("ksyntaxhighlighting")) {
            Scintilla::ILexer5 *lexer = createKshLexer();
            lexer->PropertySet("lexer.ksyntaxhighlighting.definition", language.toUtf8().constData());
            editor->setLexer(lexer, language, false);
            m_documentManager->setDocumentLanguage(docId, language);
        } else {
            DocumentManager::applyLexerToEditor(editor, language, m_documentManager, false, false);
            m_documentManager->setDocumentLanguage(docId, language);
        }

        if (cursorPos > 0) {
            editor->gotoPos(cursorPos);
        }
        editor->setFirstVisibleLine(static_cast<int>(editor->visibleFromDocLine(firstVisibleLine)));
        editor->setXOffset(0);

        m_documentManager->setDocumentModified(docId, shouldMarkModified);

        ++opened;
    }

    if (opened <= 0) {
        return false;
    }

    const int activeIndex = group.readEntry(QStringLiteral("activeIndex"), 0);
    const QVector<int> docIds = viewManager->documentIds();
    if (activeIndex >= 0 && activeIndex < docIds.size()) {
        if (SciEdit *editor = viewManager->editorForDocId(docIds[activeIndex])) {
            viewManager->switchToEditor(editor);
            editor->colourise(0, -1);
        }
    }

    return true;
}

void SessionManager::clearSession()
{
    m_backupFileNames.clear();
    KConfigGroup group = sessionConfigGroup();
    group.deleteGroup();
    group.sync();
}
