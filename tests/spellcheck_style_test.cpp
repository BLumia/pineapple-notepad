/*
 * Tests for the spell check style pipeline:
 *  - static BaseStyles spellCheck declarations (Lexilla languages)
 *  - SciEdit's per-style lookup table filled from the KSH lexer
 *    serialization "C" flag (see researches/ksh-lexer.md 2.5 and
 *    researches/spellcheck.md)
 *  - plain-text mode (no lexer: style 0 is checked)
 *  - the Sonnet runtime itself (skipped when no dictionary is available)
 */
#include "basestyles.h"
#include "kshlexer.h"
#include "sciedit.h"

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>

#include <Sonnet/Speller>

#include <Scintilla.h>
#include <ILexer.h>
#include <Lexilla.h>

#include <QtTest/QtTest>

#include <memory>

using namespace Qt::StringLiterals;

class SpellCheckStyleTest : public QObject
{
    Q_OBJECT

private slots:
    void testLexillaBaseStylesSpellCheck();
    void testLexillaStyleSpecFromBaseStyles();
    void testKshStyleNeedsSpellCheck();
    void testPlainTextChecksEverything();
    void testSonnetSpeller();

private:
    static bool kshDefinitionAvailable(const char *name);
};

bool SpellCheckStyleTest::kshDefinitionAvailable(const char *name)
{
    return kshutils::repository().definitionForName(QString::fromUtf8(name)).isValid();
}

// The first-batch Lexilla languages must declare spell checking for their
// comment (and string family) styles; see researches/spellcheck.md Phase 3.
void SpellCheckStyleTest::testLexillaBaseStylesSpellCheck()
{
    struct Entry {
        QString lexer;
        QList<int> expectedTrue;
    };
    const QList<Entry> table = {
        { u"cpp"_s, {1, 2, 3, 6, 12, 13, 14, 15, 20, 21, 22, 23, 24} },
        // SCE_H_DEFAULT (0) is the prose text between tags, the main spell
        // check target in markup languages.
        { u"hypertext"_s, {0, 6, 7, 9, 20, 29, 30} },
        { u"xml"_s, {0, 6, 7, 9, 20} },
        { u"python"_s, {1, 3, 6, 7, 12, 13, 16, 18, 19} },
        { u"bash"_s, {2, 5, 11} },
        { u"yaml"_s, {1, 7} },
        { u"lua"_s, {1, 2, 3, 6, 8, 12} },
        { u"ruby"_s, {2, 3, 6, 12, 18, 24, 25, 26, 27, 28, 41, 42, 43, 44} },
        { u"toml"_s, {1, 9, 10, 11, 12, 15} },
        // JSON: string values and comments only; style 4 (SCE_JSON_PROPERTYNAME,
        // object keys) must stay unchecked.
        { u"json"_s, {2, 3, 6, 7} },
        { u"rust"_s, {1, 2, 3, 4, 13, 14, 21, 22, 24, 25} },
        { u"css"_s, {9, 13, 14} },
        { u"cmake"_s, {1, 2, 3, 4} },
        { u"props"_s, {1} },
        { u"batch"_s, {1} },
        { u"makefile"_s, {1} },
        { u"powershell"_s, {1, 2, 13, 14} },
        // AsciiDoc: prose markup, everything natural-language (default, strong,
        // emphasis, headings, blockquote, comments) is checked; code blocks,
        // literals, passthrough, links and attributes are not.
        { u"asciidoc"_s, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 13, 17, 18} },
        // LaTeX: document prose (0) and comments; math/verbatim unchecked.
        { u"latex"_s, {0, 4, 7} },
        { u"nix"_s, {1, 2, 3, 4, 16} },
        { u"nsis"_s, {1, 2, 3, 4, 18} },
        // PO: msgid/msgstr payloads and comments; msgctxt (7) is identifier-like.
        { u"po"_s, {1, 3, 5, 9, 12, 13} },
        { u"inno"_s, {1, 7, 10, 11} },
        // GDScript: 16 is SCE_GD_NODEPATH ($Path/To/Node), must stay unchecked.
        { u"gdscript"_s, {1, 3, 6, 7, 12} },
        // Dart: all four comment styles and the whole string family (5-12).
        { u"dart"_s, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12} },
        // Perl: comments, POD prose, plain and interpolated strings; regex,
        // v-string, backtick, here-doc, qw and POD verbatim stay unchecked.
        { u"perl"_s, {2, 3, 6, 26, 27} },
        // Zig: line/doc/top-level-doc comments, string and multiline string.
        { u"zig"_s, {1, 2, 3, 7, 8} },
    };

    for (const Entry &entry : table) {
        const QHash<int, StyleSpec> *styles = BaseStyles::stylesForLexer(entry.lexer);
        QVERIFY2(styles, qPrintable(u"no BaseStyles table for %1"_s.arg(entry.lexer)));
        for (const int styleId : entry.expectedTrue) {
            const auto it = styles->constFind(styleId);
            QVERIFY2(it != styles->constEnd(),
                     qPrintable(u"%1 has no style %2"_s.arg(entry.lexer).arg(styleId)));
            QVERIFY2(it.value().spellCheck,
                     qPrintable(u"%1 style %2 should be spell-checked"_s.arg(entry.lexer).arg(styleId)));
        }
        // Sanity: for code languages the default style (0) must not be
        // spell-checked (identifiers/normal text are skipped); document/markup
        // languages are the exception (style 0 is natural-language text).
        // JSON keys (SCE_JSON_PROPERTYNAME == 4) are identifiers as well.
        if (entry.lexer == u"hypertext"_s || entry.lexer == u"xml"_s
            || entry.lexer == u"asciidoc"_s || entry.lexer == u"latex"_s) {
            QVERIFY(styles->value(0).spellCheck);
        } else {
            QVERIFY(!styles->value(0).spellCheck);
        }
        if (entry.lexer == u"json"_s) {
            QVERIFY(!styles->value(4).spellCheck);
        }
        if (entry.lexer == u"gdscript"_s) {
            QVERIFY(!styles->value(16).spellCheck);
        }
        if (entry.lexer == u"po"_s) {
            QVERIFY(!styles->value(7).spellCheck);
        }
    }
}

