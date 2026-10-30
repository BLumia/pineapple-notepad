/*
 * Tests for comment toggle functionality (line comment & stream comment).
 */
#include "sciedit.h"

#include <Scintilla.h>
#include <ILexer.h>
#include <Lexilla.h>

#include <QtTest/QtTest>

class CommentTest : public QObject
{
    Q_OBJECT

private:
    static SciEdit *createCppEditor()
    {
        auto *edit = new SciEdit;
        edit->setLexer(CreateLexer("cpp"), QStringLiteral("cpp"));
        return edit;
    }

    static QByteArray allText(SciEdit *e)
    {
        return e->getText(e->textLength());
    }

private slots:
    void streamComment_addToSelection();
    void streamComment_addPreservesSelection();
    void streamComment_removeSelf();
    void streamComment_removeEnclosed();
    void streamComment_ignoreDocCommentAbove();
    void streamComment_addThenRemoveRoundTrip();
    void lineComment_addPrefix();
    void lineComment_removePrefix();
    void lineComment_preservesSelection();
    void lineComment_indentedLine_preservesSelectionStart();
    void lineComment_skipsEmptyLines();
    void smartToggle_noSelection_usesLineComment();
    void smartToggle_fullLineSelection_usesLineComment();
    void smartToggle_partialSelection_usesStreamComment();
};

void CommentTest::streamComment_addToSelection()
{
    SciEdit *edit = createCppEditor();
    edit->setText("hello world");
    edit->setSel(6, 11);

    edit->toggleStreamComment();

    QCOMPARE(allText(edit), QByteArray("hello /* world */"));
    QCOMPARE(edit->selectionStart(), sptr_t(6));
    QCOMPARE(edit->selectionEnd(), sptr_t(17));
    delete edit;
}

void CommentTest::streamComment_addPreservesSelection()
{
    SciEdit *edit = createCppEditor();
    edit->setText("hello world");
    edit->setSel(0, 5);

    edit->toggleStreamComment();

    QCOMPARE(edit->selectionStart(), sptr_t(0));
    QCOMPARE(edit->selectionEnd(), sptr_t(11));
    delete edit;
}

void CommentTest::streamComment_removeSelf()
{
    SciEdit *edit = createCppEditor();
    edit->setText("hello /*world*/");
    edit->setSel(6, 15);

    edit->toggleStreamComment();

    QCOMPARE(allText(edit), QByteArray("hello world"));
    QCOMPARE(edit->selectionStart(), sptr_t(6));
    QCOMPARE(edit->selectionEnd(), sptr_t(11));
    delete edit;
}

void CommentTest::streamComment_removeEnclosed()
{
    SciEdit *edit = createCppEditor();
    edit->setText("hello /*world*/");
    edit->setSel(8, 13);

    edit->toggleStreamComment();

    QCOMPARE(allText(edit), QByteArray("hello world"));
    QCOMPARE(edit->selectionStart(), sptr_t(6));
    QCOMPARE(edit->selectionEnd(), sptr_t(11));
    delete edit;
}

void CommentTest::streamComment_ignoreDocCommentAbove()
{
    const QByteArray code =
        "/*!\n"
        " * \\sa name()\n"
        " */\n"
        "int name();\n";

    SciEdit *edit = createCppEditor();
    edit->setText(code.constData());

    // Select "name()" on the "int name();" line (second occurrence)
    sptr_t nameStart = code.indexOf("name()", code.indexOf("int "));
    edit->setSel(nameStart, nameStart + 6);

    edit->toggleStreamComment();

    QByteArray result = allText(edit);
    QVERIFY2(result.contains("/*!"), "Doc comment /*! should be preserved");
    QVERIFY2(result.contains("/* name() */"), "name() should be wrapped in stream comment");
    delete edit;
}

void CommentTest::streamComment_addThenRemoveRoundTrip()
{
    SciEdit *edit = createCppEditor();
    edit->setText("int translatedName();");
    // "int " = 4 chars, "translatedName" = 14 chars → setSel(4, 18)
    edit->setSel(4, 18);

    // First toggle: add comment → "int /* translatedName */();"
    edit->toggleStreamComment();
    QCOMPARE(allText(edit), QByteArray("int /* translatedName */();"));

    // "int " = 4, "/* " = 3, "translatedName" = 14, " */" = 3
    // → /* translatedName */ is at positions 4..24
    edit->setSel(4, 24);

    // Second toggle: remove comment → "int translatedName();"
    edit->toggleStreamComment();
    QCOMPARE(allText(edit), QByteArray("int translatedName();"));
    delete edit;
}

void CommentTest::lineComment_addPrefix()
{
    SciEdit *edit = createCppEditor();
    edit->setText("line1\nline2\nline3");
    edit->setSel(0, 17);

    edit->toggleLineComment();

    QCOMPARE(allText(edit), QByteArray("// line1\n// line2\n// line3"));
    delete edit;
}

void CommentTest::lineComment_removePrefix()
{
    SciEdit *edit = createCppEditor();
    edit->setText("// line1\n// line2\n// line3");
    edit->setSel(0, 26);

    edit->toggleLineComment();

    QCOMPARE(allText(edit), QByteArray("line1\nline2\nline3"));
    delete edit;
}

void CommentTest::lineComment_preservesSelection()
{
    SciEdit *edit = createCppEditor();
    edit->setText("line1\nline2\nline3");
    edit->setSel(0, 17);

    edit->toggleLineComment();

    // "// " is 3 chars per line, 3 lines → selStart shifts by 3, selEnd by 9
    QCOMPARE(edit->selectionStart(), sptr_t(3));
    QCOMPARE(edit->selectionEnd(), sptr_t(26));
    delete edit;
}

void CommentTest::lineComment_indentedLine_preservesSelectionStart()
{
    SciEdit *edit = createCppEditor();
    edit->setText("    test");
    edit->setSel(0, 8);

    edit->toggleLineComment();

    QCOMPARE(allText(edit), QByteArray("    // test"));
    QCOMPARE(edit->selectionStart(), sptr_t(0));
    QCOMPARE(edit->selectionEnd(), sptr_t(11));
    delete edit;
}

void CommentTest::lineComment_skipsEmptyLines()
{
    SciEdit *edit = createCppEditor();
    edit->setText("line1\n\nline3");
    edit->setSel(0, 12);

    edit->toggleLineComment();

    QCOMPARE(allText(edit), QByteArray("// line1\n\n// line3"));
    delete edit;
}

void CommentTest::smartToggle_noSelection_usesLineComment()
{
    SciEdit *edit = createCppEditor();
    edit->setText("hello");
    edit->setEmptySelection(2);

    edit->toggleComment();

    QCOMPARE(allText(edit), QByteArray("// hello"));
    delete edit;
}

void CommentTest::smartToggle_fullLineSelection_usesLineComment()
{
    SciEdit *edit = createCppEditor();
    edit->setText("hello\nworld");
    edit->setSel(0, 6);

    edit->toggleComment();

    QCOMPARE(allText(edit), QByteArray("// hello\nworld"));
    delete edit;
}

void CommentTest::smartToggle_partialSelection_usesStreamComment()
{
    SciEdit *edit = createCppEditor();
    edit->setText("hello world");
    edit->setSel(6, 11);

    edit->toggleComment();

    QCOMPARE(allText(edit), QByteArray("hello /* world */"));
    delete edit;
}

QTEST_MAIN(CommentTest)
#include "comment_test.moc"
