#pragma once

#include <QDockWidget>

namespace Ui {
class FolderWorkspaceDock;
}

class QFileSystemModel;
class FolderWorkspaceManager;

class FolderWorkspaceDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit FolderWorkspaceDock(QWidget *parent = nullptr);
    ~FolderWorkspaceDock() override;

    void setManager(FolderWorkspaceManager *manager);

    void addWorkspace(const QString &path);
    void removeCurrentWorkspace();
    QString currentWorkspace() const;
    bool hasWorkspace() const;

signals:
    void fileDoubleClicked(const QString &filePath);
    void selectFolderRequested();
    void useCurrentDocumentDirectoryRequested();
    void useCurrentDocumentGitRootRequested();

private:
    void setRootPath(const QString &path);
    void showPlaceholder();
    void onItemDoubleClicked(const QModelIndex &index);
    void onComboIndexChanged(int index);

    Ui::FolderWorkspaceDock *ui = nullptr;
    QFileSystemModel *m_fileModel = nullptr;
    FolderWorkspaceManager *m_manager = nullptr;
    bool m_syncingCombo = false;
};
