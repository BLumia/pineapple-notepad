#include "mockdocument.h"
#include "searchresultlexer.h"

#include <QtTest/QtTest>

#include <Scintilla.h>

#include <vector>

using namespace pineapple;

namespace {

int styleAt(const MockDocument &doc, int position)
{
    return doc.StyleAt(position);
}

int levelNum(int level)
{
    return level & SC_FOLDLEVELNUMBERMASK;
}

bool isHeader(int level)
{
    return (level & SC_FOLDLEVELHEADERFLAG) != 0;
}

} // namespace

class SearchResultLexerTest : public QObject
{
    Q_OBJECT

private slots:
    void testLineClassification();
    void testResultLineStyles();
    void testWordToSearchUtf8Offsets();
    void testLocalizedLabelWithPaddedNumber();
    void testMultipleSegmentsPerLine();
    void testMarkingClampedToLineEnd();
    void testNoMarkings();
    void testFoldLevels();
    void testPartialLexUsesGlobalLineNumbers();
};

void SearchResultLexerTest::testLineClassification()
{
    pineapple::MockDocument doc;
    doc.setText("Search \"foo\"\r\n"
                "  a.cpp\r\n"
                "\tLine 5: int foo = 0;");

    SearchResultLexer *lexer = createSearchResultLexer();
    lexer->Lex(0, doc.Length(), 0, &doc);

    QCOMPARE(styleAt(doc, 0), SearchResultStyleSearchHeader);
    QCOMPARE(styleAt(doc, 15), SearchResultStyleFileHeader);
    QCOMPARE(styleAt(doc, 24), SearchResultStyleDefault); // the leading tab

    lexer->Release();
}

void SearchResultLexerTest::testResultLineStyles()
{
    // "\tLine 5: int foo = 0;"
    const std::string line = "\tLine 5: int foo = 0;";
    pineapple::MockDocument doc;
    doc.setText(line);

    std::vector<SearchResultMarkingLine> markings(1);
    // "foo" at content bytes 4..7, prefix "\tLine 5: " is 9 bytes.
    markings[0].segments.push_back({9 + 4, 9 + 7});

    SearchResultLexer *lexer = createSearchResultLexer();
    lexer->setMarkings(&markings);
    lexer->Lex(0, doc.Length(), 0, &doc);

    QCOMPARE(styleAt(doc, 0), SearchResultStyleDefault);              // '\t'
    QCOMPARE(styleAt(doc, 1), SearchResultStyleLineNumber);           // 'L'
    QCOMPARE(styleAt(doc, 7), SearchResultStyleLineNumber);           // ':' in "Line 5:"
    QCOMPARE(styleAt(doc, 8), SearchResultStyleDefault);              // ' '
    QCOMPARE(styleAt(doc, 9 + 3), SearchResultStyleDefault);          // ' ' before "foo"
    QCOMPARE(styleAt(doc, 9 + 4), SearchResultStyleWordToSearch);     // 'f'
    QCOMPARE(styleAt(doc, 9 + 6), SearchResultStyleWordToSearch);     // 'o'
    QCOMPARE(styleAt(doc, 9 + 7), SearchResultStyleDefault);          // ' '
    QCOMPARE(styleAt(doc, int(line.size()) - 1), SearchResultStyleDefault);

    lexer->Release();
}

void SearchResultLexerTest::testWordToSearchUtf8Offsets()
{
    // Result line: "\tLine 1: <content>"; content "你好X" where the match is
    // "好X" (UTF-16 indices 1..3 of the content, UTF-8 bytes 3..7).
    const QString content = QStringLiteral("你好X");
    pineapple::MockDocument doc;
    doc.setText((QStringLiteral("\tLine 1: ") + content).toUtf8().constData());

    const int prefixBytes = 9; // "\tLine 1: "
    std::vector<SearchResultMarkingLine> markings(1);
    markings[0].segments.push_back({prefixBytes + 3, prefixBytes + 7});

    SearchResultLexer *lexer = createSearchResultLexer();
    lexer->setMarkings(&markings);
    lexer->Lex(0, doc.Length(), 0, &doc);

    QCOMPARE(styleAt(doc, prefixBytes + 0), SearchResultStyleDefault);      // 你
    QCOMPARE(styleAt(doc, prefixBytes + 2), SearchResultStyleDefault);
    QCOMPARE(styleAt(doc, prefixBytes + 3), SearchResultStyleWordToSearch); // 好
    QCOMPARE(styleAt(doc, prefixBytes + 5), SearchResultStyleWordToSearch);
    QCOMPARE(styleAt(doc, doc.Length() - 1), SearchResultStyleWordToSearch); // X (last byte)

    lexer->Release();
}

