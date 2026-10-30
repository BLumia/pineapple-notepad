#include "filechangewatcher.h"

#include "appsettings.h"
#include "document.h"
#include "documentmanager.h"
#include "sciedit.h"

#include <QAbstractButton>
#include <QDateTime>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QStringBuilder>
#include <QTimer>
#include <QWidget>

using namespace Qt::StringLiterals;

namespace {
FileStatusAutoDetection detectionScope()
{
    return static_cast<FileStatusAutoDetection>(AppSettings::fileStatusAutoDetection());
}

FileChangeDetectionBackend detectionBackend()
{
    return static_cast<FileChangeDetectionBackend>(AppSettings::fileChangeDetectionBackend());
}
}

FileChangeWatcher::FileChangeWatcher(DocumentManager *documentManager, QObject *parent)
    : QObject(parent)
    , m_documentManager(documentManager)
{
    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &FileChangeWatcher::onWatchedFileChanged);

    connect(m_documentManager, &DocumentManager::documentOpened, this, [this](int docId, const QString &) {
        updateWatchedFileForDocument(docId);
        // Check the file state right after opening it. The baseline
        // seeded in updateWatchedFileForDocument() makes this a no-op
        // unless the file changed on disk while it was being loaded.
        if (pollingDetectionEnabled()) {
            checkDocumentFileState(docId);
        }
    });
    connect(m_documentManager, &DocumentManager::documentSaved, this, [this](int docId, const QString &) {
        updateWatchedFileForDocument(docId);
    });
    connect(m_documentManager, &DocumentManager::documentClosed, this, [this](int docId) {
        removeWatchedFileForDocument(docId);
        stopMonitoring(docId);
        m_pendingFileActions.remove(docId);
        m_activePromptDocs.remove(docId);
    });

    connect(qApp, &QGuiApplication::applicationStateChanged, this, &FileChangeWatcher::onApplicationStateChanged);
}

void FileChangeWatcher::setDialogParent(QWidget *parent)
{
    m_dialogParent = parent;
}

void FileChangeWatcher::updateWatchedFileForDocument(int docId)
{
    if (!m_fileWatcher) {
        return;
    }

    const QString oldWatchedPath = m_docIdToWatchedPath.value(docId);
    if (!oldWatchedPath.isEmpty()) {
        m_fileWatcher->removePath(oldWatchedPath);
        m_watchedPathToDocId.remove(watchKeyForPath(oldWatchedPath));
        m_docIdToWatchedPath.remove(docId);
    }

    Document *doc = m_documentManager->getDocument(docId);
    if (!doc || doc->untitled || doc->filePath.isEmpty()) {
        return;
    }

    // Seed the disk timestamp baseline unconditionally: the timestamp polling
    // backend needs it even when the document is not subscribed on the watcher.
    if (!doc->lastKnownDiskModifiedAt.isValid()) {
        doc->lastKnownDiskModifiedAt = QFileInfo(doc->filePath).lastModified();
    }

    if (!shouldWatchDoc(docId)) {
        return;
    }

    const QString watchedPath = watchKeyForPath(doc->filePath);
    if (watchedPath.isEmpty()) {
        return;
    }

    if (!m_fileWatcher->addPath(watchedPath)) {
        return;
    }

    m_docIdToWatchedPath.insert(docId, watchedPath);
    m_watchedPathToDocId.insert(watchedPath, docId);
}

bool FileChangeWatcher::pollingDetectionEnabled() const
{
    return detectionBackend() == FileChangeDetectionBackend::TimestampPolling
        && detectionScope() != FileStatusAutoDetection::Disabled;
}

