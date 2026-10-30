#pragma once

#include <KSyntaxHighlighting/Theme>

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

struct StyleSpec {
    KSyntaxHighlighting::Theme::TextStyle textStyle = KSyntaxHighlighting::Theme::Normal;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool eolFilled = false;
    // Reserved for declaring per-style spell-check for non-KSH lexers. The KSH lexer
    // determines spell-check dynamically and ignores this field.
    bool spellCheck = false;
};

struct MultiLineComment {
    QString start;
    QString end;
};

struct CommentSpec {
    QString singleLineComment;
    MultiLineComment multiLineComment;
};

struct LanguageSpec {
    QHash<int, StyleSpec> styles;
    QHash<int, QByteArray> keywords;
    QHash<QByteArray, QByteArray> properties;
    QString singleLineComment;
    MultiLineComment multiLineComment;
};

// Catalog metadata for a language entry (from language-config.json or user-languages.json).
struct LanguageInfo {
    QString id;
    QString displayName; // may be empty; falls back to id
    QString effectiveLexer; // resolved lexer name ("cpp", "ksyntaxhighlighting", ...)
    QStringList extensions;
    QStringList filenames;
    bool userDefined = false;
};

struct LanguageConfig {
    QHash<QString, LanguageSpec> languages;
    QHash<QString, LanguageInfo> infos;
    bool valid = false;
};

struct LanguageAssociations {
    QHash<QString, QString> extensionToLanguage;
    QHash<QString, QString> filenameToLanguage;
    QHash<QString, QString> languageToLexer;
    bool valid = false;
};

// A resolved "Language" menu entry. Encoded form: "lexilla:<catalogId>" / "ksh:<definitionName>".
struct LanguageMenuEntry {
    QString encoded;
    QString languageId; // decoded id: catalog id or KSH definition name
    QString displayName;
    bool isKsh = false;
    bool userDefined = false; // catalog entry authored by the user (UDL)
    bool valid = false;
};

namespace LanguageConfigStore
{
// Built-in language-config.json (qrc, immutable).
const QJsonDocument &document();
const QJsonObject &rootObject();
const QJsonObject &languagesObject();

// Merged catalog (built-in json + user-languages.json); rebuilt when the
// generation changes (see reloadUserLanguages()).
const LanguageConfig &parsedLanguageConfig();
const LanguageSpec *languageSpec(const QString &languageId);
LanguageSpec baseLanguageSpec(const QString &lexerName);
CommentSpec commentSpec(const QString &languageId);

// Extension/filename -> language associations from the merged catalog.
const LanguageAssociations &associations();

// User defined languages (UDL), stored in user-languages.json.
QString userLanguagesFilePath();
QStringList userIds();
bool isUserDefined(const QString &languageId);
// True if the id exists in the built-in language-config.json (regardless of
// whether it is currently overridden by a user entry).
bool isBuiltInLanguage(const QString &languageId);
// True if the id is a built-in language currently replaced by a user entry.
bool isOverridden(const QString &languageId);
QJsonObject loadUserLanguagesObject();
bool saveUserLanguagesObject(const QJsonObject &languages);
// Re-reads user-languages.json and invalidates all derived caches.
void reloadUserLanguages();

// Display name resolution: catalog displayName -> KSH translatedName -> id.
QString displayName(const QString &languageId);

// "Language" menu entry encoding helpers.
QString encodeMenuEntry(bool ksh, const QString &id);
// Decodes and validates: "lexilla:" entries must exist in the merged catalog,
// "ksh:" entries must resolve to a valid KSH definition. Dangling entries are
// returned with valid == false.
LanguageMenuEntry decodeMenuEntry(const QString &encoded);
// The default "Language" menu entry list, defined by the "defaultLanguageMenu"
// array in language-config.json. Absent field -> empty list.
QList<LanguageMenuEntry> defaultMenuEntries();
}
