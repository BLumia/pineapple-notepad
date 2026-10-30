#include "mockdocument.h"
#include "kshlexer.h"

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>

#include <QtTest/QtTest>

#include <cstring>

namespace {

bool kshDefinitionAvailable(const char *name)
{
    return kshutils::repository().definitionForName(QString::fromUtf8(name)).isValid();
}

void lexWholeDoc(Scintilla::ILexer5 *lexer, pineapple::MockDocument &doc)
{
    lexer->Lex(0, doc.Length(), 0, &doc);
}

int baseStyleCount()
{
    std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> fresh(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    return fresh->NamedStyles();
}

struct StyleEntry {
    int styleId;
    int textStyleId;
    char bold;
    char italic;
    char underline;
    char strike;
    char spellCheck;
};

QList<StyleEntry> parseStylesProperty(const char *str)
{
    QList<StyleEntry> result;
    if (!str || !*str)
        return result;
    for (const QByteArray &entry : QByteArray::fromRawData(str, static_cast<int>(std::strlen(str))).split(';')) {
        if (entry.isEmpty())
            continue;
        const QList<QByteArray> fields = entry.split(':');
        if (fields.size() != 3)
            continue;
        bool ok = false;
        StyleEntry e{};
        e.styleId = fields[0].toInt(&ok);
        if (!ok || e.styleId < STYLE_LASTPREDEFINED + 1)
            continue;
        e.textStyleId = fields[1].toInt(&ok);
        if (!ok)
            continue;
        if (fields[2].size() != 5)
            continue;
        e.bold = fields[2].at(0);
        e.italic = fields[2].at(1);
        e.underline = fields[2].at(2);
        e.strike = fields[2].at(3);
        e.spellCheck = fields[2].at(4);
        result.append(e);
    }
    return result;
}

struct AllEntry {
    int styleId;
    int textStyleId;
    char spellCheck;
};

QList<AllEntry> parseAllEntries(const char *str)
{
    QList<AllEntry> result;
    if (!str || !*str)
        return result;
    for (const QByteArray &entry : QByteArray::fromRawData(str, static_cast<int>(std::strlen(str))).split(';')) {
        if (entry.isEmpty())
            continue;
        const QList<QByteArray> fields = entry.split(':');
        if (fields.size() != 3)
            continue;
        bool ok = false;
        AllEntry e{};
        e.styleId = fields[0].toInt(&ok);
        if (!ok)
            continue;
        e.textStyleId = fields[1].toInt(&ok);
        if (!ok)
            continue;
        if (fields[2].size() != 5)
            continue;
        e.spellCheck = fields[2].at(4);
        result.append(e);
    }
    return result;
}

} // namespace

class KshLexerFormatTest : public QObject {
    Q_OBJECT

private slots:
    void testMarkdownHasExtendedStyles();
    void testMarkdownBoldVsNormal();
    void testMarkdownItalicVsNormal();
    void testMarkdownBoldVsItalic();
    void testNamedStylesReflectsExtendedCount();
    void testPrimaryStyleFromStyle();
    void testNameOfExtendedStyle();
    void testStylesPropertyParseable();
    void testScssHasExtendedStyles();
    void testExtendedStylesToggle();
    void testSpellCheckInExtendedStyles();
    void testBaseStyleSpellCheckEntries();
    void testSpellCheckOnlyForTextStyles();
};

void KshLexerFormatTest::testMarkdownHasExtendedStyles()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    QCOMPARE(lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown"), 0);
    lexer->PropertySet("lexer.ksyntaxhighlighting.theme", "Breeze Light");

    const char *styles = lexer->PropertyGet("lexer.ksyntaxhighlighting.styles");
    const QList<StyleEntry> entries = parseStylesProperty(styles);
    QVERIFY(!entries.isEmpty());
    for (const auto &e : entries)
        QVERIFY(e.styleId >= STYLE_LASTPREDEFINED + 1);
}

void KshLexerFormatTest::testMarkdownBoldVsNormal()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");

    pineapple::MockDocument doc;
    doc.setText("**bold** normal");
    lexWholeDoc(lexer.get(), doc);

    auto styleAt = [&](int pos) -> char { return doc.styles.at(static_cast<size_t>(pos)); };

    const char boldStyle = styleAt(2);
    const char normalStyle = styleAt(10);
    QVERIFY2(boldStyle != normalStyle,
             "'**bold**' inner text should differ from 'normal' style");
    QVERIFY2(boldStyle >= STYLE_LASTPREDEFINED + 1,
             "'**bold**' inner text should use an extended styleId");
    QCOMPARE(normalStyle, 0);
}