bool FileChangeWatcher::shouldWatchDoc(int docId) const
{
    Document *doc = m_documentManager->getDocument(docId);
    if (!doc || doc->untitled || doc->filePath.isEmpty()) {
        return false;
    }

    // Monitor File (tail -f) is always driven by the file system watcher,
    // regardless of the detection backend and scope settings.
    if (doc->isMonitoringOn) {
        return true;
    }

    if (detectionBackend() == FileChangeDetectionBackend::TimestampPolling) {
        return false;
    }

    switch (detectionScope()) {
    case FileStatusAutoDetection::Disabled:
        return false;
    case FileStatusAutoDetection::CurrentFile:
        return docId == m_currentDocId;
    case FileStatusAutoDetection::AllOpenedFiles:
        return true;
    }

    return false;
}

void FileChangeWatcher::setCurrentDocument(int docId)
{
    if (m_currentDocId == docId) {
        return;
    }

    const int previousDocId = m_currentDocId;
    m_currentDocId = docId;

    if (detectionScope() != FileStatusAutoDetection::CurrentFile) {
        return;
    }

    if (detectionBackend() == FileChangeDetectionBackend::FileSystemWatcher) {
        // Really subscribe only the current document: drop the previous
        // document (unless it is monitored) and pick up the new one.
        if (previousDocId >= 0) {
            updateWatchedFileForDocument(previousDocId);
        }
        if (docId >= 0) {
            updateWatchedFileForDocument(docId);
        }
    }

    // Check the newly activated document on every tab switch. QFileSystemWatcher
    // only reports changes occurring after addPath(), so without this check a
    // modification made while the document was not covered (e.g. a background
    // document in CurrentFile + watcher mode) would go unnoticed until the
    // file changes on disk again.
    if (docId >= 0) {
        checkDocumentFileState(docId);
    }
}

void FileChangeWatcher::refreshFromFileSettings()
{
    if (!m_fileWatcher) {
        return;
    }

    const QStringList watchedPaths = m_fileWatcher->files();
    for (const QString &path : watchedPaths) {
        m_fileWatcher->removePath(path);
    }
    m_docIdToWatchedPath.clear();
    m_watchedPathToDocId.clear();

    const QList<int> docIds = m_documentManager->getAllDocumentIds();
    for (const int docId : docIds) {
        updateWatchedFileForDocument(docId);
    }
}

void FileChangeWatcher::removeWatchedFileForDocument(int docId)
{
    if (!m_fileWatcher) {
        return;
    }

    const QString watchedPath = m_docIdToWatchedPath.take(docId);
    if (watchedPath.isEmpty()) {
        return;
    }

    m_watchedPathToDocId.remove(watchKeyForPath(watchedPath));
    m_fileWatcher->removePath(watchedPath);
}

bool FileChangeWatcher::performReload(int docId)
{
    Document *doc = m_documentManager->getDocument(docId);
    if (!doc || doc->untitled || doc->filePath.isEmpty() || !doc->hasAnyEditor()) {
        return false;
    }

    if (!doc->reload()) {
        return false;
    }

    m_documentManager->setDocumentModified(docId, false);
    m_documentManager->setDocumentEncodingModified(docId, false);

    if (SciEdit *ed = doc->firstEditor()) {
        ed->requestUrlScan();
    }

    emit documentReloaded(docId);
    return true;
}

bool FileChangeWatcher::promptAndReloadDocument(int docId, bool promptWhenUnmodified, const QString &title, const QString &modifiedMessage, const QString &unmodifiedMessage, bool defaultYes)
{
    Document *doc = m_documentManager->getDocument(docId);
    if (!doc || doc->untitled || doc->filePath.isEmpty() || !doc->hasAnyEditor()) {
        return false;
    }

    const bool isModified = m_documentManager->isDocumentModified(docId);
    if (isModified) {
        if (!modifiedMessage.isEmpty()) {
            const QMessageBox::StandardButton reply = QMessageBox::question(
                m_dialogParent,
                title,
                modifiedMessage,
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );
            if (reply != QMessageBox::Yes) {
                return false;
            }
        }
    } else if (promptWhenUnmodified) {
        if (!unmodifiedMessage.isEmpty()) {
            const QMessageBox::StandardButton reply = QMessageBox::question(
                m_dialogParent,
                title,
                unmodifiedMessage,
                QMessageBox::Yes | QMessageBox::No,
                defaultYes ? QMessageBox::Yes : QMessageBox::No
            );
            if (reply != QMessageBox::Yes) {
                return false;
            }
        }
    }

    if (!performReload(docId)) {
        QMessageBox::warning(m_dialogParent, title, tr("Failed to reload file from disk."));
        return false;
    }
    return true;
}

