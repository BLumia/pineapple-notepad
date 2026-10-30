#include "themeconfigstore.h"
#include "kshlexer.h"
#include "apppaths.h"

#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Theme>

#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QSaveFile>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

Q_LOGGING_CATEGORY(lcThemeCfgStore, "pnotepad.themeconfigstore")

struct ThemeExtension {
    QString darkVariant;
    QString lightVariant;
    QHash<QString, QColor> extensionColors;
};

struct ExtensionConfig {
    QHash<QString, ThemeExtension> themes;
    bool loaded = false;
};

static QHash<QString, QColor> parseExtensionColors(const QJsonObject &obj)
{
    QHash<QString, QColor> colors;
    colors.reserve(obj.size());
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!it.value().isString())
            continue;
        const QColor c(it.value().toString());
        if (c.isValid())
            colors.insert(it.key(), c);
    }
    return colors;
}

static QString userThemeExtensionsPath()
{
    return AppPaths::appDataPath().absoluteFilePath(QStringLiteral("theme-extensions.json"));
}

// Parse themes from a JSON document into the given config.
// Does NOT clear existing entries — caller controls merge semantics.
static void parseThemes(const QJsonDocument &doc, ExtensionConfig &config)
{
    if (!doc.isObject())
        return;
    const QJsonObject root = doc.object();
    const QJsonObject themesRoot = root.value(QStringLiteral("themes")).toObject();
    for (auto it = themesRoot.begin(); it != themesRoot.end(); ++it) {
        if (!it.value().isObject())
            continue;

        const QJsonObject themeObj = it.value().toObject();
        ThemeExtension &ext = config.themes[it.key()]; // create or reference

        const QJsonValue darkVar = themeObj.value(QStringLiteral("dark-variant"));
        if (darkVar.isString())
            ext.darkVariant = darkVar.toString();

        const QJsonValue lightVar = themeObj.value(QStringLiteral("light-variant"));
        if (lightVar.isString())
            ext.lightVariant = lightVar.toString();

        const QJsonValue extColors = themeObj.value(QStringLiteral("extensions"));
        if (extColors.isObject()) {
            const auto parsed = parseExtensionColors(extColors.toObject());
            for (auto cit = parsed.begin(); cit != parsed.end(); ++cit)
                ext.extensionColors[cit.key()] = cit.value(); // overwrite per-role
        }
    }
}

static ExtensionConfig loadExtensionConfig()
{
    ExtensionConfig config;

    // 1. Built-in resource
    {
        QFile f(QStringLiteral(":/theme-extensions.json"));
        if (f.open(QIODevice::ReadOnly)) {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
            if (parseError.error == QJsonParseError::NoError)
                parseThemes(doc, config);
        }
    }

    // 2. User file (overrides built-in per-theme per-role)
    {
        QFile f(userThemeExtensionsPath());
        if (f.open(QIODevice::ReadOnly)) {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
            if (parseError.error == QJsonParseError::NoError)
                parseThemes(doc, config);
        }
    }

    config.loaded = true;
    return config;
}

