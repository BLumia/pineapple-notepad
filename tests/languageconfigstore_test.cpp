#include <QtTest>

#include "languageconfigstore.h"
#include "kshlexer.h"

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>

#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;

class LanguageConfigStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void builtinCatalogIsLoaded();
    void decodeMenuEntry_data();
    void decodeMenuEntry();
    void defaultMenuEntriesContents();
    void userLanguageMergeAndGeneration();
    void userEntryOverridesBuiltin();
    void userLanguageAssociations();

private:
    QTemporaryDir m_dir;
};

void LanguageConfigStoreTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());

    // Start from a clean state: no user languages.
    QFile::remove(LanguageConfigStore::userLanguagesFilePath());
    LanguageConfigStore::reloadUserLanguages();
}

void LanguageConfigStoreTest::cleanupTestCase()
{
    QFile::remove(LanguageConfigStore::userLanguagesFilePath());
    LanguageConfigStore::reloadUserLanguages();
}

void LanguageConfigStoreTest::cleanup()
{
    QFile::remove(LanguageConfigStore::userLanguagesFilePath());
    LanguageConfigStore::reloadUserLanguages();
}

void LanguageConfigStoreTest::builtinCatalogIsLoaded()
{
    const auto &config = LanguageConfigStore::parsedLanguageConfig();
    QVERIFY(config.valid);
    QVERIFY(config.infos.contains(u"cpp"_s));
    QCOMPARE(config.infos.value(u"cpp"_s).displayName, u"C++"_s);
    QCOMPARE(config.infos.value(u"cpp"_s).effectiveLexer, u"cpp"_s);
    QVERIFY(!config.infos.value(u"cpp"_s).userDefined);

    // KSH-engine languages no longer need catalog entries; their extension
    // detection and comment spec fall back to the KSH repository.
    QVERIFY(!config.infos.contains(u"yaml"_s));
    QVERIFY(kshutils::repository().definitionForName(u"YAML"_s).isValid());
    QCOMPARE(LanguageConfigStore::commentSpec(u"YAML"_s).singleLineComment, u"#"_s);

    // No user languages yet.
    QVERIFY(LanguageConfigStore::userIds().isEmpty());
}

void LanguageConfigStoreTest::decodeMenuEntry_data()
{
    QTest::addColumn<QString>("encoded");
    QTest::addColumn<bool>("expectedValid");
    QTest::addColumn<QString>("expectedId");
    QTest::addColumn<bool>("expectedKsh");

    QTest::newRow("lexilla-cpp") << u"lexilla:cpp"_s << true << u"cpp"_s << false;
    QTest::newRow("lexilla-json") << u"lexilla:json"_s << true << u"json"_s << false;
    // KSH engine catalog entries are canonically encoded through their KSH
    // definition name, which is also a valid ksh: entry.
    QTest::newRow("ksh-definition") << u"ksh:Markdown"_s << true << u"Markdown"_s << true;
    // Dangling references must be rejected (e.g. deleted UDL), never resolved
    // through the bare lexer fallback.
    QTest::newRow("lexilla-dangling") << u"lexilla:nosuchlanguage"_s << false << QString() << false;
    QTest::newRow("ksh-dangling") << u"ksh:Nosuch Definition"_s << false << QString() << false;
    QTest::newRow("garbage") << u"what"_s << false << QString() << false;
    QTest::newRow("empty-id") << u"lexilla:"_s << false << QString() << false;
}

void LanguageConfigStoreTest::decodeMenuEntry()
{
    QFETCH(QString, encoded);
    QFETCH(bool, expectedValid);
    QFETCH(QString, expectedId);
    QFETCH(bool, expectedKsh);

    const auto entry = LanguageConfigStore::decodeMenuEntry(encoded);
    QCOMPARE(entry.valid, expectedValid);
    if (expectedValid) {
        QCOMPARE(entry.languageId, expectedId);
        QCOMPARE(entry.isKsh, expectedKsh);
        QVERIFY(!entry.displayName.isEmpty());
        QCOMPARE(entry.encoded, encoded);
    }
}

void LanguageConfigStoreTest::defaultMenuEntriesContents()
{
    const auto entries = LanguageConfigStore::defaultMenuEntries();
    QVERIFY(!entries.isEmpty());

    bool foundCpp = false;
    bool foundKshEngineEntry = false;
    for (const auto &entry : entries) {
        QVERIFY(entry.valid);
        if (entry.languageId == u"cpp"_s) {
            foundCpp = true;
            QVERIFY(!entry.isKsh);
        }
        // Built-in KSH-engine entries (e.g. markdown) are encoded as ksh: entries.
        if (entry.isKsh && entry.languageId == u"Markdown"_s) {
            foundKshEngineEntry = true;
        }
    }
    QVERIFY(foundCpp);
    QVERIFY(foundKshEngineEntry);
}

