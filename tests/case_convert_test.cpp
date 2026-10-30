/*
 * Tests for case conversion functionality (Proper Case, Sentence case, etc.).
 */
#include "sciedit.h"

#include <Scintilla.h>
#include <ILexer.h>
#include <Lexilla.h>

#include <QtTest/QtTest>

class CaseConvertTest : public QObject
{
    Q_OBJECT

private:
    static SciEdit *createEditor()
    {
        auto *edit = new SciEdit;
        edit->setLexer(CreateLexer("cpp"), QStringLiteral("cpp"));
        return edit;
    }

    static QByteArray allText(SciEdit *e)
    {
        return e->getText(e->textLength());
    }

    static void selectAll(SciEdit *e)
    {
        e->setSel(0, e->textLength());
    }

private slots:
    // --- Proper Case (force) ---
    void properCase_mixedCase();
    void properCase_allUpper();
    void properCase_allLower();
    void properCase_singleWord();
    void properCase_contraction();
    void properCase_withNumbers();
    void properCase_multipleSpaces();
    void properCase_emptySelection();

    // --- Proper Case (blend) ---
    void properCaseBlend_mixedCase();
    void properCaseBlend_allUpper();
    void properCaseBlend_allLower();
    void properCaseBlend_contraction();

    // --- Sentence case (force) ---
    void sentenceCase_basic();
    void sentenceCase_allUpper();
    void sentenceCase_multipleSentences();
    void sentenceCase_abbreviation();
    void sentenceCase_pronounI();
    void sentenceCase_blankLineNewParagraph();
    void sentenceCase_emptySelection();

    // --- Sentence case (blend) ---
    void sentenceCaseBlend_basic();
    void sentenceCaseBlend_preservesCase();

    // --- Invert case ---
    void invertCase_basic();
    void invertCase_allUpper();
    void invertCase_allLower();
    void invertCase_mixed();
    void invertCase_nonLetters();
    void invertCase_emptySelection();

    // --- Random case ---
    void randomCase_lengthPreserved();
    void randomCase_nonLettersUnchanged();
    void randomCase_lettersPresent();

    // --- Multi-selection ---
    void properCase_multiSelection();
    void invertCase_multiSelection();
};

// ============================================================
// Proper Case (force)
// ============================================================

void CaseConvertTest::properCase_mixedCase()
{
    SciEdit *e = createEditor();
    e->setText("hELLo wORLD");
    selectAll(e);
    e->properCase();
    QCOMPARE(allText(e), QByteArray("Hello World"));
    delete e;
}

void CaseConvertTest::properCase_allUpper()
{
    SciEdit *e = createEditor();
    e->setText("HELLO WORLD");
    selectAll(e);
    e->properCase();
    QCOMPARE(allText(e), QByteArray("Hello World"));
    delete e;
}

void CaseConvertTest::properCase_allLower()
{
    SciEdit *e = createEditor();
    e->setText("hello world");
    selectAll(e);
    e->properCase();
    QCOMPARE(allText(e), QByteArray("Hello World"));
    delete e;
}

void CaseConvertTest::properCase_singleWord()
{
    SciEdit *e = createEditor();
    e->setText("tEsT");
    selectAll(e);
    e->properCase();
    QCOMPARE(allText(e), QByteArray("Test"));
    delete e;
}

void CaseConvertTest::properCase_contraction()
{
    SciEdit *e = createEditor();
    e->setText("don't stop");
    selectAll(e);
    e->properCase();
    QCOMPARE(allText(e), QByteArray("Don't Stop"));
    delete e;
}

void CaseConvertTest::properCase_withNumbers()
{
    SciEdit *e = createEditor();
    e->setText("hello2 world");
    selectAll(e);
    e->properCase();
    QCOMPARE(allText(e), QByteArray("Hello2 World"));
    delete e;
}

void CaseConvertTest::properCase_multipleSpaces()
{
    SciEdit *e = createEditor();
    e->setText("hello  world");
    selectAll(e);
    e->properCase();
    QCOMPARE(allText(e), QByteArray("Hello  World"));
    delete e;
}