void SearchResultLexerTest::testLocalizedLabelWithPaddedNumber()
{
    // Result line with a translated (multi-byte) label and a space-padded
    // line number: "\t行  8: 你好X". The prefix layout in bytes:
    // \t=0, 行=1..3, ' '=4, ' '=5, '8'=6, ':'=7, ' '=8, content from 9.
    pineapple::MockDocument doc;
    doc.setText(QStringLiteral("\t行  8: 你好X").toUtf8().constData());

    std::vector<SearchResultMarkingLine> markings(1);
    // Content "你好X" (7 bytes); match "好X" at content bytes 3..7.
    markings[0].segments.push_back({9 + 3, 9 + 7});

    SearchResultLexer *lexer = createSearchResultLexer();
    lexer->setMarkings(&markings);
    lexer->Lex(0, doc.Length(), 0, &doc);

    QCOMPARE(styleAt(doc, 0), SearchResultStyleDefault);           // '\t'
    QCOMPARE(styleAt(doc, 1), SearchResultStyleLineNumber);        // 行
    QCOMPARE(styleAt(doc, 4), SearchResultStyleLineNumber);        // padding space
    QCOMPARE(styleAt(doc, 6), SearchResultStyleLineNumber);        // '8'
    QCOMPARE(styleAt(doc, 7), SearchResultStyleLineNumber);        // ':'
    QCOMPARE(styleAt(doc, 8), SearchResultStyleDefault);           // ' '
    QCOMPARE(styleAt(doc, 9 + 3), SearchResultStyleWordToSearch);  // 好
    QCOMPARE(styleAt(doc, doc.Length() - 1), SearchResultStyleWordToSearch); // X

    lexer->Release();
}

void SearchResultLexerTest::testMultipleSegmentsPerLine()
{
    // A merged entry (several matches on one source line) carries several
    // segments on the same buffer line: "\tLine 1: foo bar foo"
    pineapple::MockDocument doc;
    doc.setText("\tLine 1: foo bar foo");

    std::vector<SearchResultMarkingLine> markings(1);
    // Prefix "\tLine 1: " is 9 bytes; matches at content bytes 0..3 and 8..11.
    markings[0].segments.push_back({9 + 0, 9 + 3});
    markings[0].segments.push_back({9 + 8, 9 + 11});

    SearchResultLexer *lexer = createSearchResultLexer();
    lexer->setMarkings(&markings);
    lexer->Lex(0, doc.Length(), 0, &doc);

    QCOMPARE(styleAt(doc, 9 + 0), SearchResultStyleWordToSearch);  // 'f' (1st)
    QCOMPARE(styleAt(doc, 9 + 2), SearchResultStyleWordToSearch);  // 'o' (1st)
    QCOMPARE(styleAt(doc, 9 + 3), SearchResultStyleDefault);       // ' '
    QCOMPARE(styleAt(doc, 9 + 4), SearchResultStyleDefault);       // "bar"
    QCOMPARE(styleAt(doc, 9 + 6), SearchResultStyleDefault);
    QCOMPARE(styleAt(doc, 9 + 8), SearchResultStyleWordToSearch);  // 'f' (2nd)
    QCOMPARE(styleAt(doc, doc.Length() - 1), SearchResultStyleWordToSearch);

    lexer->Release();
}

void SearchResultLexerTest::testMarkingClampedToLineEnd()
{
    pineapple::MockDocument doc;
    doc.setText("\tLine 1: abc");

    std::vector<SearchResultMarkingLine> markings(1);
    markings[0].segments.push_back({0, 99999});

    SearchResultLexer *lexer = createSearchResultLexer();
    lexer->setMarkings(&markings);
    lexer->Lex(0, doc.Length(), 0, &doc);

    // Everything from the content start to the end of the line is marked,
    // and nothing is written out of range.
    QCOMPARE(styleAt(doc, 9), SearchResultStyleWordToSearch);
    QCOMPARE(styleAt(doc, doc.Length() - 1), SearchResultStyleWordToSearch);
    QCOMPARE(doc.styles.size(), static_cast<size_t>(doc.Length()));

    lexer->Release();
}

