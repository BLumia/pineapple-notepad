#pragma once

#include <KConfigGroup>

#include <QObject>
#include <QStringList>

class FavoritesStore final : public QObject
{
    Q_OBJECT

public:
    explicit FavoritesStore(QObject *parent = nullptr);
    explicit FavoritesStore(const KConfigGroup &group, QObject *parent = nullptr);

    QStringList entries() const;
    bool isFavorite(const QString &filePath) const;
    void add(const QString &filePath);
    void remove(const QString &filePath);
    void removeEntries(const QStringList &filePaths);
    bool toggle(const QString &filePath);

Q_SIGNALS:
    void entriesChanged();

private:
    void load();
    void save();

    QStringList m_entries; // most recently added first
    KConfigGroup m_group;
};
