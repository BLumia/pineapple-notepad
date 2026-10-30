#include "languageconfigstore.h"
#include "apppaths.h"
#include "basestyles.h"
#include "kshlexer.h"

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>

#include <QFile>
#include <QJsonArray>
#include <QJsonParseError>
#include <QMetaEnum>
#include <QSet>

using namespace Qt::StringLiterals;

namespace {

// Bumped by reloadUserLanguages(); all generation-aware caches rebuild lazily.
int s_generation = 0;

KSyntaxHighlighting::Theme::TextStyle textStyleFromString(const QString &name)
{
    static const auto metaEnum =
        QMetaEnum::fromType<KSyntaxHighlighting::Theme::TextStyle>();
    bool ok = false;
    const int value = metaEnum.keyToValue(name.toUtf8().constData(), &ok);
    if (ok)
        return static_cast<KSyntaxHighlighting::Theme::TextStyle>(value);
    return KSyntaxHighlighting::Theme::Normal;
}

StyleSpec parseStyleSpec(const QJsonObject &obj)
{
    StyleSpec spec;

    const auto tsIt = obj.find(u"textStyle"_s);
    if (tsIt != obj.end() && tsIt->isString())
        spec.textStyle = textStyleFromString(tsIt->toString());

    if (obj.value(u"bold"_s).isBool())
        spec.bold = obj.value(u"bold"_s).toBool();
    if (obj.value(u"italic"_s).isBool())
        spec.italic = obj.value(u"italic"_s).toBool();
    if (obj.value(u"underline"_s).isBool())
        spec.underline = obj.value(u"underline"_s).toBool();
    if (obj.value(u"eolFilled"_s).isBool())
        spec.eolFilled = obj.value(u"eolFilled"_s).toBool();
    if (obj.value(u"spellCheck"_s).isBool())
        spec.spellCheck = obj.value(u"spellCheck"_s).toBool();

    return spec;
}

static QHash<int, StyleSpec> parseStyleSpecs(const QJsonObject &stylesObj)
{
    QHash<int, StyleSpec> styles;
    for (auto it = stylesObj.begin(); it != stylesObj.end(); ++it) {
        bool ok = false;
        const int styleId = it.key().toInt(&ok);
        if (!ok || !it.value().isObject()) {
            continue;
        }
        styles.insert(styleId, parseStyleSpec(it.value().toObject()));
    }
    return styles;
}

void parseLanguageObject(const QString &languageId, const QJsonObject &langObj,
                         bool userDefined, LanguageConfig &config)
{
    LanguageSpec lang;
    LanguageInfo info;
    info.id = languageId;
    info.userDefined = userDefined;

    const QString effectiveLexer = langObj.value(u"lexer"_s).toString(languageId);
    info.effectiveLexer = effectiveLexer;

    const QString displayName = langObj.value(u"displayName"_s).toString().trimmed();
    if (!displayName.isEmpty()) {
        info.displayName = displayName;
    }

    const QJsonArray extensionsArr = langObj.value(u"extensions"_s).toArray();
    for (const QJsonValue &v : extensionsArr) {
        if (!v.isString()) {
            continue;
        }
        const QString ext = v.toString().trimmed();
        if (!ext.isEmpty()) {
            info.extensions.append(ext);
        }
    }

    const QJsonArray filenamesArr = langObj.value(u"filenames"_s).toArray();
    for (const QJsonValue &v : filenamesArr) {
        if (!v.isString()) {
            continue;
        }
        const QString fileName = v.toString().trimmed();
        if (!fileName.isEmpty()) {
            info.filenames.append(fileName);
        }
    }

    if (const QHash<int, StyleSpec> *base = BaseStyles::stylesForLexer(effectiveLexer))
        lang.styles = *base;
    const auto overrides = parseStyleSpecs(langObj.value(u"styles"_s).toObject());
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
        lang.styles.insert(it.key(), it.value());
    }

    const QJsonObject keywordsObj = langObj.value(u"keywords"_s).toObject();
    lang.keywords.reserve(keywordsObj.size());
    for (auto itKw = keywordsObj.begin(); itKw != keywordsObj.end(); ++itKw) {
        bool ok = false;
        const int keywordSet = itKw.key().toInt(&ok);
        if (!ok || !itKw.value().isString()) {
            continue;
        }
        lang.keywords.insert(keywordSet, itKw.value().toString().toUtf8());
    }

    const QJsonObject propertiesObj = langObj.value(u"properties"_s).toObject();
    lang.properties.reserve(propertiesObj.size());
    for (auto itProp = propertiesObj.begin(); itProp != propertiesObj.end(); ++itProp) {
        if (!itProp.value().isString()) {
            continue;
        }
        lang.properties.insert(itProp.key().toUtf8(), itProp.value().toString().toUtf8());
    }

    const QJsonValue singleLineCommentValue = langObj.value(u"singleLineComment"_s);
    if (singleLineCommentValue.isString()) {
        lang.singleLineComment = singleLineCommentValue.toString();
    }