void KshLexerFormatTest::testMarkdownItalicVsNormal()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");

    pineapple::MockDocument doc;
    doc.setText("*italic* normal");
    lexWholeDoc(lexer.get(), doc);

    auto styleAt = [&](int pos) -> char { return doc.styles.at(static_cast<size_t>(pos)); };

    const char italicStyle = styleAt(1);
    const char normalStyle = styleAt(9);
    QVERIFY2(italicStyle != normalStyle,
             "'*italic*' inner text should differ from 'normal' style");
    QVERIFY2(italicStyle >= STYLE_LASTPREDEFINED + 1,
             "'*italic*' inner text should use an extended styleId");
    QCOMPARE(normalStyle, 0);
}

void KshLexerFormatTest::testMarkdownBoldVsItalic()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");

    pineapple::MockDocument doc;
    doc.setText("**bold**\n*italic*");
    lexWholeDoc(lexer.get(), doc);

    auto styleAt = [&](int pos) -> char { return doc.styles.at(static_cast<size_t>(pos)); };

    const char boldStyle = styleAt(2);
    const char italicStyle = styleAt(10);
    QVERIFY2(boldStyle != italicStyle,
             "'**bold**' and '*italic*' should use different styleIds");
    QVERIFY(boldStyle >= STYLE_LASTPREDEFINED + 1);
    QVERIFY(italicStyle >= STYLE_LASTPREDEFINED + 1);
}

void KshLexerFormatTest::testNamedStylesReflectsExtendedCount()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });

    const int baseCount = lexer->NamedStyles();
    QVERIFY2(baseCount > 0, "Initial NamedStyles() should be positive");

    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");

    QVERIFY2(lexer->NamedStyles() > baseCount,
             "NamedStyles() should grow after a definition with Format overrides is set");
}

void KshLexerFormatTest::testPrimaryStyleFromStyle()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");

    const int named = lexer->NamedStyles();
    const int initialBase = baseStyleCount();

    QCOMPARE(lexer->PrimaryStyleFromStyle(0), 0);
    QVERIFY(lexer->PrimaryStyleFromStyle(1) < initialBase);

    for (int s = STYLE_LASTPREDEFINED + 1; s < named; ++s) {
        const int primary = lexer->PrimaryStyleFromStyle(s);
        QVERIFY2(primary >= 0 && primary < initialBase,
                 "PrimaryStyleFromStyle for extended styleId should return a base TextStyle id");
    }
}

void KshLexerFormatTest::testNameOfExtendedStyle()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");

    QCOMPARE(QByteArray(lexer->NameOfStyle(0)), QByteArray("Normal"));
    QCOMPARE(QByteArray(lexer->NameOfStyle(1)), QByteArray("Keyword"));

    const int named = lexer->NamedStyles();
    for (int s = STYLE_LASTPREDEFINED + 1; s < named; ++s) {
        const QByteArray name = lexer->NameOfStyle(s);
        QVERIFY2(!name.isEmpty(), "NameOfStyle for valid extended styleId should be non-empty");
    }

    QVERIFY(QByteArray(lexer->NameOfStyle(9999)).isEmpty());
}

void KshLexerFormatTest::testStylesPropertyParseable()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");
    lexer->PropertySet("lexer.ksyntaxhighlighting.theme", "Breeze Light");

    const int baseCount = baseStyleCount();

    const QList<StyleEntry> entries = parseStylesProperty(
        lexer->PropertyGet("lexer.ksyntaxhighlighting.styles"));
    QVERIFY(!entries.isEmpty());

    bool foundBold = false;
    bool foundItalic = false;
    for (const auto &e : entries) {
        QVERIFY(e.textStyleId >= 0);
        QVERIFY(e.textStyleId < baseCount);
        if (e.bold == 'B')
            foundBold = true;
        if (e.italic == 'I')
            foundItalic = true;
    }
    QVERIFY2(foundBold, "Markdown styles property should contain at least one bold entry");
    QVERIFY2(foundItalic, "Markdown styles property should contain at least one italic entry");
}

