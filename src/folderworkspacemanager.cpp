#include "folderworkspacemanager.h"

FolderWorkspaceManager::FolderWorkspaceManager(QObject *parent)
    : QObject(parent)
{
}

void FolderWorkspaceManager::addWorkspace(const QString &path)
{
    if (m_workspacePaths.contains(path)) {
        switchTo(path);
        return;
    }

    m_workspacePaths.append(path);
    m_currentWorkspace = path;
    emit workspaceAdded(path);
    emit currentWorkspaceChanged(path);
}

void FolderWorkspaceManager::removeWorkspace(const QString &path)
{
    if (!m_workspacePaths.contains(path)) {
        return;
    }

    m_workspacePaths.removeOne(path);

    if (m_currentWorkspace == path) {
        if (m_workspacePaths.isEmpty()) {
            m_currentWorkspace.clear();
            emit workspaceRemoved(path);
            emit allWorkspacesClosed();
        } else {
            m_currentWorkspace = m_workspacePaths.last();
            emit workspaceRemoved(path);
            emit currentWorkspaceChanged(m_currentWorkspace);
        }
    } else {
        emit workspaceRemoved(path);
    }
}

void FolderWorkspaceManager::switchTo(const QString &path)
{
    if (m_currentWorkspace == path) {
        return;
    }
    if (!m_workspacePaths.contains(path)) {
        return;
    }
    m_currentWorkspace = path;
    emit currentWorkspaceChanged(path);
}

QString FolderWorkspaceManager::currentWorkspace() const
{
    return m_currentWorkspace;
}

QStringList FolderWorkspaceManager::workspaces() const
{
    return m_workspacePaths;
}

bool FolderWorkspaceManager::isEmpty() const
{
    return m_workspacePaths.isEmpty();
}

int FolderWorkspaceManager::count() const
{
    return m_workspacePaths.size();
}
