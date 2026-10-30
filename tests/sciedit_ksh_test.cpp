/*
 * Regression tests for SciEdit + KSH lexer integration.
 *
 * Bug history: SciEdit::applyLexerStyles() checked m_languageId ==
 * "ksyntaxhighlighting" to detect KSH lexers, but m_languageId holds the
 * language id override (e.g., the KSH definition name "Markdown"), not the
 * actual lexer name.
 * The check was always false for KSH-backed languages, so
 * applyKshExtendedStyles() never ran and extended per-Format styles
 * (bold/italic/foreground for styleIds >= STYLE_DEFAULT+1) were never
 * applied.  The fix changed the check to use lexerLanguage() (which calls
 * ILexer5::GetName() internally) instead of m_languageId.
 */
#include "sciedit.h"
#include "kshlexer.h"

#include <Scintilla.h>
#include <ILexer.h>

#include <QtTest/QtTest>

#include <memory>

class SciEditKshTest : public QObject
{
    Q_OBJECT

private slots:
    void testKshLexerNameInvariant();
    void testExtendedStylesAppliedWithLanguageOverride();
};

// The fix relies on ILexer5::GetName() returning "ksyntaxhighlighting".
// SciEdit::applyLexerStyles() compares lexerLanguage() against this string.
void SciEditKshTest::testKshLexerNameInvariant()
{
    std::unique_ptr<Scintilla::ILexer5, void(*)(Scintilla::ILexer5 *)> lexer(
        createKshLexer(), [](Scintilla::ILexer5 *l) { l->Release(); });
    QCOMPARE(QByteArray(lexer->GetName()), QByteArray("ksyntaxhighlighting"));
}

// Core regression test: setLexer with a languageIdOverride that differs from
// the lexer name must still detect the KSH lexer and apply extended styles.
// Before the fix, m_languageId was the override (not "ksyntaxhighlighting"),
// the detection check failed, and applyKshExtendedStyles() was never called.
// The definition property is set explicitly, mirroring what
// DocumentManager::applyLexerToEditor does for KSH-backed languages.
void SciEditKshTest::testExtendedStylesAppliedWithLanguageOverride()
{
    SciEdit edit;
    Scintilla::ILexer5 *lexer = createKshLexer();
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");
    edit.setLexer(lexer, QStringLiteral("Markdown"));

    // Markdown definition has Format-level overrides (bold headers, italic,
    // etc.), so extended styleIds (>= STYLE_DEFAULT+1) must exist.
    const sptr_t total = edit.namedStyles();
    QVERIFY2(total > STYLE_DEFAULT,
             qPrintable(QStringLiteral("Expected extended styles for Markdown, "
                                        "but namedStyles() returned %1").arg(total)));

    // applyKshExtendedStyles explicitly calls styleSetBold/styleSetItalic for
    // every extended entry.  If the function ran, at least one extended style
    // should be bold (Markdown headers) and at least one should be italic.
    bool anyBold = false;
    bool anyItalic = false;
    for (sptr_t s = STYLE_LASTPREDEFINED + 1; s < total; ++s) {
        if (edit.styleBold(s))
            anyBold = true;
        if (edit.styleItalic(s))
            anyItalic = true;
    }
    QVERIFY2(anyBold,
             "No extended style is bold - applyKshExtendedStyles likely did not run");
    QVERIFY2(anyItalic,
             "No extended style is italic - applyKshExtendedStyles likely did not run");
}

QTEST_MAIN(SciEditKshTest)
#include "sciedit_ksh_test.moc"