void KshLexerFormatTest::testScssHasExtendedStyles()
{
    if (!kshDefinitionAvailable("SCSS"))
        QSKIP("SCSS KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "SCSS");

    const int baseCount = baseStyleCount();

    QVERIFY2(lexer->NamedStyles() > baseCount,
             "SCSS definition is expected to declare some Format-level overrides");

    const QList<StyleEntry> entries = parseStylesProperty(
        lexer->PropertyGet("lexer.ksyntaxhighlighting.styles"));
    QVERIFY(!entries.isEmpty());
    for (const auto &e : entries)
        QVERIFY(e.styleId >= STYLE_LASTPREDEFINED + 1);
}

void KshLexerFormatTest::testExtendedStylesToggle()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    QCOMPARE(lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown"), 0);

    const int baseCount = baseStyleCount();

    // Default: extended styles enabled → NamedStyles > base count
    QVERIFY2(lexer->NamedStyles() > baseCount,
             "Extended styles should be enabled by default");
    QVERIFY2(lexer->PropertyGet("lexer.ksyntaxhighlighting.extended-styles")[0] == '1',
             "PropertyGet should report '1' by default");

    // Disable → NamedStyles drops to base count, styles property empty
    QCOMPARE(lexer->PropertySet("lexer.ksyntaxhighlighting.extended-styles", "0"), 0);
    QCOMPARE(lexer->NamedStyles(), baseCount);
    QVERIFY2(lexer->PropertyGet("lexer.ksyntaxhighlighting.extended-styles")[0] == '0',
             "PropertyGet should report '0' after disabling");
    QVERIFY(parseStylesProperty(
        lexer->PropertyGet("lexer.ksyntaxhighlighting.styles")).isEmpty());

    // Re-enable → extended styles restored
    QCOMPARE(lexer->PropertySet("lexer.ksyntaxhighlighting.extended-styles", "1"), 0);
    QVERIFY2(lexer->NamedStyles() > baseCount,
             "Extended styles should be restored after re-enabling");
    QVERIFY2(!parseStylesProperty(
        lexer->PropertyGet("lexer.ksyntaxhighlighting.styles")).isEmpty(),
        "Styles property should be non-empty after re-enabling");
}

void KshLexerFormatTest::testSpellCheckInExtendedStyles()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    QCOMPARE(lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown"), 0);
    lexer->PropertySet("lexer.ksyntaxhighlighting.theme", "Breeze Light");

    const QList<StyleEntry> entries = parseStylesProperty(
        lexer->PropertyGet("lexer.ksyntaxhighlighting.styles"));
    QVERIFY(!entries.isEmpty());

    bool foundSpellCheck = false;
    for (const auto &e : entries) {
        if (e.spellCheck == 'C') {
            foundSpellCheck = true;
            QVERIFY2(e.bold == 'B' || e.italic == 'I' || e.underline == 'U' || e.strike == 'S',
                     "spellCheck 'C' on extended style should accompany a font override");
            break;
        }
    }
    QVERIFY2(foundSpellCheck,
             "Markdown extended styles should include spell-checked formats (e.g. Strong Text)");
}

void KshLexerFormatTest::testBaseStyleSpellCheckEntries()
{
    if (!kshDefinitionAvailable("ISO C++"))
        QSKIP("ISO C++ KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    QCOMPARE(lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "ISO C++"), 0);
    lexer->PropertySet("lexer.ksyntaxhighlighting.theme", "Breeze Light");

    const QList<AllEntry> entries = parseAllEntries(
        lexer->PropertyGet("lexer.ksyntaxhighlighting.styles"));

    bool foundBaseSpellCheck = false;
    for (const auto &e : entries) {
        if (e.styleId < STYLE_LASTPREDEFINED + 1 && e.spellCheck == 'C') {
            foundBaseSpellCheck = true;
            QCOMPARE(e.styleId, e.textStyleId);
            break;
        }
    }
    QVERIFY2(foundBaseSpellCheck,
             "ISO C++ should produce base-style spellCheck entries for Comment/String");
}

void KshLexerFormatTest::testSpellCheckOnlyForTextStyles()
{
    if (!kshDefinitionAvailable("ISO C++"))
        QSKIP("ISO C++ KSH definition not available");

    const std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    QCOMPARE(lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "ISO C++"), 0);
    lexer->PropertySet("lexer.ksyntaxhighlighting.theme", "Breeze Light");

    const QList<AllEntry> entries = parseAllEntries(
        lexer->PropertyGet("lexer.ksyntaxhighlighting.styles"));

    for (const auto &e : entries) {
        if (e.styleId < STYLE_LASTPREDEFINED + 1) {
            QVERIFY2(e.spellCheck == 'C',
                     "Base-style entries must only exist for spellCheck-enabled formats");
        }
    }
}

QTEST_GUILESS_MAIN(KshLexerFormatTest)

#include "kshlexer_format_test.moc"