// applyLexerStyles() must feed StyleSpec::spellCheck entries into
// SciEdit::styleNeedsSpellCheck(); exercised through a real Lexilla lexer.
void SpellCheckStyleTest::testLexillaStyleSpecFromBaseStyles()
{
    SciEdit edit;
    edit.setLexer(CreateLexer("cpp"), QStringLiteral("cpp"));
    edit.setText("// a misspelled coment\nint x = 0;\n");
    edit.colourise(0, -1); // widget not shown: force styling instead of idle-styling

    const sptr_t commentStyle = edit.styleAt(2);
    QCOMPARE(commentStyle, sptr_t(2)); // SCE_C_COMMENTLINE
    QVERIFY(edit.styleNeedsSpellCheck(int(commentStyle)));

    const sptr_t codeStyle = edit.styleAt(edit.positionFromLine(1) + 3); // the space after "int"
    QCOMPARE(codeStyle, sptr_t(0)); // SCE_C_DEFAULT
    QVERIFY(!edit.styleNeedsSpellCheck(int(codeStyle)));
}

// The KSH lexer serializes the per-Format spellCheck flag as 'C'; SciEdit
// must record it in the style lookup table (Markdown normal text is prose
// and spell-checked, keywords are not).
void SpellCheckStyleTest::testKshStyleNeedsSpellCheck()
{
    if (!kshDefinitionAvailable("Markdown")) {
        QSKIP("KSH definition 'Markdown' is not available");
    }

    SciEdit edit;
    Scintilla::ILexer5 *lexer = createKshLexer();
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");
    edit.setLexer(lexer, QStringLiteral("Markdown"));
    edit.setText("some prose\n");
    edit.colourise(0, -1);

    const sptr_t proseStyle = edit.styleAt(0);
    QVERIFY2(edit.styleNeedsSpellCheck(int(proseStyle)),
             "Markdown normal text (prose) should be spell-checked");
}

// Without a lexer (plain text) style 0 is the only style and must be checked.
void SpellCheckStyleTest::testPlainTextChecksEverything()
{
    SciEdit edit;
    QVERIFY(edit.styleNeedsSpellCheck(0));
    edit.setLexer(nullptr);
    QVERIFY(edit.styleNeedsSpellCheck(0));
}

// Runtime dictionary availability check; skipped when Sonnet finds no
// dictionary (e.g. clean Windows systems without the hunspell data).
void SpellCheckStyleTest::testSonnetSpeller()
{
    Sonnet::Speller speller;
    if (!speller.isValid()) {
        QSKIP("No spell check dictionary available");
    }
    QVERIFY(speller.isCorrect(QStringLiteral("hello")));
    QVERIFY(!speller.isCorrect(QStringLiteral("qlwkhz")));
}

QTEST_MAIN(SpellCheckStyleTest)
#include "spellcheck_style_test.moc"