bool FileChangeWatcher::setMonitoring(int docId, bool on)
{
    Document *doc = m_documentManager->getDocument(docId);
    if (!doc) {
        return false;
    }

    if (on) {
        if (doc->untitled || doc->filePath.isEmpty()) {
            return false;
        }
        if (m_documentManager->isDocumentModified(docId)) {
            QMessageBox::information(
                m_dialogParent,
                tr("Monitoring"),
                tr("The document has unsaved changes. Please save before monitoring.")
            );
            return false;
        }
        if (!QFileInfo::exists(doc->filePath)) {
            QMessageBox::information(
                m_dialogParent,
                tr("Monitoring"),
                tr("The file must exist on disk to be monitored.")
            );
            return false;
        }

        doc->isMonitoringOn = true;
        for (const auto &ref : doc->editorReferences()) {
            if (ref.editor) {
                ref.editor->setReadOnly(true);
            }
        }

        // Monitoring is always watcher-driven: make sure the path is
        // subscribed even when the detection settings would not cover it.
        updateWatchedFileForDocument(docId);

        QTimer *timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, &FileChangeWatcher::onMonitorTimeout);
        m_monitorTimers.insert(docId, timer);

        emit monitoringChanged(docId, true);
        return true;
    } else {
        stopMonitoring(docId);
        return true;
    }
}

bool FileChangeWatcher::isDocumentMonitored(int docId) const
{
    Document *doc = m_documentManager->getDocument(docId);
    return doc && doc->isMonitoringOn;
}

void FileChangeWatcher::stopMonitoring(int docId)
{
    Document *doc = m_documentManager->getDocument(docId);
    if (!doc || !doc->isMonitoringOn) {
        return;
    }

    doc->isMonitoringOn = false;
    for (const auto &ref : doc->editorReferences()) {
        if (ref.editor) {
            ref.editor->setReadOnly(doc->readOnly);
        }
    }

    if (QTimer *timer = m_monitorTimers.take(docId)) {
        timer->stop();
        delete timer;
    }

    // Drop the watcher subscription again if the detection settings no
    // longer cover this document (e.g. it is not the current file).
    updateWatchedFileForDocument(docId);

    emit monitoringChanged(docId, false);
}

void FileChangeWatcher::onMonitorTimeout()
{
    QTimer *timer = qobject_cast<QTimer *>(sender());
    if (!timer) {
        return;
    }

    int docId = m_monitorTimers.key(timer, -1);
    if (docId < 0) {
        return;
    }

    Document *doc = m_documentManager->getDocument(docId);
    if (!doc || !doc->hasAnyEditor()) {
        return;
    }

    SciEdit *ed = doc->firstEditor();

    bool wasAtEnd = false;
    if (ed) {
        const sptr_t lastVisibleLine = ed->firstVisibleLine() + ed->linesOnScreen();
        const sptr_t totalLines = ed->lineCount();
        wasAtEnd = lastVisibleLine >= totalLines - 1;
    }

    for (const auto &ref : doc->editorReferences()) {
        if (ref.editor) {
            ref.editor->setReadOnly(false);
        }
    }

    if (!doc->reload()) {
        for (const auto &ref : doc->editorReferences()) {
            if (ref.editor) {
                ref.editor->setReadOnly(true);
            }
        }
        return;
    }

    for (const auto &ref : doc->editorReferences()) {
        if (ref.editor) {
            ref.editor->setReadOnly(true);
        }
    }

    m_documentManager->setDocumentModified(docId, false);
    m_documentManager->setDocumentEncodingModified(docId, false);

    if (ed) {
        ed->requestUrlScan();
    }

    emit documentReloaded(docId);
    emit scrollToEndRequested(docId, wasAtEnd);
}

