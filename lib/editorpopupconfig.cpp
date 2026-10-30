#include "editorpopupconfig.h"

#include <KConfigGroup>
#include <KSharedConfig>

using namespace Qt::StringLiterals;

namespace EditorPopupConfig {

namespace {
KConfigGroup group()
{
    return KConfigGroup(KSharedConfig::openConfig(), u"Editor Popup"_s);
}
} // namespace

QStringList items()
{
    return group().readEntry(u"Items"_s, QStringList{});
}

void setItems(const QStringList &items)
{
    KConfigGroup g = group();
    g.writeEntry(u"Items"_s, items);
    g.writeEntry(u"SchemaVersion"_s, kSchemaVersion);
    g.sync();
}

bool isCustomized()
{
    const KConfigGroup g = group();
    return g.hasKey(u"Items"_s) && g.readEntry(u"SchemaVersion"_s, 0) == kSchemaVersion;
}

void clearCustomization()
{
    KConfigGroup g = group();
    g.deleteEntry(u"Items"_s);
    g.deleteEntry(u"SchemaVersion"_s);
    g.sync();
}

} // namespace EditorPopupConfig
