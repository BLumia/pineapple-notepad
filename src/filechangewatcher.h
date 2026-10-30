#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QFileSystemWatcher;
class QTimer;
class QWidget;
class DocumentManager;

enum class PendingFileActionType {
    FileModified,
    FileDeleted
};

// Value assignments must match the fileStatusAutoDetection and
// fileChangeDetectionBackend entries in appsettings.kcfg.
enum class FileStatusAutoDetection {
    Disabled = 0,
    CurrentFile = 1,
    AllOpenedFiles = 2
};

enum class FileChangeDetectionBackend {
    FileSystemWatcher = 0,
    TimestampPolling = 1
};

class FileChangeWatcher : public QObject
{
    Q_OBJECT

public:
    explicit FileChangeWatcher(DocumentManager *documentManager, QObject *parent = nullptr);

    void setDialogParent(QWidget *parent);
    void updateWatchedFileForDocument(int docId);
    void removeWatchedFileForDocument(int docId);
    bool promptAndReloadDocument(int docId, bool promptWhenUnmodified, const QString &title, const QString &modifiedMessage, const QString &unmodifiedMessage, bool defaultYes);
    bool setMonitoring(int docId, bool on);
    bool isDocumentMonitored(int docId) const;
    void setCurrentDocument(int docId);
    void refreshFromFileSettings();

signals:
    void documentReloaded(int docId);
    void closeDocumentRequested(int docId);
    void scrollToEndRequested(int docId, bool wasAtEnd);
    void monitoringChanged(int docId, bool on);

private slots:
    void onApplicationStateChanged(Qt::ApplicationState state);

private:
    void onWatchedFileChanged(const QString &path);
    void onMonitorTimeout();
    void stopMonitoring(int docId);
    void processPendingFileChanges();
    static QString watchKeyForPath(const QString &path);
    bool performReload(int docId);
    bool shouldWatchDoc(int docId) const;
    bool pollingDetectionEnabled() const;
    void checkDocumentFileState(int docId);

    DocumentManager *m_documentManager;
    QFileSystemWatcher *m_fileWatcher = nullptr;
    QHash<int, QString> m_docIdToWatchedPath;
    QHash<QString, int> m_watchedPathToDocId;
    QHash<int, QTimer *> m_monitorTimers;
    QHash<int, PendingFileActionType> m_pendingFileActions;
    QSet<int> m_activePromptDocs;
    QWidget *m_dialogParent = nullptr;
    int m_currentDocId = -1;
};