void FileChangeWatcher::onWatchedFileChanged(const QString &path)
{
    const QString key = watchKeyForPath(path);
    const int docId = m_watchedPathToDocId.value(key, -1);
    if (docId < 0) {
        return;
    }

    Document *doc = m_documentManager->getDocument(docId);
    if (!doc || doc->untitled) {
        return;
    }

    const QString watchedPath = m_docIdToWatchedPath.value(docId);
    if (watchedPath.isEmpty()) {
        return;
    }

    // Some watcher backends stop reporting after the file is replaced via
    // rename; subscribe again if the path was dropped while the file lives on.
    if (QFileInfo::exists(watchedPath) && !m_fileWatcher->files().contains(watchedPath)) {
        m_fileWatcher->addPath(watchedPath);
    }

    checkDocumentFileState(docId);
}

void FileChangeWatcher::checkDocumentFileState(int docId)
{
    Document *doc = m_documentManager->getDocument(docId);
    if (!doc || doc->untitled) {
        return;
    }

    QString path = m_docIdToWatchedPath.value(docId);
    if (path.isEmpty()) {
        // Documents checked by the polling backend are not subscribed on the
        // watcher; derive the path from the document instead.
        path = watchKeyForPath(doc->filePath);
    }
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo fi(path);
    if (!fi.exists()) {
        if (doc->isMonitoringOn) {
            stopMonitoring(docId);
            return;
        }

        m_pendingFileActions[docId] = PendingFileActionType::FileDeleted;
        if (QGuiApplication::applicationState() == Qt::ApplicationActive) {
            processPendingFileChanges();
        }
        return;
    }

    const QDateTime diskModified = fi.lastModified();
    if (!diskModified.isValid()) {
        return;
    }

    if (doc->isLoading) {
        doc->lastKnownDiskModifiedAt = diskModified;
        return;
    }
    if (doc->isSaving) {
        doc->lastKnownDiskModifiedAt = diskModified;
        return;
    }

    if (!doc->lastKnownDiskModifiedAt.isValid() || diskModified <= doc->lastKnownDiskModifiedAt) {
        doc->lastKnownDiskModifiedAt = diskModified;
        return;
    }

    doc->lastKnownDiskModifiedAt = diskModified;

    if (doc->isMonitoringOn) {
        if (QTimer *timer = m_monitorTimers.value(docId)) {
            if (!timer->isActive()) {
                timer->start(250);
            }
        }
        return;
    }

    m_pendingFileActions[docId] = PendingFileActionType::FileModified;
    if (QGuiApplication::applicationState() == Qt::ApplicationActive) {
        processPendingFileChanges();
    }
}

void FileChangeWatcher::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (state != Qt::ApplicationActive) {
        return;
    }

    if (pollingDetectionEnabled()) {
        QList<int> docIds;
        if (detectionScope() == FileStatusAutoDetection::CurrentFile) {
            if (m_currentDocId >= 0) {
                docIds.append(m_currentDocId);
            }
        } else {
            docIds = m_documentManager->getAllDocumentIds();
        }

        for (const int docId : std::as_const(docIds)) {
            // Monitored documents are driven by the watcher; skip them here
            // to avoid double handling.
            const Document *doc = m_documentManager->getDocument(docId);
            if (!doc || doc->isMonitoringOn) {
                continue;
            }
            checkDocumentFileState(docId);
        }
    }

    if (!m_pendingFileActions.isEmpty()) {
        processPendingFileChanges();
    }
}

