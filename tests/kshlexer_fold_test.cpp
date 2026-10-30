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

class KshLexerFoldTest : public QObject {
    Q_OBJECT

private slots:
    void testMarkdownSiblingHeadings_data();
    void testMarkdownSiblingHeadings();
    void testMarkdownFirstHeading();
    void testMarkdownNestedHeadings();
    void testMarkdownHeadingDepthDecrease();
    void testMarkdownSiblingH1AfterNested();
    void testEmptyDocument();
};

void KshLexerFoldTest::testMarkdownSiblingHeadings_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("expectLine");
    QTest::addColumn<int>("expectLevelNum");
    QTest::addColumn<bool>("expectHeader");

    const QString body =
        QStringLiteral("## 1st h2\n"
                       "\n"
                       "para1\n"
                       "\n"
                       "## 2nd h2\n"
                       "\n"
                       "para2");

    QTest::newRow("L0 first H2 is a header at BASE") << body << 0 << SC_FOLDLEVELBASE << true;
    QTest::newRow("L1 blank under 1st H2 is BASE+1") << body << 1 << (SC_FOLDLEVELBASE + 1) << false;
    QTest::newRow("L2 para1 is BASE+1") << body << 2 << (SC_FOLDLEVELBASE + 1) << false;
    QTest::newRow("L4 second H2 is a sibling header at BASE") << body << 4 << SC_FOLDLEVELBASE << true;
    QTest::newRow("L5 blank under 2nd H2 is BASE+1") << body << 5 << (SC_FOLDLEVELBASE + 1) << false;
    QTest::newRow("L6 para2 is BASE+1") << body << 6 << (SC_FOLDLEVELBASE + 1) << false;
}

void KshLexerFoldTest::testMarkdownSiblingHeadings()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    QFETCH(QString, text);
    QFETCH(int, expectLine);
    QFETCH(int, expectLevelNum);
    QFETCH(bool, expectHeader);

    pineapple::MockDocument doc;
    doc.setText(text.toUtf8().constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    QCOMPARE(lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown"), 0);
    QCOMPARE(lexer->PropertySet("fold", "1"), 0);

    foldWholeDoc(lexer, doc);

    QCOMPARE(levelNum(doc.GetLevel(expectLine)), expectLevelNum);
    QCOMPARE(isHeader(doc.GetLevel(expectLine)), expectHeader);

    lexer->Release();
}

void KshLexerFoldTest::testMarkdownFirstHeading()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("# Title\n"
                               "\n"
                               "content")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QCOMPARE(isHeader(doc.GetLevel(0)), true);
    QCOMPARE(levelNum(doc.GetLevel(1)), SC_FOLDLEVELBASE + 1);
    QCOMPARE(isWhite(doc.GetLevel(1)), true);
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE + 1);

    lexer->Release();
}

void KshLexerFoldTest::testMarkdownNestedHeadings()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("# H1\n"
                               "\n"
                               "## H2\n"
                               "\n"
                               "content\n"
                               "\n"
                               "## H2 again")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QCOMPARE(isHeader(doc.GetLevel(0)), true);
    QCOMPARE(levelNum(doc.GetLevel(1)), SC_FOLDLEVELBASE + 1);
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE + 1);
    QCOMPARE(isHeader(doc.GetLevel(2)), true);
    QCOMPARE(levelNum(doc.GetLevel(3)), SC_FOLDLEVELBASE + 2);
    QCOMPARE(levelNum(doc.GetLevel(4)), SC_FOLDLEVELBASE + 2);
    QCOMPARE(levelNum(doc.GetLevel(5)), SC_FOLDLEVELBASE + 2);
    QCOMPARE(levelNum(doc.GetLevel(6)), SC_FOLDLEVELBASE + 1);
    QCOMPARE(isHeader(doc.GetLevel(6)), true);

    lexer->Release();
}

void KshLexerFoldTest::testMarkdownHeadingDepthDecrease()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("## H2\n"
                               "\n"
                               "### H3\n"
                               "\n"
                               "content\n"
                               "\n"
                               "# H1")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QCOMPARE(isHeader(doc.GetLevel(0)), true);
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE + 1);
    QCOMPARE(isHeader(doc.GetLevel(2)), true);
    QCOMPARE(levelNum(doc.GetLevel(4)), SC_FOLDLEVELBASE + 2);
    // # H1 closes H3, H2, then opens H1 — depth visits 0 before returning to 1.
    QCOMPARE(levelNum(doc.GetLevel(6)), SC_FOLDLEVELBASE);
    QCOMPARE(isHeader(doc.GetLevel(6)), true);

    lexer->Release();
}

void KshLexerFoldTest::testMarkdownSiblingH1AfterNested()
{
    if (!kshDefinitionAvailable("Markdown"))
        QSKIP("Markdown KSH definition not available");

    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("# H1\n"
                               "\n"
                               "## H2\n"
                               "\n"
                               "content\n"
                               "\n"
                               "# H1 again")
                    .toUtf8()
                    .constData());

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    // L0: # H1 -> header at BASE
    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QCOMPARE(isHeader(doc.GetLevel(0)), true);
    // L2: ## H2 -> header at BASE+1
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE + 1);
    QCOMPARE(isHeader(doc.GetLevel(2)), true);
    // L4: content -> BASE+2
    QCOMPARE(levelNum(doc.GetLevel(4)), SC_FOLDLEVELBASE + 2);
    // L6: # H1 again -> closes H2 and old H1, opens new H1, sibling at BASE
    QCOMPARE(levelNum(doc.GetLevel(6)), SC_FOLDLEVELBASE);
    QCOMPARE(isHeader(doc.GetLevel(6)), true);

    lexer->Release();
}

void KshLexerFoldTest::testEmptyDocument()
{
    pineapple::MockDocument doc;
    doc.setText("");

    Scintilla::ILexer5 *lexer = createKshLexer();
    QVERIFY(lexer);
    lexer->PropertySet("lexer.ksyntaxhighlighting.definition", "Markdown");
    lexer->PropertySet("fold", "1");

    foldWholeDoc(lexer, doc);

    QCOMPARE(doc.lineCount(), 1);

    lexer->Release();
}

QTEST_GUILESS_MAIN(KshLexerFoldTest)

#include "kshlexer_fold_test.moc"
