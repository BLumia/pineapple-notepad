/*
 * Tests for indentation style detection (IndentGuesser::guessIndentation),
 * the VS Code algorithm family. Cases mirror KTextEditor's
 * autotests/input/indent_detect fixtures plus Scintilla-specific edges.
 */
#include "indentguesser.h"
#include "sciedit.h"

#include <QtTest/QtTest>

class IndentDetectTest : public QObject
{
    Q_OBJECT

private:
    static SciEdit *createEditor(const QByteArray &text)
    {
        auto *edit = new SciEdit;
        edit->setText(text.constData());
        return edit;
    }

    static IndentDetectionResult detectContent(const QByteArray &text, int defaultWidth = 4,
                                                bool defaultSpaces = true)
    {
        SciEdit *edit = createEditor(text);
        const IndentDetectionResult result = IndentGuesser::guessIndentation(edit, defaultWidth, defaultSpaces);
        delete edit;
        return result;
    }

private slots:
    void twoSpaceIndent();
    void fourSpaceIndent();
    void tabIndent();
    void oneSpaceXml();
    void straySingleSpaceKeepsFour();
    void straySevenSpacesPrefersEvenWidth();
    void crlfLineEndings();
    void alignmentIsNotIndentation();
    void emptyDocumentReturnsDefaults();
    void emptyDocumentReturnsTabDefaults();
    void flatDocumentReturnsDefaults();
    void tabsMajorityWins();
    void spacesMajorityWins();
    void oversizedLineIsSafe();
    void scansAtMostTenThousandLines();
};

void IndentDetectTest::twoSpaceIndent()
{
    const auto result = detectContent("function foo() {\n"
                                      "  if (a) {\n"
                                      "    b();\n"
                                      "  }\n"
                                      "}\n");
    QCOMPARE(result.indentUsingSpaces, true);
    QCOMPARE(result.indentWidth, 2);
}

void IndentDetectTest::fourSpaceIndent()
{
    const auto result = detectContent("int main() {\n"
                                      "    int a = 1;\n"
                                      "    if (a) {\n"
                                      "        a++;\n"
                                      "    }\n"
                                      "    return 0;\n"
                                      "}\n");
    QCOMPARE(result.indentUsingSpaces, true);
    QCOMPARE(result.indentWidth, 4);
}

void IndentDetectTest::tabIndent()
{
    const auto result = detectContent("int main(void)\n"
                                      "{\n"
                                      "\tif (x)\n"
                                      "\t\treturn 1;\n"
                                      "\treturn 0;\n"
                                      "}\n");
    // Tab indentation: only the type is guessed, the width stays at default.
    QCOMPARE(result.indentUsingSpaces, false);
}

void IndentDetectTest::oneSpaceXml()
{
    const auto result = detectContent("<root>\n"
                                      " <a>\n"
                                      "  <b/>\n"
                                      " </a>\n"
                                      "</root>\n");
    QCOMPARE(result.indentUsingSpaces, true);
    QCOMPARE(result.indentWidth, 1);
}

void IndentDetectTest::straySingleSpaceKeepsFour()
{
    // A couple of stray 1-space indents must not drag the width down to 1.
    const auto result = detectContent("int main() {\n"
                                      "    int a = 1;\n"
                                      " if (a)\n"
                                      "        a++;\n"
                                      "    return 0;\n"
                                      "}\n");
    QCOMPARE(result.indentUsingSpaces, true);
    QCOMPARE(result.indentWidth, 4);
}

void IndentDetectTest::straySevenSpacesPrefersEvenWidth()
{
    // Odd widths only win when no even width has any votes (Kate bug 474505).
    const auto result = detectContent("# Title\n"
                                      "\n"
                                      "    normal code\n"
                                      "       bogus indent\n"
                                      "    more code\n");
    QCOMPARE(result.indentUsingSpaces, true);
    QCOMPARE(result.indentWidth, 4);
}

void IndentDetectTest::crlfLineEndings()
{
    const auto result = detectContent("function foo() {\r\n"
                                      "  if (a) {\r\n"
                                      "    b();\r\n"
                                      "  }\r\n"
                                      "}\r\n");
    QCOMPARE(result.indentUsingSpaces, true);
    QCOMPARE(result.indentWidth, 2);
}

void IndentDetectTest::alignmentIsNotIndentation()
{
    // The continuation line aligns to the first line's content after a
    // trailing comma; that must not be taken as an indent level of 6.
    const auto result = detectContent("const a = b + c,\n"
                                      "      d = b - c;\n"
                                      "if (x) {\n"
                                      "  y();\n"
                                      "}\n");
    QCOMPARE(result.indentUsingSpaces, true);
    QCOMPARE(result.indentWidth, 2);
}

void IndentDetectTest::emptyDocumentReturnsDefaults()
{
    const auto result = detectContent("", 4, true);
    QCOMPARE(result.indentUsingSpaces, true);
    QCOMPARE(result.indentWidth, 4);
}

void IndentDetectTest::emptyDocumentReturnsTabDefaults()
{
    const auto result = detectContent("", 8, false);
    QCOMPARE(result.indentUsingSpaces, false);
    QCOMPARE(result.indentWidth, 8);
}

void IndentDetectTest::flatDocumentReturnsDefaults()
{
    const auto result = detectContent("one\n"
                                      "two\n"
                                      "three\n");
    QCOMPARE(result.indentUsingSpaces, true);
    QCOMPARE(result.indentWidth, 4);
}

void IndentDetectTest::tabsMajorityWins()
{
    const auto result = detectContent("\ta\n"
                                      "\tb\n"
                                      "\tc\n"
                                      "\td\n"
                                      "    e\n"
                                      "    f\n");
    QCOMPARE(result.indentUsingSpaces, false);
}

void IndentDetectTest::spacesMajorityWins()
{
    const auto result = detectContent("\ta\n"
                                      "\tb\n"
                                      "    c\n"
                                      "    d\n"
                                      "    e\n"
                                      "    f\n");
    QCOMPARE(result.indentUsingSpaces, true);
}

void IndentDetectTest::oversizedLineIsSafe()
{
    // A single huge line (beyond the fetch cap) must neither crash nor
    // disturb the width guess of the surrounding regular code.
    const QByteArray hugeLine(200000, 'a');
    const auto result = detectContent("int main() {\n"
                                      "    int a = 1;\n"
                                      + hugeLine + "\n"
                                      "    return 0;\n"
                                      "}\n");
    QCOMPARE(result.indentUsingSpaces, true);
    QCOMPARE(result.indentWidth, 4);
}

void IndentDetectTest::scansAtMostTenThousandLines()
{
    // The 10001st line (2-space indented) is beyond the scan cap and must be
    // ignored, leaving the flat document at the default width.
    QString text;
    for (int i = 0; i < 10000; ++i) {
        text += QStringLiteral("line\n");
    }
    text += QStringLiteral("  indented\n");
    const auto result = detectContent(text.toUtf8());
    QCOMPARE(result.indentUsingSpaces, true);
    QCOMPARE(result.indentWidth, 4);
}

QTEST_MAIN(IndentDetectTest)
#include "indent_detect_test.moc"
