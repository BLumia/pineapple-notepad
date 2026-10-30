#include "mockdocument.h"
#include "kshlexer.h"

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>

#include <QtTest/QtTest>

namespace {

bool kshDefinitionAvailable(const char *name)
{
    return kshutils::repository().definitionForName(QString::fromUtf8(name)).isValid();
}

void foldWholeDoc(Scintilla::ILexer5 *lexer, pineapple::MockDocument &doc)
{
    lexer->Fold(0, doc.Length(), 0, &doc);
}

int levelNum(int level)
{
    return level & SC_FOLDLEVELNUMBERMASK;
}

bool isHeader(int level)
{
    return (level & SC_FOLDLEVELHEADERFLAG) != 0;
}

bool isWhite(int level)
{
    return (level & SC_FOLDLEVELWHITEFLAG) != 0;
}

} // namespace

class KshLexerIndentFoldTest : public QObject {
    Q_OBJECT

private slots:
    void testBasicNesting();
    void testNestedDepth();
    void testBlankLinesTransparent();
    void testCommentLinesTransparent();
    void testIndentedCommentNotTransparent();
    void testDedentToZero();
    void testSiblingDedent();
    void testTripleQuotedString();
    void testTabIndentation();
    void testEmptyDocument();
};

void KshLexerIndentFoldTest::testBasicNesting()
{
    if (!kshDefinitionAvailable("Python"))
        QSKIP("Python KSH definition not available");

    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("def foo():\n"
                               "    x = 1\n"
                               "    y = 2\n")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Python");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    // L0: def foo(): indent=0, next non-empty indent=4 → header
    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(0)));

    // L1: x = 1, indent=4 → inside fold, no header
    QCOMPARE(levelNum(doc.GetLevel(1)), SC_FOLDLEVELBASE + 4);
    QVERIFY(!isHeader(doc.GetLevel(1)));

    // L2: y = 2, indent=4 → same level, no header
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE + 4);
    QVERIFY(!isHeader(doc.GetLevel(2)));

    lexer->Release();
}

void KshLexerIndentFoldTest::testNestedDepth()
{
    if (!kshDefinitionAvailable("Python"))
        QSKIP("Python KSH definition not available");

    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("def foo():\n"
                               "    if True:\n"
                               "        pass\n"
                               "    x = 1\n")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Python");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    // L0: def foo(): indent=0 → header
    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(0)));

    // L1: if True: indent=4 → header (next indent=8)
    QCOMPARE(levelNum(doc.GetLevel(1)), SC_FOLDLEVELBASE + 4);
    QVERIFY(isHeader(doc.GetLevel(1)));

    // L2: pass, indent=8 → deepest level
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE + 8);
    QVERIFY(!isHeader(doc.GetLevel(2)));

    // L3: x = 1, indent=4 → back to level 1 (sibling of if)
    QCOMPARE(levelNum(doc.GetLevel(3)), SC_FOLDLEVELBASE + 4);
    QVERIFY(!isHeader(doc.GetLevel(3)));

    lexer->Release();
}

void KshLexerIndentFoldTest::testBlankLinesTransparent()
{
    if (!kshDefinitionAvailable("Python"))
        QSKIP("Python KSH definition not available");

    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("def foo():\n"
                               "\n"
                               "    x = 1\n"
                               "\n"
                               "    y = 2\n")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Python");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    // L0: def foo(): indent=0 → header (skips blank, next non-empty indent=4)
    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(0)));

    // L1: blank → transparent, inherits next non-empty level (BASE+4)
    QCOMPARE(isWhite(doc.GetLevel(1)), true);
    QVERIFY(levelNum(doc.GetLevel(1)) >= SC_FOLDLEVELBASE + 1);

    // L2: x = 1, indent=4
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE + 4);

    // L3: blank → transparent
    QCOMPARE(isWhite(doc.GetLevel(3)), true);

    // L4: y = 2, indent=4
    QCOMPARE(levelNum(doc.GetLevel(4)), SC_FOLDLEVELBASE + 4);

    lexer->Release();
}

void KshLexerIndentFoldTest::testCommentLinesTransparent()
{
    if (!kshDefinitionAvailable("Python"))
        QSKIP("Python KSH definition not available");

    // Unindented comment lines match foldingIgnoreList regex fully (\s*#.*)
    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("# top-level comment\n"
                               "def foo():\n"
                               "    x = 1\n")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Python");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    // L0: # comment → matches \s*#.* fully → transparent
    QCOMPARE(isWhite(doc.GetLevel(0)), true);

    // L1: def foo(): → header
    QCOMPARE(levelNum(doc.GetLevel(1)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(1)));

    lexer->Release();
}

void KshLexerIndentFoldTest::testIndentedCommentNotTransparent()
{
    if (!kshDefinitionAvailable("Python"))
        QSKIP("Python KSH definition not available");

    // Indented comment lines: \s+ matches leading whitespace partially,
    // so capturedLength != line length → NOT treated as empty.
    // The line's indentation counts normally.
    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("def foo():\n"
                               "    # a comment\n"
                               "    x = 1\n")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Python");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    // L0: def foo(): indent=0, next non-transparent (comment at indent=4) → header
    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(0)));

    // L1: # comment at indent=4 → NOT white, regular code line at indent=4
    QVERIFY(!isWhite(doc.GetLevel(1)));
    QCOMPARE(levelNum(doc.GetLevel(1)), SC_FOLDLEVELBASE + 4);
    QVERIFY(!isHeader(doc.GetLevel(1)));

    // L2: x = 1, indent=4
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE + 4);
    QVERIFY(!isHeader(doc.GetLevel(2)));

    lexer->Release();
}