void CaseConvertTest::properCase_emptySelection()
{
    SciEdit *e = createEditor();
    e->setText("hello");
    e->setEmptySelection(2);
    e->properCase();
    QCOMPARE(allText(e), QByteArray("hello"));
    delete e;
}

// ============================================================
// Proper Case (blend)
// ============================================================

void CaseConvertTest::properCaseBlend_mixedCase()
{
    SciEdit *e = createEditor();
    e->setText("hELLo wORLD");
    selectAll(e);
    e->properCaseBlend();
    QCOMPARE(allText(e), QByteArray("HELLo WORLD"));
    delete e;
}

void CaseConvertTest::properCaseBlend_allUpper()
{
    SciEdit *e = createEditor();
    e->setText("HELLO WORLD");
    selectAll(e);
    e->properCaseBlend();
    QCOMPARE(allText(e), QByteArray("HELLO WORLD"));
    delete e;
}

void CaseConvertTest::properCaseBlend_allLower()
{
    SciEdit *e = createEditor();
    e->setText("hello world");
    selectAll(e);
    e->properCaseBlend();
    QCOMPARE(allText(e), QByteArray("Hello World"));
    delete e;
}

void CaseConvertTest::properCaseBlend_contraction()
{
    SciEdit *e = createEditor();
    e->setText("don't stop");
    selectAll(e);
    e->properCaseBlend();
    QCOMPARE(allText(e), QByteArray("Don't Stop"));
    delete e;
}

// ============================================================
// Sentence case (force)
// ============================================================

void CaseConvertTest::sentenceCase_basic()
{
    SciEdit *e = createEditor();
    e->setText("this is a test.");
    selectAll(e);
    e->sentenceCase();
    QCOMPARE(allText(e), QByteArray("This is a test."));
    delete e;
}

void CaseConvertTest::sentenceCase_allUpper()
{
    SciEdit *e = createEditor();
    e->setText("THIS IS A TEST. AND MORE.");
    selectAll(e);
    e->sentenceCase();
    QCOMPARE(allText(e), QByteArray("This is a test. And more."));
    delete e;
}

void CaseConvertTest::sentenceCase_multipleSentences()
{
    SciEdit *e = createEditor();
    e->setText("hello world. how are you? fine!");
    selectAll(e);
    e->sentenceCase();
    QCOMPARE(allText(e), QByteArray("Hello world. How are you? Fine!"));
    delete e;
}

void CaseConvertTest::sentenceCase_abbreviation()
{
    SciEdit *e = createEditor();
    e->setText("e.g. this is an example.");
    selectAll(e);
    e->sentenceCase();
    QCOMPARE(allText(e), QByteArray("E.g. This is an example."));
    delete e;
}

void CaseConvertTest::sentenceCase_pronounI()
{
    SciEdit *e = createEditor();
    e->setText("am i here?");
    selectAll(e);
    e->sentenceCase();
    QCOMPARE(allText(e), QByteArray("Am I here?"));
    delete e;
}

void CaseConvertTest::sentenceCase_blankLineNewParagraph()
{
    SciEdit *e = createEditor();
    e->setText("first paragraph.\n\nnew paragraph here.");
    selectAll(e);
    e->sentenceCase();
    QCOMPARE(allText(e), QByteArray("First paragraph.\n\nNew paragraph here."));
    delete e;
}

void CaseConvertTest::sentenceCase_emptySelection()
{
    SciEdit *e = createEditor();
    e->setText("hello.");
    e->setEmptySelection(2);
    e->sentenceCase();
    QCOMPARE(allText(e), QByteArray("hello."));
    delete e;
}

// ============================================================
// Sentence case (blend)
// ============================================================

void CaseConvertTest::sentenceCaseBlend_basic()
{
    SciEdit *e = createEditor();
    e->setText("this IS a TEST.");
    selectAll(e);
    e->sentenceCaseBlend();
    QCOMPARE(allText(e), QByteArray("This IS a TEST."));
    delete e;
}

void CaseConvertTest::sentenceCaseBlend_preservesCase()
{
    SciEdit *e = createEditor();
    e->setText("hELLO. wORLD.");
    selectAll(e);
    e->sentenceCaseBlend();
    QCOMPARE(allText(e), QByteArray("HELLO. WORLD."));
    delete e;
}