    const QJsonValue multiLineCommentValue = langObj.value(u"multiLineComment"_s);
    if (multiLineCommentValue.isObject()) {
        const QJsonObject multiLineCommentObj = multiLineCommentValue.toObject();
        const QJsonValue startValue = multiLineCommentObj.value(u"start"_s);
        const QJsonValue endValue = multiLineCommentObj.value(u"end"_s);
        if (startValue.isString() && endValue.isString()) {
            lang.multiLineComment.start = startValue.toString();
            lang.multiLineComment.end = endValue.toString();
        }
    }

    config.languages.insert(languageId, lang);
    config.infos.insert(languageId, info);
}

LanguageConfig loadParsedLanguageConfig()
{
    LanguageConfig config;

    const QJsonObject languagesRoot = LanguageConfigStore::languagesObject();
    for (auto itLang = languagesRoot.begin(); itLang != languagesRoot.end(); ++itLang) {
        if (!itLang.value().isObject()) {
            continue;
        }
        parseLanguageObject(itLang.key(), itLang.value().toObject(), false, config);
    }

    // User languages: an entry whose id collides with a built-in one replaces
    // it entirely (override, flagged userDefined == true); other entries are
    // plain user defined languages (UDL).
    const QJsonObject userRoot = LanguageConfigStore::loadUserLanguagesObject();
    for (auto itLang = userRoot.begin(); itLang != userRoot.end(); ++itLang) {
        if (!itLang.value().isObject()) {
            continue;
        }
        parseLanguageObject(itLang.key(), itLang.value().toObject(), true, config);
    }

    config.valid = !config.languages.isEmpty();
    return config;
}

LanguageAssociations buildAssociations(const LanguageConfig &config)
{
    LanguageAssociations out;
    // Two passes so that user entries (UDL and overrides) never claim a
    // filename/extension mapping owned by another language, independent of
    // hash iteration order.
    for (int pass = 0; pass < 2; ++pass) {
        const bool userPass = (pass == 1);
        for (auto it = config.infos.constBegin(); it != config.infos.constEnd(); ++it) {
            const LanguageInfo &info = it.value();
            if (info.userDefined != userPass) {
                continue;
            }
            out.languageToLexer.insert(info.id, info.effectiveLexer);
            for (const QString &fileName : info.filenames) {
                const QString key = fileName.toLower();
                if (info.userDefined && out.filenameToLanguage.contains(key)) {
                    continue;
                }
                out.filenameToLanguage.insert(key, info.id);
            }
            for (const QString &ext : info.extensions) {
                const QString key = ext.toLower();
                if (info.userDefined && out.extensionToLanguage.contains(key)) {
                    continue;
                }
                out.extensionToLanguage.insert(key, info.id);
            }
        }
    }
    out.valid = !out.filenameToLanguage.isEmpty() || !out.extensionToLanguage.isEmpty()
                || !out.languageToLexer.isEmpty();
    return out;
}

} // namespace

