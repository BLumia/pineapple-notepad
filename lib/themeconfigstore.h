#pragma once

#include <KSyntaxHighlighting/Theme>

#include <QColor>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>

namespace ThemeConfigStore
{

struct ColorRoleEntry {
    QString role;
    QString displayName;
};

struct ColorRoleCategory {
    QString name;
    QList<ColorRoleEntry> entries;
};

QColor colorForRole(const QString &themeId, const QString &role);
QColor colorForTextStyle(const QString &themeId, KSyntaxHighlighting::Theme::TextStyle ts);
QList<QPair<QString, QString>> availableThemes();
QString themeVariant(const QString &themeId, bool dark);

QList<ColorRoleCategory> colorRoleCatalog();

QHash<QString, QColor> userColors(const QString &themeId);
void saveUserColors(const QString &themeId, const QHash<QString, QColor> &colors);
void resetUserColors(const QString &themeId);
void reloadExtensionConfig();

}