// ============================================================
// Invert case
// ============================================================

void CaseConvertTest::invertCase_basic()
{
    SciEdit *e = createEditor();
    e->setText("Hello World");
    selectAll(e);
    e->invertCase();
    QCOMPARE(allText(e), QByteArray("hELLO wORLD"));
    delete e;
}

void CaseConvertTest::invertCase_allUpper()
{
    SciEdit *e = createEditor();
    e->setText("ABC");
    selectAll(e);
    e->invertCase();
    QCOMPARE(allText(e), QByteArray("abc"));
    delete e;
}

void CaseConvertTest::invertCase_allLower()
{
    SciEdit *e = createEditor();
    e->setText("xyz");
    selectAll(e);
    e->invertCase();
    QCOMPARE(allText(e), QByteArray("XYZ"));
    delete e;
}

void CaseConvertTest::invertCase_mixed()
{
    SciEdit *e = createEditor();
    e->setText("aBcDeF");
    selectAll(e);
    e->invertCase();
    QCOMPARE(allText(e), QByteArray("AbCdEf"));
    delete e;
}

void CaseConvertTest::invertCase_nonLetters()
{
    SciEdit *e = createEditor();
    e->setText("abc 123 def!");
    selectAll(e);
    e->invertCase();
    QCOMPARE(allText(e), QByteArray("ABC 123 DEF!"));
    delete e;
}

void CaseConvertTest::invertCase_emptySelection()
{
    SciEdit *e = createEditor();
    e->setText("Hello");
    e->setEmptySelection(2);
    e->invertCase();
    QCOMPARE(allText(e), QByteArray("Hello"));
    delete e;
}

// ============================================================
// Random case
// ============================================================

void CaseConvertTest::randomCase_lengthPreserved()
{
    SciEdit *e = createEditor();
    e->setText("Hello World 123!");
    const int origLen = e->textLength();
    selectAll(e);
    e->randomCase();
    QCOMPARE(e->textLength(), origLen);
    delete e;
}

void CaseConvertTest::randomCase_nonLettersUnchanged()
{
    SciEdit *e = createEditor();
    e->setText("a1 b2 c3!");
    selectAll(e);
    e->randomCase();
    QByteArray result = allText(e);
    QCOMPARE(result.size(), 9);
    QCOMPARE(result.at(1), '1');
    QCOMPARE(result.at(2), ' ');
    QCOMPARE(result.at(4), '2');
    QCOMPARE(result.at(5), ' ');
    QCOMPARE(result.at(7), '3');
    QCOMPARE(result.at(8), '!');
    delete e;
}

void CaseConvertTest::randomCase_lettersPresent()
{
    SciEdit *e = createEditor();
    e->setText("abc");
    selectAll(e);
    e->randomCase();
    QByteArray result = allText(e);
    QCOMPARE(result.size(), 3);
    for (int i = 0; i < 3; ++i) {
        QVERIFY2(QChar(result.at(i)).isUpper() || QChar(result.at(i)).isLower(),
                 "Each letter should be either upper or lower");
    }
    delete e;
}

// ============================================================
// Multi-selection
// ============================================================

void CaseConvertTest::properCase_multiSelection()
{
    SciEdit *e = createEditor();
    e->setText("hello world foo bar");
    // Select "hello" (0..5) and "foo" (12..15)
    e->setSelection(0, 5);
    e->addSelection(12, 15);
    e->properCase();
    QCOMPARE(allText(e), QByteArray("Hello world Foo bar"));
    delete e;
}

void CaseConvertTest::invertCase_multiSelection()
{
    SciEdit *e = createEditor();
    e->setText("Hello World");
    // Select "Hello" (0..5) and "World" (6..11)
    e->setSelection(0, 5);
    e->addSelection(6, 11);
    e->invertCase();
    QCOMPARE(allText(e), QByteArray("hELLO wORLD"));
    delete e;
}

QTEST_MAIN(CaseConvertTest)
#include "case_convert_test.moc"