void FileChangeWatcher::processPendingFileChanges()
{
    const auto pending = m_pendingFileActions;
    m_pendingFileActions.clear();

    for (auto it = pending.begin(); it != pending.end(); ++it) {
        const int docId = it.key();
        const PendingFileActionType actionType = it.value();

        Document *doc = m_documentManager->getDocument(docId);
        if (!doc || doc->untitled) {
            continue;
        }

        if (m_activePromptDocs.contains(docId)) {
            continue;
        }

        if (actionType == PendingFileActionType::FileDeleted) {
            QString path = m_docIdToWatchedPath.value(docId);
            if (path.isEmpty()) {
                path = watchKeyForPath(doc->filePath);
            }
            const QString fileName = QFileInfo(path).fileName();
            const QString title = tr("Keep Non-existing File");
            const QString message = tr("The file \"%1\" doesn't exist anymore.\nKeep this file in editor?").arg(fileName);

            m_activePromptDocs.insert(docId);
            const QMessageBox::StandardButton reply = QMessageBox::question(
                m_dialogParent,
                title,
                message,
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes
            );
            m_activePromptDocs.remove(docId);

            if (reply == QMessageBox::No) {
                emit closeDocumentRequested(docId);
            } else {
                m_documentManager->setDocumentModified(docId, true);
            }
        } else if (actionType == PendingFileActionType::FileModified) {
            QString path = m_docIdToWatchedPath.value(docId);
            if (path.isEmpty()) {
                path = watchKeyForPath(doc->filePath);
            }
            const QString fileName = QFileInfo(path).fileName();
            const bool isModified = m_documentManager->isDocumentModified(docId);

            if (AppSettings::fileUpdateSilently() && !isModified) {
                // Silent update: reload without asking, but only for documents without unsaved
                // changes - dirty documents still ask.
                if (!performReload(docId)) {
                    QMessageBox::warning(m_dialogParent, tr("Reload Document"), tr("Failed to reload file from disk."));
                } else if (AppSettings::fileUpdateScrollToEnd()) {
                    emit scrollToEndRequested(docId, true);
                }
                continue;
            }

            const QString title = tr("Reload Document");
            const QString warning =
                isModified ? (u"\n"_s % tr("Reloading will discard your unsaved changes.")) : QString();
            const QString message =
                tr(R"(The file "%1" has been modified on disk.)").arg(fileName) %
                warning %
                u"\n\n"_s %
                tr("Reload now, or reload and monitor (tail -f) for further changes?");

            QMessageBox msgBox(m_dialogParent);
            msgBox.setWindowTitle(title);
            msgBox.setIcon(QMessageBox::Question);
            msgBox.setText(message);
            QPushButton *reloadMonitorBtn = msgBox.addButton(tr("Reload & Monitor"), QMessageBox::AcceptRole);
            reloadMonitorBtn->setIcon(QIcon::fromTheme("view-visible"));
            QPushButton *reloadBtn = msgBox.addButton(tr("Reload"), QMessageBox::AcceptRole);
            reloadBtn->setIcon(QIcon::fromTheme("dialog-ok"));
            QPushButton *cancelBtn = msgBox.addButton(tr("Don't Reload"), QMessageBox::RejectRole);
            cancelBtn->setIcon(QIcon::fromTheme("dialog-close"));
            msgBox.setDefaultButton(reloadBtn);

            m_activePromptDocs.insert(docId);
            msgBox.exec();
            m_activePromptDocs.remove(docId);

            QAbstractButton *clicked = msgBox.clickedButton();
            if (clicked == reloadBtn || clicked == reloadMonitorBtn) {
                if (!performReload(docId)) {
                    QMessageBox::warning(m_dialogParent, title, tr("Failed to reload file from disk."));
                } else {
                    if (AppSettings::fileUpdateScrollToEnd()) {
                        emit scrollToEndRequested(docId, true);
                    }
                    if (clicked == reloadMonitorBtn) {
                        setMonitoring(docId, true);
                    }
                }
            }
        }
    }
}

QString FileChangeWatcher::watchKeyForPath(const QString &path)
{
    if (path.isEmpty()) {
        return QString();
    }

    const QFileInfo fi(path);
    return fi.canonicalFilePath().isEmpty() ? fi.absoluteFilePath() : fi.canonicalFilePath();
}
