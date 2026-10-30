#pragma once

#include <QSortFilterProxyModel>

class RecentTagProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit RecentTagProxyModel(QObject *parent = nullptr);

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};
