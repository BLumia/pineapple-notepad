#include "quickswitcher.h"
#include "documentmanager.h"
#include "recenttagproxymodel.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QMouseEvent>

QuickSwitcher::QuickSwitcher(DocumentManager *dm, QWidget *parent)
    : QTreeView(parent)
    , m_dm(dm)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setRootIsDecorated(false);
    setHeaderHidden(true);
    setUniformRowHeights(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setTextElideMode(Qt::ElideMiddle);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_proxy = new RecentTagProxyModel(this);
    m_proxy->setSourceModel(m_dm);
    setModel(m_proxy);
}

void QuickSwitcher::popup()
{
    if (m_proxy->rowCount() <= 1)
        return;

    const int rowHeight = sizeHintForRow(0);
    const int frame = 2 * frameWidth();
    const int visibleRows = qMin(qMax(m_proxy->rowCount(), 6), 12);
    resize(360, rowHeight * visibleRows + frame);

    if (parentWidget()) {
        QWidget *central = parentWidget()->window();
        const QPoint globalCenter = central->mapToGlobal(central->rect().center());
        move(globalCenter - QPoint(width() / 2, height() / 2));
    }

    const QModelIndex first = m_proxy->index(0, 0);
    const QModelIndex second = m_proxy->index(qMin(1, m_proxy->rowCount() - 1), 0);
    selectionModel()->select(second, QItemSelectionModel::Rows | QItemSelectionModel::ClearAndSelect);
    selectionModel()->setCurrentIndex(second, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    Q_UNUSED(first);

    show();
    setFocus();
}

void QuickSwitcher::step(int offset)
{
    if (!isVisible()) {
        popup();
        return;
    }

    int row = currentIndex().row() + offset;
    if (row < 0)
        row = m_proxy->rowCount() - 1;
    else if (row >= m_proxy->rowCount())
        row = 0;

    const QModelIndex idx = m_proxy->index(row, 0);
    selectionModel()->select(idx, QItemSelectionModel::Rows | QItemSelectionModel::ClearAndSelect);
    selectionModel()->setCurrentIndex(idx, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
}

void QuickSwitcher::activateCurrent()
{
    const QModelIndex idx = currentIndex();
    if (idx.isValid()) {
        const int docId = idx.data(DocumentManager::DocIdRole).toInt();
        if (docId >= 0)
            emit documentActivated(docId);
    }
    hide();
}

void QuickSwitcher::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        activateCurrent();
        event->accept();
        return;
    }
    QTreeView::keyPressEvent(event);
}

void QuickSwitcher::keyReleaseEvent(QKeyEvent *event)
{
#ifdef Q_OS_MACOS
    if (event->key() == Qt::Key_Meta) {
#else
    if (event->key() == Qt::Key_Control) {
#endif
        activateCurrent();
        event->accept();
        return;
    }
    QTreeView::keyReleaseEvent(event);
}

void QuickSwitcher::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex idx = indexAt(event->pos());
    if (idx.isValid()) {
        setCurrentIndex(idx);
        activateCurrent();
        event->accept();
        return;
    }
    QTreeView::mousePressEvent(event);
}