void LanguageConfigStoreTest::userLanguageMergeAndGeneration()
{
    // Initially absent.
    QVERIFY(!LanguageConfigStore::isUserDefined(u"testudl"_s));

    QJsonObject udl;
    udl.insert(u"displayName"_s, u"Test UDL"_s);
    udl.insert(u"lexer"_s, u"cpp"_s);
    udl.insert(u"singleLineComment"_s, u"//"_s);
    {
        QJsonArray exts;
        exts.append(u"testudl"_s);
        udl.insert(u"extensions"_s, exts);
    }
    {
        QJsonObject keywords;
        keywords.insert(u"0"_s, u"foo bar"_s);
        udl.insert(u"keywords"_s, keywords);
    }

    QJsonObject languages;
    languages.insert(u"testudl"_s, udl);
    QVERIFY(LanguageConfigStore::saveUserLanguagesObject(languages));
    LanguageConfigStore::reloadUserLanguages();

    // Merged catalog contains the user entry with master metadata.
    const auto &config = LanguageConfigStore::parsedLanguageConfig();
    QVERIFY(config.infos.contains(u"testudl"_s));
    const LanguageInfo &info = config.infos.value(u"testudl"_s);
    QVERIFY(info.userDefined);
    QCOMPARE(info.displayName, u"Test UDL"_s);
    QCOMPARE(info.effectiveLexer, u"cpp"_s);
    QCOMPARE(info.extensions, QStringList{ u"testudl"_s });

    QVERIFY(LanguageConfigStore::isUserDefined(u"testudl"_s));
    QCOMPARE(LanguageConfigStore::displayName(u"testudl"_s), u"Test UDL"_s);

    // Keywords and styles resolve through the master lexer.
    const LanguageSpec *spec = LanguageConfigStore::languageSpec(u"testudl"_s);
    QVERIFY(spec);
    QCOMPARE(spec->keywords.value(0), QByteArrayLiteral("foo bar"));
    QVERIFY(!spec->styles.isEmpty()); // cpp base styles
    QCOMPARE(LanguageConfigStore::commentSpec(u"testudl"_s).singleLineComment, u"//"_s);

    // Encoded as a lexilla: entry and valid.
    const auto entry = LanguageConfigStore::decodeMenuEntry(u"lexilla:testudl"_s);
    QVERIFY(entry.valid);
    QVERIFY(!entry.isKsh);
    QVERIFY(entry.userDefined);

    // Default menu entries must only contain built-in languages.
    for (const auto &defaultEntry : LanguageConfigStore::defaultMenuEntries()) {
        QVERIFY(defaultEntry.languageId != u"testudl"_s);
    }
}

void LanguageConfigStoreTest::userEntryOverridesBuiltin()
{
    QVERIFY(LanguageConfigStore::isBuiltInLanguage(u"python"_s));
    QVERIFY(!LanguageConfigStore::isBuiltInLanguage(u"nosuchlang"_s));
    QVERIFY(!LanguageConfigStore::isOverridden(u"python"_s));

    QJsonObject udl;
    udl.insert(u"displayName"_s, u"Fake Python"_s);
    udl.insert(u"lexer"_s, u"cpp"_s);
    udl.insert(u"singleLineComment"_s, u"#"_s);
    {
        QJsonArray exts;
        exts.append(u"fpy"_s);
        udl.insert(u"extensions"_s, exts);
    }
    {
        QJsonObject keywords;
        keywords.insert(u"0"_s, u"spam eggs"_s);
        udl.insert(u"keywords"_s, keywords);
    }

    QJsonObject languages;
    languages.insert(u"python"_s, udl);
    QVERIFY(LanguageConfigStore::saveUserLanguagesObject(languages));
    LanguageConfigStore::reloadUserLanguages();

    // Whole-entry replacement: the user entry wins, flagged as an override.
    QVERIFY(LanguageConfigStore::isOverridden(u"python"_s));
    QVERIFY(LanguageConfigStore::isUserDefined(u"python"_s));
    QVERIFY(LanguageConfigStore::isBuiltInLanguage(u"python"_s));
    const auto &info = LanguageConfigStore::parsedLanguageConfig().infos.value(u"python"_s);
    QCOMPARE(info.displayName, u"Fake Python"_s);
    QCOMPARE(info.effectiveLexer, u"cpp"_s);
    QCOMPARE(info.extensions, QStringList{ u"fpy"_s });
    const LanguageSpec *spec = LanguageConfigStore::languageSpec(u"python"_s);
    QVERIFY(spec);
    QCOMPARE(spec->keywords.value(0), QByteArrayLiteral("spam eggs"));

    // The override's own extension is detected; a plain UDL is not "overridden".
    QCOMPARE(LanguageConfigStore::associations().extensionToLanguage.value(u"fpy"_s), u"python"_s);
    QVERIFY(!LanguageConfigStore::isOverridden(u"mylang"_s));
}

void LanguageConfigStoreTest::userLanguageAssociations()
{
    QJsonObject udl;
    udl.insert(u"lexer"_s, u"cpp"_s);
    {
        QJsonArray exts;
        exts.append(u"myext"_s);
        udl.insert(u"extensions"_s, exts);
    }

    QJsonObject languages;
    languages.insert(u"mylang"_s, udl);
    QVERIFY(LanguageConfigStore::saveUserLanguagesObject(languages));
    LanguageConfigStore::reloadUserLanguages();

    const auto &assoc = LanguageConfigStore::associations();
    QCOMPARE(assoc.extensionToLanguage.value(u"myext"_s), u"mylang"_s);
    QCOMPARE(assoc.languageToLexer.value(u"mylang"_s), u"cpp"_s);

    // A user extension never overrides a built-in association.
    QJsonObject shadow;
    shadow.insert(u"lexer"_s, u"cpp"_s);
    {
        QJsonArray exts;
        exts.append(u"cpp"_s); // owned by the built-in "cpp" language
        shadow.insert(u"extensions"_s, exts);
    }
    languages.insert(u"shadow"_s, shadow);
    QVERIFY(LanguageConfigStore::saveUserLanguagesObject(languages));
    LanguageConfigStore::reloadUserLanguages();
    QCOMPARE(LanguageConfigStore::associations().extensionToLanguage.value(u"cpp"_s), u"cpp"_s);
}

QTEST_MAIN(LanguageConfigStoreTest)
#include "languageconfigstore_test.moc"