void KshLexerIndentFoldTest::testDedentToZero()
{
    if (!kshDefinitionAvailable("Python"))
        QSKIP("Python KSH definition not available");

    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("def foo():\n"
                               "    x = 1\n"
                               "def bar():\n"
                               "    y = 2\n")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Python");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    // L0: def foo(): indent=0 → header
    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(0)));

    // L1: x = 1, indent=4 → inside foo
    QCOMPARE(levelNum(doc.GetLevel(1)), SC_FOLDLEVELBASE + 4);

    // L2: def bar(): indent=0 → sibling header
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(2)));

    // L3: y = 2, indent=4 → inside bar
    QCOMPARE(levelNum(doc.GetLevel(3)), SC_FOLDLEVELBASE + 4);

    lexer->Release();
}

void KshLexerIndentFoldTest::testSiblingDedent()
{
    if (!kshDefinitionAvailable("Python"))
        QSKIP("Python KSH definition not available");

    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("class Foo:\n"
                               "    def method_a(self):\n"
                               "        pass\n"
                               "    def method_b(self):\n"
                               "        pass\n")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Python");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    // L0: class Foo: indent=0 → header
    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(0)));

    // L1: def method_a: indent=4 → header (next indent=8)
    QCOMPARE(levelNum(doc.GetLevel(1)), SC_FOLDLEVELBASE + 4);
    QVERIFY(isHeader(doc.GetLevel(1)));

    // L2: pass, indent=8
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE + 8);
    QVERIFY(!isHeader(doc.GetLevel(2)));

    // L3: def method_b: indent=4 → sibling header (dedent from 8 to 4)
    QCOMPARE(levelNum(doc.GetLevel(3)), SC_FOLDLEVELBASE + 4);
    QVERIFY(isHeader(doc.GetLevel(3)));

    // L4: pass, indent=8
    QCOMPARE(levelNum(doc.GetLevel(4)), SC_FOLDLEVELBASE + 8);
    QVERIFY(!isHeader(doc.GetLevel(4)));

    lexer->Release();
}

void KshLexerIndentFoldTest::testTripleQuotedString()
{
    if (!kshDefinitionAvailable("Python"))
        QSKIP("Python KSH definition not available");

    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("def foo():\n"
                               "    x = \"\"\"\n"
                               "        indented text\n"
                               "    \"\"\"\n"
                               "    y = 2\n")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Python");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    // L0: def foo(): indent=0 → header
    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(0)));

    // L1: x = """ ... indent=4 → not a header (next line is inside string → transparent)
    QCOMPARE(levelNum(doc.GetLevel(1)), SC_FOLDLEVELBASE + 4);

    // L2: "    indented text" → inside triple-quote string, indentation folding suspended
    //    Should be transparent (WHITE flag)
    QCOMPARE(isWhite(doc.GetLevel(2)), true);

    // L3: "    \"\"\"" → closes the string, still transparent (state at end of line
    //    is still inside string context, or just exited)
    // L4: y = 2, indent=4 → normal code line inside def
    QCOMPARE(levelNum(doc.GetLevel(4)), SC_FOLDLEVELBASE + 4);
    QVERIFY(!isHeader(doc.GetLevel(4)));

    lexer->Release();
}

void KshLexerIndentFoldTest::testTabIndentation()
{
    if (!kshDefinitionAvailable("Python"))
        QSKIP("Python KSH definition not available");

    // Use tab-indented Python (tabwidth=4)
    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("def foo():\n"
                               "\tx = 1\n"
                               "\t\tif True:\n"
                               "\t\t\tpass\n")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Python");
    lexer->PropertySet("lexer.ksyntaxhighlighting.tabwidth", "4");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    // L0: def foo(): indent=0 → header
    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(0)));

    // L1: \tx = 1 → tab expands to 4, indent=4
    QCOMPARE(levelNum(doc.GetLevel(1)), SC_FOLDLEVELBASE + 4);

    // L2: \t\tif True: → indent=8, header (next indent=12)
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE + 8);
    QVERIFY(isHeader(doc.GetLevel(2)));

    // L3: \t\t\tpass → indent=12
    QCOMPARE(levelNum(doc.GetLevel(3)), SC_FOLDLEVELBASE + 12);

    lexer->Release();
}

void KshLexerIndentFoldTest::testEmptyDocument()
{
    if (!kshDefinitionAvailable("Python"))
        QSKIP("Python KSH definition not available");

    pineapple::MockDocument doc;
    doc.setText("");

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Python");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    QCOMPARE(doc.lineCount(), 1);

    lexer->Release();
}

QTEST_GUILESS_MAIN(KshLexerIndentFoldTest)

#include "kshlexer_indentfold_test.moc"
