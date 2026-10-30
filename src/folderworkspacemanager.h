#pragma once

#include <QObject>
#include <QStringList>

class FolderWorkspaceManager : public QObject
{
    Q_OBJECT

public:
    explicit FolderWorkspaceManager(QObject *parent = nullptr);

    void addWorkspace(const QString &path);
    void removeWorkspace(const QString &path);
    void switchTo(const QString &path);

    QString currentWorkspace() const;
    QStringList workspaces() const;
    bool isEmpty() const;
    int count() const;

signals:
    void workspaceAdded(const QString &path);
    void workspaceRemoved(const QString &path);
    void currentWorkspaceChanged(const QString &path);
    void allWorkspacesClosed();

private:
    QStringList m_workspacePaths;
    QString m_currentWorkspace;
};
