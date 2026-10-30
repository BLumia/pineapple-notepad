#include "documentlistdock.h"
#include "ui_documentlistdock.h"
#include "documentmanager.h"
#include "editorviewmanager.h"

#include <QKeyEvent>
#include <QMouseEvent>

DocumentListDock::DocumentListDock(QWidget *parent)
    : QDockWidget(parent)
    , ui(new Ui::DocumentListDock)
{
    ui->setupUi(this);

    ui->treeView->viewport()->installEventFilter(this);
    ui->treeView->installEventFilter(this);

    connect(ui->treeView, &QTreeView::clicked, this, [this](const QModelIndex &idx) {
        const int docId = idx.data(DocumentManager::DocIdRole).toInt();
        if (docId >= 0)
            emit documentActivated(docId);
    });

    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->treeView, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const QModelIndex idx = ui->treeView->indexAt(pos);
        if (!idx.isValid())
            return;
        const int docId = idx.data(DocumentManager::DocIdRole).toInt();
        if (docId >= 0)
            emit contextMenuRequested(docId, ui->treeView->viewport()->mapToGlobal(pos));
    });
}

DocumentListDock::~DocumentListDock() = default;

void DocumentListDock::setDocumentManager(DocumentManager *dm)
{
    m_dm = dm;
    ui->treeView->setModel(dm);
}

void DocumentListDock::setEditorViewManager(EditorViewManager *vm)
{
    Q_UNUSED(vm);
}

void DocumentListDock::setCurrentDocId(int docId)
{
    if (m_dm)
        m_dm->setCurrentDocId(docId);
}

bool DocumentListDock::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->treeView->viewport() && event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::MiddleButton) {
            const QModelIndex idx = ui->treeView->indexAt(me->pos());
            if (idx.isValid()) {
                const int docId = idx.data(DocumentManager::DocIdRole).toInt();
                if (docId >= 0)
                    emit closeRequested(docId);
                return true;
            }
        }
    } else if (watched == ui->treeView && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            const QModelIndex idx = ui->treeView->currentIndex();
            if (idx.isValid()) {
                const int docId = idx.data(DocumentManager::DocIdRole).toInt();
                if (docId >= 0)
                    emit documentActivated(docId);
                return true;
            }
        }
    }
    return QDockWidget::eventFilter(watched, event);
}