namespace LanguageConfigStore
{

const QJsonDocument &document()
{
    static const QJsonDocument doc = []() -> QJsonDocument {
        QFile f(u":/language-config.json"_s);
        if (!f.open(QIODevice::ReadOnly)) {
            return QJsonDocument();
        }

        QJsonParseError parseError;
        const QJsonDocument parsed = QJsonDocument::fromJson(f.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
            return QJsonDocument();
        }

        return parsed;
    }();

    return doc;
}

const QJsonObject &rootObject()
{
    static const QJsonObject obj = document().object();
    return obj;
}

const QJsonObject &languagesObject()
{
    static const QJsonObject obj = rootObject().value(u"languages"_s).toObject();
    return obj;
}

const LanguageConfig &parsedLanguageConfig()
{
    static LanguageConfig config;
    static int cachedGeneration = -1;
    if (cachedGeneration != s_generation) {
        config = loadParsedLanguageConfig();
        cachedGeneration = s_generation;
    }
    return config;
}

const LanguageAssociations &associations()
{
    static LanguageAssociations assoc;
    static int cachedGeneration = -1;
    if (cachedGeneration != s_generation) {
        assoc = buildAssociations(parsedLanguageConfig());
        cachedGeneration = s_generation;
    }
    return assoc;
}

const LanguageSpec *languageSpec(const QString &languageId)
{
    const LanguageConfig &cfg = parsedLanguageConfig();
    if (!cfg.valid) {
        return nullptr;
    }
    const auto it = cfg.languages.constFind(languageId);
    if (it == cfg.languages.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

LanguageSpec baseLanguageSpec(const QString &lexerName)
{
    LanguageSpec spec;
    if (const QHash<int, StyleSpec> *base = BaseStyles::stylesForLexer(lexerName))
        spec.styles = *base;
    return spec;
}

CommentSpec commentSpec(const QString &languageId)
{
    CommentSpec info;
    const LanguageSpec *spec = languageSpec(languageId);
    if (spec) {
        info.singleLineComment = spec->singleLineComment;
        info.multiLineComment = spec->multiLineComment;
        return info;
    }

    const auto def = kshutils::repository().definitionForName(languageId);
    if (def.isValid()) {
        info.singleLineComment = def.singleLineCommentMarker();
        const auto ml = def.multiLineCommentMarker();
        info.multiLineComment.start = ml.first;
        info.multiLineComment.end = ml.second;
    }
    return info;
}

QString userLanguagesFilePath()
{
    return AppPaths::appDataPath().filePath(u"user-languages.json"_s);
}

QJsonObject loadUserLanguagesObject()
{
    QFile f(userLanguagesFilePath());
    if (!f.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }

    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(f.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        return QJsonObject();
    }
    return parsed.object().value(u"languages"_s).toObject();
}

bool saveUserLanguagesObject(const QJsonObject &languages)
{
    QJsonObject root;
    root.insert(u"version"_s, 1);
    root.insert(u"languages"_s, languages);

    QFile f(userLanguagesFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void reloadUserLanguages()
{
    ++s_generation;
}

QStringList userIds()
{
    QStringList ids;
    const LanguageConfig &cfg = parsedLanguageConfig();
    for (auto it = cfg.infos.constBegin(); it != cfg.infos.constEnd(); ++it) {
        if (it.value().userDefined) {
            ids.append(it.key());
        }
    }
    return ids;
}

bool isUserDefined(const QString &languageId)
{
    const LanguageConfig &cfg = parsedLanguageConfig();
    const auto it = cfg.infos.constFind(languageId);
    return it != cfg.infos.constEnd() && it.value().userDefined;
}

bool isBuiltInLanguage(const QString &languageId)
{
    return languagesObject().contains(languageId);
}

bool isOverridden(const QString &languageId)
{
    return isUserDefined(languageId) && isBuiltInLanguage(languageId);
}

QString displayName(const QString &languageId)
{
    const LanguageConfig &cfg = parsedLanguageConfig();
    const auto it = cfg.infos.constFind(languageId);
    if (it != cfg.infos.constEnd() && !it.value().displayName.isEmpty()) {
        return it.value().displayName;
    }

    const auto def = kshutils::repository().definitionForName(languageId);
    if (def.isValid()) {
        return def.translatedName();
    }
    return languageId;
}

QString encodeMenuEntry(bool ksh, const QString &id)
{
    return (ksh ? u"ksh:"_s : u"lexilla:"_s) + id;
}

LanguageMenuEntry decodeMenuEntry(const QString &encoded)
{
    LanguageMenuEntry entry;
    entry.encoded = encoded;

    const int colon = encoded.indexOf(u':');
    if (colon <= 0 || colon + 1 >= encoded.size()) {
        return entry;
    }
    const QString prefix = encoded.left(colon);
    const QString id = encoded.mid(colon + 1);
    if (id.isEmpty()) {
        return entry;
    }
    entry.languageId = id;

    if (prefix == u"lexilla"_s) {
        const LanguageConfig &cfg = parsedLanguageConfig();
        const auto it = cfg.infos.constFind(id);
        if (it == cfg.infos.constEnd()) {
            return entry; // dangling (e.g. deleted UDL); must not fall through to bare lexer
        }
        const LanguageInfo &info = it.value();
        entry.displayName = info.displayName.isEmpty() ? info.id : info.displayName;
        entry.userDefined = info.userDefined;
        entry.valid = true;
    } else if (prefix == u"ksh"_s) {
        const auto def = kshutils::repository().definitionForName(id);
        if (!def.isValid()) {
            return entry;
        }
        entry.displayName = def.translatedName();
        entry.isKsh = true;
        entry.valid = true;
    }
    return entry;
}

QList<LanguageMenuEntry> defaultMenuEntries()
{
    QList<LanguageMenuEntry> entries;

    // Source of truth: the explicit "defaultLanguageMenu" array in
    // language-config.json. Elements are either plain catalog ids ("cpp"),
    // "lexilla:<catalogId>", or "ksh:<definitionName>" (for languages that need
    // no catalog entry, e.g. KSH definitions referenced directly). If the field
    // is absent or yields no valid entries, the default menu is empty.
    const QJsonArray menuArr = rootObject().value(u"defaultLanguageMenu"_s).toArray();
    QSet<QString> seen;
    seen.reserve(menuArr.size());
    for (const QJsonValue &v : menuArr) {
        if (!v.isString()) {
            continue;
        }
        QString value = v.toString().trimmed();
        bool isKsh = false;
        if (value.startsWith(u"ksh:"_s)) {
            isKsh = true;
            value = value.mid(4);
        } else if (value.startsWith(u"lexilla:"_s)) {
            value = value.mid(8);
        }
        if (value.isEmpty()) {
            continue;
        }
        const QString encoded = encodeMenuEntry(isKsh, value);
        if (seen.contains(encoded)) {
            continue;
        }
        const auto entry = decodeMenuEntry(encoded);
        if (entry.valid) {
            seen.insert(encoded);
            entries.append(entry);
        }
    }
    return entries;
}

} // namespace LanguageConfigStore
