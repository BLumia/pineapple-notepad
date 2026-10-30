#pragma once

#include <KConfigGroup>
#include <KSharedConfig>

#include <QObject>
#include <QHash>
#include <functional>

class DocumentManager;
class EditorViewManager;

class SessionManager : public QObject
{
    Q_OBJECT

public:
    explicit SessionManager(DocumentManager *documentManager, QObject *parent = nullptr);

    void saveSession(EditorViewManager *viewManager, bool writeKConfig = true);
    bool restoreSession(EditorViewManager *viewManager, QWidget *parentWidget,
                        std::function<int(const QString &)> openFunc,
                        std::function<int()> newFunc);
    void clearSession();
    void removeBackupForDoc(int docId);

    static QString sessionDirectoryPath();

signals:
    void recentFileOpened(const QString &filePath);

private:
    static void clearBackupDirectoryFiles();
    static KConfigGroup sessionConfigGroup();
    QString backupFilePathForDoc(int docId, const QString &baseName);
    void backupSnapshots(EditorViewManager *viewManager);
    void writeSessionToKConfig(EditorViewManager *viewManager);

    DocumentManager *m_documentManager;
    QHash<int, QString> m_backupFileNames;
};
