#include "recenttagproxymodel.h"
#include "documentmanager.h"

RecentTagProxyModel::RecentTagProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    sort(0, Qt::AscendingOrder);
}

bool RecentTagProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const quint64 leftTag = left.data(DocumentManager::RecentTagRole).toULongLong();
    const quint64 rightTag = right.data(DocumentManager::RecentTagRole).toULongLong();
    return leftTag > rightTag;
}