namespace ThemeConfigStore
{

static ExtensionConfig &mutableExtensionConfig()
{
    static ExtensionConfig config = loadExtensionConfig();
    return config;
}

static const ExtensionConfig &extensionConfig()
{
    return mutableExtensionConfig();
}

static QRgb blendOver(QRgb src, QRgb dst)
{
    const int a = qAlpha(src);
    if (a == 0 || a == 255)
        return src;
    const int r = (qRed(src) * a + qRed(dst) * (255 - a)) / 255;
    const int g = (qGreen(src) * a + qGreen(dst) * (255 - a)) / 255;
    const int b = (qBlue(src) * a + qBlue(dst) * (255 - a)) / 255;
    return qRgb(r, g, b);
}

// Resolves a KSH text style color against an already-resolved theme, applying the
// Normal-style fallback and alpha-blending over the theme background.
static QColor textColorForStyle(const KSyntaxHighlighting::Theme &theme,
                                KSyntaxHighlighting::Theme::TextStyle ts)
{
    const QRgb bgRgb = theme.editorColor(KSyntaxHighlighting::Theme::BackgroundColor);
    QRgb rgb = theme.textColor(ts);
    if (rgb == 0)
        rgb = theme.textColor(KSyntaxHighlighting::Theme::Normal);
    if (qAlpha(rgb) > 0 && qAlpha(rgb) < 255)
        rgb = blendOver(rgb, bgRgb);
    return QColor::fromRgb(rgb);
}

QColor colorForRole(const QString &themeId, const QString &role)
{
    // 1. Check extension colors (non-KSH keys like editor.CodeFolding.bg)
    const auto &extConfig = extensionConfig();
    auto itTheme = extConfig.themes.constFind(themeId);
    if (itTheme != extConfig.themes.constEnd()) {
        auto itColor = itTheme->extensionColors.constFind(role);
        if (itColor != itTheme->extensionColors.constEnd())
            return itColor.value();
    }

    // 2. Resolve KSH theme (fallback to default if not found)
    KSyntaxHighlighting::Theme theme = kshutils::repository().theme(themeId);
    if (!theme.isValid()) {
        theme = kshutils::repository().defaultTheme(KSyntaxHighlighting::Repository::LightTheme);
        if (!theme.isValid()) {
            qCWarning(lcThemeCfgStore).noquote() << "No valid theme for role:" << role;
            return QColor(Qt::red);
        }
    }

    // Background color for alpha blending (KSH themes may use #AARRGGBB)
    const QRgb bgRgb = theme.editorColor(KSyntaxHighlighting::Theme::BackgroundColor);

    auto resolve = [&](QRgb rgb) -> QColor {
        if (qAlpha(rgb) > 0 && qAlpha(rgb) < 255)
            rgb = blendOver(rgb, bgRgb);
        return QColor::fromRgb(rgb);
    };

    // 3. Editor color roles (editor.XXX)
    if (role.startsWith(QStringLiteral("editor."))) {
        const QString roleName = role.mid(7);
        static const auto metaEnum =
            QMetaEnum::fromType<KSyntaxHighlighting::Theme::EditorColorRole>();
        for (int i = 0; i < metaEnum.keyCount(); ++i) {
            if (roleName == QLatin1String(metaEnum.key(i))) {
                QRgb rgb = theme.editorColor(
                    static_cast<KSyntaxHighlighting::Theme::EditorColorRole>(metaEnum.value(i)));
                if (rgb == 0)
                    rgb = bgRgb;
                return resolve(rgb);
            }
        }
    }

    // 4. Text style names
    static const auto textStyleEnum =
        QMetaEnum::fromType<KSyntaxHighlighting::Theme::TextStyle>();
    for (int i = 0; i < textStyleEnum.keyCount(); ++i) {
        if (role == QLatin1String(textStyleEnum.key(i))) {
            return textColorForStyle(theme,
                static_cast<KSyntaxHighlighting::Theme::TextStyle>(textStyleEnum.value(i)));
        }
    }

    // 5. Known non-KSH fallbacks for themes without extension colors
    if (role == QStringLiteral("editor.CodeFolding.bg"))
        return resolve(theme.editorColor(KSyntaxHighlighting::Theme::LineNumbers));
    if (role == QStringLiteral("editor.CodeFolding.highlight"))
        return resolve(theme.editorColor(KSyntaxHighlighting::Theme::CurrentLineNumber));
    if (role == QStringLiteral("editor.BracketMatching.fg"))
        return resolve(theme.textColor(KSyntaxHighlighting::Theme::Normal));
    if (role == QStringLiteral("editor.tagMatchHighlight"))
        return resolve(theme.editorColor(KSyntaxHighlighting::Theme::TextSelection));

    qCWarning(lcThemeCfgStore).noquote() << "Unknown role:" << role << "for theme:" << themeId;
    return QColor(Qt::red);
}

QColor colorForTextStyle(const QString &themeId, KSyntaxHighlighting::Theme::TextStyle ts)
{
    // 1. Honor user/theme extension overrides keyed by the text style name, mirroring
    //    colorForRole()'s step 1. The theme editor saves per-text-style customizations
    //    (e.g. a user-customized "Keyword" color) into extensionColors under that name.
    {
        static const auto metaEnum =
            QMetaEnum::fromType<KSyntaxHighlighting::Theme::TextStyle>();
        if (const char *name = metaEnum.valueToKey(static_cast<int>(ts))) {
            const auto &extConfig = extensionConfig();
            auto itTheme = extConfig.themes.constFind(themeId);
            if (itTheme != extConfig.themes.constEnd()) {
                auto itColor = itTheme->extensionColors.constFind(QLatin1String(name));
                if (itColor != itTheme->extensionColors.constEnd())
                    return itColor.value();
            }
        }
    }

    // 2. Resolve KSH theme and apply textColor (with Normal fallback + alpha blend).
    KSyntaxHighlighting::Theme theme = kshutils::repository().theme(themeId);
    if (!theme.isValid()) {
        theme = kshutils::repository().defaultTheme(KSyntaxHighlighting::Repository::LightTheme);
        if (!theme.isValid()) {
            qCWarning(lcThemeCfgStore).noquote() << "No valid theme for text style:" << int(ts);
            return QColor(Qt::red);
        }
    }
    return textColorForStyle(theme, ts);
}

QList<QPair<QString, QString>> availableThemes()
{
    const auto themes = kshutils::repository().themes();
    QList<QPair<QString, QString>> out;
    out.reserve(themes.size());
    for (const auto &theme : themes) {
        const QString name = theme.name();
        QString display = theme.translatedName();
        if (display.isEmpty())
            display = name;
        out.push_back(qMakePair(name, display));
    }

    std::stable_sort(out.begin(), out.end(),
                     [](const QPair<QString, QString> &a, const QPair<QString, QString> &b) {
                         const bool aOurs = a.first.startsWith(QStringLiteral("Pineapple Notepad"));
                         const bool bOurs = b.first.startsWith(QStringLiteral("Pineapple Notepad"));
                         if (aOurs != bOurs)
                             return aOurs;
                         return a.second.localeAwareCompare(b.second) < 0;
                     });

    return out;
}

QString themeVariant(const QString &themeId, bool dark)
{
    const auto &config = extensionConfig();
    const auto it = config.themes.constFind(themeId);
    if (it == config.themes.constEnd())
        return QString();
    return dark ? it->darkVariant : it->lightVariant;
}

// --- Color role catalog ---

QList<ColorRoleCategory> colorRoleCatalog()
{
    QList<ColorRoleCategory> catalog;

    // Category 1: Text Styles (all TextStyle enum values)
    {
        ColorRoleCategory cat;
        cat.name = QObject::tr("Text Styles");
        const QList<ColorRoleEntry> textStyleEntries = {
            {u"Normal"_s,           QObject::tr("Normal")},
            {u"Keyword"_s,          QObject::tr("Keyword")},
            {u"Function"_s,         QObject::tr("Function")},
            {u"Variable"_s,         QObject::tr("Variable")},
            {u"ControlFlow"_s,      QObject::tr("Control Flow")},
            {u"Operator"_s,         QObject::tr("Operator")},
            {u"BuiltIn"_s,          QObject::tr("Built-in")},
            {u"Extension"_s,        QObject::tr("Extension")},
            {u"Preprocessor"_s,     QObject::tr("Preprocessor")},
            {u"Attribute"_s,        QObject::tr("Attribute")},
            {u"Char"_s,             QObject::tr("Character")},
            {u"SpecialChar"_s,      QObject::tr("Special Character")},
            {u"String"_s,           QObject::tr("String")},
            {u"VerbatimString"_s,   QObject::tr("Verbatim String")},
            {u"SpecialString"_s,    QObject::tr("Special String")},
            {u"Import"_s,           QObject::tr("Imports, Modules, Includes")},
            {u"DataType"_s,         QObject::tr("Data Type")},
            {u"DecVal"_s,           QObject::tr("Decimal Value")},
            {u"BaseN"_s,            QObject::tr("Base N Number")},
            {u"Float"_s,            QObject::tr("Float")},
            {u"Constant"_s,         QObject::tr("Constant")},
            {u"Comment"_s,          QObject::tr("Comment")},
            {u"Documentation"_s,    QObject::tr("Documentation")},
            {u"Annotation"_s,       QObject::tr("Annotation")},
            {u"CommentVar"_s,       QObject::tr("Comment Variable")},
            {u"RegionMarker"_s,     QObject::tr("Region Marker")},
            {u"Information"_s,      QObject::tr("Information")},
            {u"Warning"_s,          QObject::tr("Warning")},
            {u"Alert"_s,            QObject::tr("Alert")},
            {u"Error"_s,            QObject::tr("Error")},
            {u"Others"_s,           QObject::tr("Others")},
        };
        cat.entries.append(textStyleEntries);
        catalog.push_back(std::move(cat));
    }

    // Category 2: Editor colors (roles actually used in applyTheme)
    {
        ColorRoleCategory cat;
        cat.name = QObject::tr("Editor");
        const QList<ColorRoleEntry> usedEditoEntries = {
            {u"editor.BackgroundColor"_s, QObject::tr("Editor Background")},
            {u"editor.TextSelection"_s, QObject::tr("Text Selection")},
            {u"editor.CurrentLine"_s, QObject::tr("Current Line")},
            {u"editor.SearchHighlight"_s, QObject::tr("Search Highlight")},
            {u"editor.tagMatchHighlight"_s, QObject::tr("Tag Match Highlight")},
            {u"editor.BracketMatching"_s, QObject::tr("Bracket Matching")},
            {u"editor.BracketMatching.fg"_s, QObject::tr("Bracket Matching Foreground")},
            {u"editor.TabMarker"_s, QObject::tr("Whitespace Indicator")},
            {u"editor.IndentationLine"_s, QObject::tr("Indentation Line")},
            {u"editor.LineNumbers"_s, QObject::tr("Line Numbers")},
            {u"editor.IconBorder"_s, QObject::tr("Line Number Background")},
            {u"editor.CodeFolding"_s, QObject::tr("Code Folding")},
            {u"editor.CodeFolding.bg"_s, QObject::tr("Code Folding Background")},
            {u"editor.CodeFolding.highlight"_s, QObject::tr("Code Folding Highlight")},
            {u"editor.CurrentLineNumber"_s, QObject::tr("Current Line Number")},
            {u"editor.ModifiedLines"_s, QObject::tr("Modified Lines")},
            {u"editor.MarkBookmark"_s, QObject::tr("Bookmark")},
            {u"editor.MarkWarning"_s, QObject::tr("Warning")},
            {u"editor.MarkError"_s, QObject::tr("Error")},
        };
        cat.entries.append(usedEditoEntries);
        catalog.push_back(std::move(cat));
    }

    return catalog;
}

// --- User override persistence ---

QHash<QString, QColor> userColors(const QString &themeId)
{
    QFile f(userThemeExtensionsPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return {};

    const QJsonObject themesRoot = doc.object().value(QStringLiteral("themes")).toObject();
    const QJsonObject themeObj = themesRoot.value(themeId).toObject();
    const QJsonObject extColors = themeObj.value(QStringLiteral("extensions")).toObject();

    return parseExtensionColors(extColors);
}

void saveUserColors(const QString &themeId, const QHash<QString, QColor> &colors)
{
    // Load existing user file (close before writing!)
    QJsonObject rootObj;
    {
        QFile readFile(userThemeExtensionsPath());
        if (readFile.open(QIODevice::ReadOnly)) {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(readFile.readAll(), &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject())
                rootObj = doc.object();
        }
    }

    QJsonObject themesRoot = rootObj.value(QStringLiteral("themes")).toObject();
    QJsonObject themeObj = themesRoot.value(themeId).toObject();
    QJsonObject extObj = themeObj.value(QStringLiteral("extensions")).toObject();

    // Merge new colors (overwrite per-role)
    for (auto it = colors.begin(); it != colors.end(); ++it)
        extObj.insert(it.key(), it.value().name());

    themeObj.insert(QStringLiteral("extensions"), extObj);
    themesRoot.insert(themeId, themeObj);
    rootObj.insert(QStringLiteral("themes"), themesRoot);
    rootObj.insert(QStringLiteral("version"), 2);

    // Write atomically
    QSaveFile out(userThemeExtensionsPath());
    if (!out.open(QIODevice::WriteOnly)) {
        qCWarning(lcThemeCfgStore) << "Cannot write user theme-extensions.json to" << userThemeExtensionsPath();
        return;
    }
    const QJsonDocument doc(rootObj);
    out.write(doc.toJson(QJsonDocument::Indented));
    out.commit();

    reloadExtensionConfig();
}

void resetUserColors(const QString &themeId)
{
    // Read and close before modifying the file
    QJsonObject rootObj;
    {
        QFile readFile(userThemeExtensionsPath());
        if (!readFile.open(QIODevice::ReadOnly))
            return;
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(readFile.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            return;
        rootObj = doc.object();
    }

    QJsonObject themesRoot = rootObj.value(QStringLiteral("themes")).toObject();
    themesRoot.remove(themeId);

    if (themesRoot.isEmpty()) {
        // No themes left — delete the file
        QFile::remove(userThemeExtensionsPath());
    } else {
        rootObj.insert(QStringLiteral("themes"), themesRoot);
        QSaveFile out(userThemeExtensionsPath());
        if (out.open(QIODevice::WriteOnly)) {
            out.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
            out.commit();
        }
    }

    reloadExtensionConfig();
}

void reloadExtensionConfig()
{
    mutableExtensionConfig() = loadExtensionConfig();
}

}