void SearchResultLexerTest::testNoMarkings()
{
    // No setMarkings() call at all: classification still works and result
    // lines degrade to default styling.
    pineapple::MockDocument doc;
    doc.setText("Search \"foo\"\r\n"
                "  a.cpp\r\n"
                "\tLine 5: int foo = 0;");

    SearchResultLexer *lexer = createSearchResultLexer();
    lexer->Lex(0, doc.Length(), 0, &doc);

    QCOMPARE(styleAt(doc, 0), SearchResultStyleSearchHeader);
    QCOMPARE(styleAt(doc, 15), SearchResultStyleFileHeader);
    for (Sci_Position i = 24; i < doc.Length(); ++i) {
        QVERIFY(styleAt(doc, int(i)) == SearchResultStyleDefault
                || styleAt(doc, int(i)) == SearchResultStyleLineNumber);
    }

    lexer->Release();
}

void SearchResultLexerTest::testFoldLevels()
{
    pineapple::MockDocument doc;
    doc.setText("Search \"foo\"\r\n"       // line 0: search header
                "  a.cpp\r\n"              // line 1: file header
                "\tLine 5: int foo;\r\n"   // line 2: result
                "\tLine 6: foo();\r\n"     // line 3: result
                "Search \"bar\"\r\n"       // line 4: search header
                "  b.cpp\r\n"              // line 5: file header
                "\tLine 1: bar;");         // line 6: result

    SearchResultLexer *lexer = createSearchResultLexer();
    lexer->Fold(0, doc.Length(), 0, &doc);

    QCOMPARE(levelNum(doc.GetLevel(0)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(0)));
    QCOMPARE(levelNum(doc.GetLevel(1)), SC_FOLDLEVELBASE + 1);
    QVERIFY(isHeader(doc.GetLevel(1)));
    QCOMPARE(levelNum(doc.GetLevel(2)), SC_FOLDLEVELBASE + 2);
    QVERIFY(!isHeader(doc.GetLevel(2)));
    QCOMPARE(levelNum(doc.GetLevel(3)), SC_FOLDLEVELBASE + 2);
    QVERIFY(!isHeader(doc.GetLevel(3)));
    QCOMPARE(levelNum(doc.GetLevel(4)), SC_FOLDLEVELBASE);
    QVERIFY(isHeader(doc.GetLevel(4)));
    QCOMPARE(levelNum(doc.GetLevel(5)), SC_FOLDLEVELBASE + 1);
    QVERIFY(isHeader(doc.GetLevel(5)));
    QCOMPARE(levelNum(doc.GetLevel(6)), SC_FOLDLEVELBASE + 2);
    QVERIFY(!isHeader(doc.GetLevel(6)));

    lexer->Release();
}

void SearchResultLexerTest::testPartialLexUsesGlobalLineNumbers()
{
    pineapple::MockDocument doc;
    doc.setText("Search \"foo\"\r\n"
                "  a.cpp\r\n"
                "\tLine 5: foo;\r\n"
                "\tLine 6: bar(foo);");

    // Markings only for line 3 (global line number), the second result line.
    // Line 3 is "\tLine 6: bar(foo);": prefix "\tLine 6: " is 9 bytes and
    // "foo" sits at content bytes 4..7, i.e. line-relative bytes 13..16.
    std::vector<SearchResultMarkingLine> markings(4);
    const int line3Start = int(doc.LineStart(3));
    markings[3].segments.push_back({13, 16});

    SearchResultLexer *lexer = createSearchResultLexer();
    lexer->setMarkings(&markings);
    // Style only from the start of line 3, as Scintilla does after appends.
    lexer->Lex(line3Start, doc.Length() - line3Start, 0, &doc);

    QCOMPARE(styleAt(doc, line3Start + 12), SearchResultStyleDefault);      // '('
    QCOMPARE(styleAt(doc, line3Start + 13), SearchResultStyleWordToSearch); // 'f'
    QCOMPARE(styleAt(doc, line3Start + 15), SearchResultStyleWordToSearch); // 'o'
    QCOMPARE(styleAt(doc, line3Start + 16), SearchResultStyleDefault);      // ')'
    // Earlier lines are untouched by this call.
    QCOMPARE(styleAt(doc, 0), 0);

    lexer->Release();
}

QTEST_GUILESS_MAIN(SearchResultLexerTest)

#include "searchresultlexer_test.moc"
