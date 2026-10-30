#include "folderworkspacedock.h"
#include "ui_folderworkspacedock.h"
#include "folderworkspacemanager.h"

#include <QComboBox>
#include <QFileSystemModel>
#include <QPushButton>
#include <QToolButton>

FolderWorkspaceDock::FolderWorkspaceDock(QWidget *parent)
    : QDockWidget(parent)
    , ui(new Ui::FolderWorkspaceDock)
{
    ui->setupUi(this);

    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setReadOnly(true);

    ui->treeView->setModel(m_fileModel);
    ui->treeView->setRootIndex(m_fileModel->index(QDir::rootPath()));
    ui->treeView->hideColumn(1);
    ui->treeView->hideColumn(2);
    ui->treeView->hideColumn(3);

    connect(ui->treeView, &QTreeView::doubleClicked, this, &FolderWorkspaceDock::onItemDoubleClicked);
    connect(ui->selectFolderButton, &QPushButton::clicked, this, &FolderWorkspaceDock::selectFolderRequested);
    connect(ui->useCurrentDocDirButton, &QPushButton::clicked, this, &FolderWorkspaceDock::useCurrentDocumentDirectoryRequested);
    connect(ui->useCurrentDocGitRootButton, &QPushButton::clicked, this, &FolderWorkspaceDock::useCurrentDocumentGitRootRequested);

    connect(ui->expandAllButton, &QToolButton::clicked, ui->treeView, &QTreeView::expandAll);
    connect(ui->collapseAllButton, &QToolButton::clicked, ui->treeView, &QTreeView::collapseAll);
    connect(ui->closeWorkspaceButton, &QToolButton::clicked, this, &FolderWorkspaceDock::removeCurrentWorkspace);

    connect(ui->workspaceCombo, &QComboBox::currentIndexChanged, this, &FolderWorkspaceDock::onComboIndexChanged);

    ui->stackedWidget->setCurrentIndex(0);
}

FolderWorkspaceDock::~FolderWorkspaceDock()
{
    delete ui;
}

void FolderWorkspaceDock::setManager(FolderWorkspaceManager *manager)
{
    m_manager = manager;

    connect(m_manager, &FolderWorkspaceManager::workspaceAdded, this, [this](const QString &path) {
        m_syncingCombo = true;
        ui->workspaceCombo->addItem(QFileInfo(path).fileName());
        ui->workspaceCombo->setItemData(ui->workspaceCombo->count() - 1, path, Qt::ToolTipRole);
        m_syncingCombo = false;
    });

    connect(m_manager, &FolderWorkspaceManager::workspaceRemoved, this, [this](const QString &path) {
        m_syncingCombo = true;
        for (int i = 0; i < ui->workspaceCombo->count(); ++i) {
            if (ui->workspaceCombo->itemData(i, Qt::ToolTipRole).toString() == path) {
                ui->workspaceCombo->removeItem(i);
                break;
            }
        }
        m_syncingCombo = false;
    });

    connect(m_manager, &FolderWorkspaceManager::currentWorkspaceChanged, this, [this](const QString &path) {
        setRootPath(path);
        m_syncingCombo = true;
        for (int i = 0; i < ui->workspaceCombo->count(); ++i) {
            if (ui->workspaceCombo->itemData(i, Qt::ToolTipRole).toString() == path) {
                ui->workspaceCombo->setCurrentIndex(i);
                break;
            }
        }
        m_syncingCombo = false;
    });

    connect(m_manager, &FolderWorkspaceManager::allWorkspacesClosed, this, &FolderWorkspaceDock::showPlaceholder);
}

void FolderWorkspaceDock::addWorkspace(const QString &path)
{
    if (m_manager) {
        m_manager->addWorkspace(path);
    }
}

void FolderWorkspaceDock::removeCurrentWorkspace()
{
    if (m_manager && !m_manager->currentWorkspace().isEmpty()) {
        m_manager->removeWorkspace(m_manager->currentWorkspace());
    }
}

QString FolderWorkspaceDock::currentWorkspace() const
{
    return m_manager ? m_manager->currentWorkspace() : QString();
}

bool FolderWorkspaceDock::hasWorkspace() const
{
    return m_manager && !m_manager->isEmpty();
}

void FolderWorkspaceDock::setRootPath(const QString &path)
{
    const QModelIndex rootIndex = m_fileModel->setRootPath(path);
    ui->treeView->setRootIndex(rootIndex);
    ui->treeView->expandToDepth(0);
    ui->treeView->collapseAll();
    ui->workspaceCombo->setToolTip(path);
    ui->stackedWidget->setCurrentIndex(1);
}

void FolderWorkspaceDock::showPlaceholder()
{
    ui->workspaceCombo->clear();
    ui->stackedWidget->setCurrentIndex(0);
}

void FolderWorkspaceDock::onItemDoubleClicked(const QModelIndex &index)
{
    if (!m_fileModel->isDir(index)) {
        emit fileDoubleClicked(m_fileModel->filePath(index));
    }
}

void FolderWorkspaceDock::onComboIndexChanged(int index)
{
    if (m_syncingCombo || index < 0 || !m_manager) {
        return;
    }
    const QString path = ui->workspaceCombo->itemData(index, Qt::ToolTipRole).toString();
    if (!path.isEmpty()) {
        m_manager->switchTo(path);
    }
}
