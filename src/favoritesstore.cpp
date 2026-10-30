#include "favoritesstore.h"

#include <KSharedConfig>

#include <QDir>
#include <QFileInfo>

using namespace Qt::Literals::StringLiterals;

namespace {

// Same canonicalization rule as MainWindow::addRecentFile(): prefer the
// canonical path of an existing file, fall back to the cleaned absolute path.
QString normalizeFavoritePath(const QString &filePath)
{
    const QFileInfo fi(filePath);
    const QString abs = fi.isAbsolute() ? fi.absoluteFilePath() : QDir::current().absoluteFilePath(filePath);
    const QFileInfo afi(abs);
    const QString canonical = afi.canonicalFilePath();
    return canonical.isEmpty() ? afi.absoluteFilePath() : canonical;
}

} // namespace

FavoritesStore::FavoritesStore(QObject *parent)
    : FavoritesStore(KConfigGroup(KSharedConfig::openConfig(), u"Favorites"_s), parent)
{
}

FavoritesStore::FavoritesStore(const KConfigGroup &group, QObject *parent)
    : QObject(parent)
    , m_group(group)
{
    load();
}

QStringList FavoritesStore::entries() const
{
    return m_entries;
}

bool FavoritesStore::isFavorite(const QString &filePath) const
{
    return m_entries.contains(normalizeFavoritePath(filePath));
}

void FavoritesStore::add(const QString &filePath)
{
    const QString normalized = normalizeFavoritePath(filePath);
    if (normalized.isEmpty()) {
        return;
    }
    if (!m_entries.isEmpty() && m_entries.first() == normalized) {
        return;
    }
    m_entries.removeAll(normalized);
    m_entries.prepend(normalized);
    save();
    emit entriesChanged();
}

void FavoritesStore::remove(const QString &filePath)
{
    const QString normalized = normalizeFavoritePath(filePath);
    if (!m_entries.contains(normalized)) {
        return;
    }
    m_entries.removeAll(normalized);
    save();
    emit entriesChanged();
}

void FavoritesStore::removeEntries(const QStringList &filePaths)
{
    bool changed = false;
    for (const QString &filePath : filePaths) {
        if (m_entries.removeAll(normalizeFavoritePath(filePath)) > 0) {
            changed = true;
        }
    }
    if (changed) {
        save();
        emit entriesChanged();
    }
}

bool FavoritesStore::toggle(const QString &filePath)
{
    const QString normalized = normalizeFavoritePath(filePath);
    if (normalized.isEmpty()) {
        return false;
    }
    if (m_entries.contains(normalized)) {
        remove(normalized);
        return false;
    }
    add(normalized);
    return true;
}

void FavoritesStore::load()
{
    const int count = m_group.readEntry(u"count"_s, 0);
    m_entries.clear();
    for (int i = 0; i < count; ++i) {
        const QString path = m_group.readEntry(u"File%1"_s.arg(i), QString());
        if (!path.isEmpty() && !m_entries.contains(path)) {
            m_entries.append(path);
        }
    }
}

void FavoritesStore::save()
{
    const int prevCount = m_group.readEntry(u"count"_s, 0);
    m_group.writeEntry(u"count"_s, static_cast<int>(m_entries.size()));
    for (int i = 0; i < m_entries.size(); ++i) {
        m_group.writeEntry(u"File%1"_s.arg(i), m_entries.at(i));
    }
    for (int i = static_cast<int>(m_entries.size()); i < prevCount; ++i) {
        m_group.deleteEntry(u"File%1"_s.arg(i));
    }
    m_group.sync();
}
