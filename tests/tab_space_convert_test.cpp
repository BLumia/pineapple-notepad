/*
 * Tests for TAB/Space conversion functionality (convertTabsToSpaces, leading-only variant).
 */
#include "sciedit.h"

#include <Scintilla.h>
#include <ILexer.h>
#include <Lexilla.h>

#include <QtTest/QtTest>

class TabSpaceConvertTest : public QObject
{
    Q_OBJECT

private:
    static SciEdit *createEditor(int tabWidth = 4)
    {
        auto *edit = new SciEdit;
        edit->setLexer(CreateLexer("cpp"), QStringLiteral("cpp"));
        edit->setTabWidth(tabWidth);
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
    // --- TAB to Space (all) ---
    void tabToSpaceAll_basic();
    void tabToSpaceAll_tabAtColumnBoundary();
    void tabToSpaceAll_noTabs();
    void tabToSpaceAll_multipleLines();
    void tabToSpaceAll_tabOnlyLine();
    void tabToSpaceAll_customTabWidth();
    void tabToSpaceAll_utf8ColumnCounting();
    void tabToSpaceAll_selectionOnlySelectedLines();
    void tabToSpaceAll_emptyDocument();

    // --- TAB to Space (leading) ---
    void tabToSpaceLeading_leadingConvertedMidPreserved();
    void tabToSpaceLeading_mixedWhitespacePrefix();
    void tabToSpaceLeading_noLeadingTab();
    void tabToSpaceLeading_multipleLinesIndependent();
    void tabToSpaceLeading_consecutiveLeadingTabs();
    void tabToSpaceLeading_tabOnlyLine();
    void tabToSpaceLeading_customTabWidth();
    void tabToSpaceLeading_leadingEndsAtNonWhitespace();
    void tabToSpaceLeading_selectionOnlySelectedLines();
    void tabToSpaceLeading_noLeadingTabsUnchanged();

    // --- document-wide mode ---
    void tabToSpaceAll_documentWideIgnoresSelection();
    void tabToSpaceAll_documentWideIgnoresRectangleSelection();
    void tabToSpaceLeading_documentWideIgnoresSelection();
    void spaceToTabAll_documentWideIgnoresSelection();
    void spaceToTabLeading_documentWideIgnoresSelection();
    void spaceToTabLeading_documentWideLeadingOnly();

    // --- selection/caret position preservation ---
    void spaceToTabLeading_documentWidePreservesSelection();
    void tabToSpaceLeading_documentWidePreservesSelection();
    void spaceToTabLeading_selectionModePreservesSelection();
    void tabToSpaceAll_documentWidePreservesCaret();
};

// ============================================================
// TAB to Space (all)
// ============================================================

void TabSpaceConvertTest::tabToSpaceAll_basic()
{
    SciEdit *e = createEditor();
    e->setText("\tfoo\tbar");
    selectAll(e);
    e->convertTabsToSpaces();
    QCOMPARE(allText(e), QByteArray("    foo bar"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceAll_tabAtColumnBoundary()
{
    SciEdit *e = createEditor();
    e->setText("a\tb");
    selectAll(e);
    e->convertTabsToSpaces();
    QCOMPARE(allText(e), QByteArray("a   b"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceAll_noTabs()
{
    SciEdit *e = createEditor();
    e->setText("foo bar");
    selectAll(e);
    e->convertTabsToSpaces();
    QCOMPARE(allText(e), QByteArray("foo bar"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceAll_multipleLines()
{
    SciEdit *e = createEditor();
    e->setText("\ta\nb\tc");
    selectAll(e);
    e->convertTabsToSpaces();
    QCOMPARE(allText(e), QByteArray("    a\nb   c"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceAll_tabOnlyLine()
{
    SciEdit *e = createEditor();
    e->setText("\t");
    selectAll(e);
    e->convertTabsToSpaces();
    QCOMPARE(allText(e), QByteArray("    "));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceAll_customTabWidth()
{
    SciEdit *e = createEditor(8);
    e->setText("\ta");
    selectAll(e);
    e->convertTabsToSpaces();
    QCOMPARE(allText(e), QByteArray("        a"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceAll_utf8ColumnCounting()
{
    SciEdit *e = createEditor();
    // "é" is 2 bytes in UTF-8; it counts as a single column.
    e->setText("\xC3\xA9\tb");
    selectAll(e);
    e->convertTabsToSpaces();
    QCOMPARE(allText(e), QByteArray("\xC3\xA9   b"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceAll_selectionOnlySelectedLines()
{
    SciEdit *e = createEditor();
    e->setText("aaa\n\tbbb\nccc");
    // Select only the middle line ("\tbbb", positions 4..8)
    e->setSel(4, 8);
    e->convertTabsToSpaces();
    QCOMPARE(allText(e), QByteArray("aaa\n    bbb\nccc"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceAll_emptyDocument()
{
    SciEdit *e = createEditor();
    e->setText("");
    e->convertTabsToSpaces();
    QCOMPARE(allText(e), QByteArray(""));
    delete e;
}

// ============================================================
// TAB to Space (leading)
// ============================================================

void TabSpaceConvertTest::tabToSpaceLeading_leadingConvertedMidPreserved()
{
    SciEdit *e = createEditor();
    e->setText("\tfoo\tbar");
    selectAll(e);
    e->convertTabsToSpaces(true);
    QCOMPARE(allText(e), QByteArray("    foo\tbar"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceLeading_mixedWhitespacePrefix()
{
    SciEdit *e = createEditor();
    // Tab at column 2 expands to 2 spaces to reach the next tab stop.
    e->setText("  \tfoo");
    selectAll(e);
    e->convertTabsToSpaces(true);
    QCOMPARE(allText(e), QByteArray("    foo"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceLeading_noLeadingTab()
{
    SciEdit *e = createEditor();
    e->setText("foo\tbar");
    selectAll(e);
    e->convertTabsToSpaces(true);
    QCOMPARE(allText(e), QByteArray("foo\tbar"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceLeading_multipleLinesIndependent()
{
    SciEdit *e = createEditor();
    e->setText("\ta\nfoo\tb\n\tc");
    selectAll(e);
    e->convertTabsToSpaces(true);
    QCOMPARE(allText(e), QByteArray("    a\nfoo\tb\n    c"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceLeading_consecutiveLeadingTabs()
{
    SciEdit *e = createEditor();
    e->setText("\t\tfoo");
    selectAll(e);
    e->convertTabsToSpaces(true);
    QCOMPARE(allText(e), QByteArray("        foo"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceLeading_tabOnlyLine()
{
    SciEdit *e = createEditor();
    e->setText("\t");
    selectAll(e);
    e->convertTabsToSpaces(true);
    QCOMPARE(allText(e), QByteArray("    "));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceLeading_customTabWidth()
{
    SciEdit *e = createEditor(8);
    e->setText("\ta");
    selectAll(e);
    e->convertTabsToSpaces(true);
    QCOMPARE(allText(e), QByteArray("        a"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceLeading_leadingEndsAtNonWhitespace()
{
    SciEdit *e = createEditor();
    // Leading whitespace run is "\t ": the tab expands to 4 spaces (columns 0..4),
    // the literal space remains (column 4..5). The tab after 'x' is not leading
    // and stays a tab.
    e->setText("\t x\t");
    selectAll(e);
    e->convertTabsToSpaces(true);
    QCOMPARE(allText(e), QByteArray("     x\t"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceLeading_selectionOnlySelectedLines()
{
    SciEdit *e = createEditor();
    e->setText("aaa\n\tb\tbb\nccc");
    // Select only the middle line (positions 4..9)
    e->setSel(4, 9);
    e->convertTabsToSpaces(true);
    QCOMPARE(allText(e), QByteArray("aaa\n    b\tbb\nccc"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceLeading_noLeadingTabsUnchanged()
{
    SciEdit *e = createEditor();
    e->setText("a\tb\n c\td");
    selectAll(e);
    e->convertTabsToSpaces(true);
    QCOMPARE(allText(e), QByteArray("a\tb\n c\td"));
    delete e;
}

// ============================================================
// document-wide mode
// ============================================================

void TabSpaceConvertTest::tabToSpaceAll_documentWideIgnoresSelection()
{
    SciEdit *e = createEditor();
    e->setText("\taaa\n\tbbb\nccc");
    // Select only the middle line; document-wide converts every line.
    e->setSel(5, 10);
    e->convertTabsToSpaces(false, true);
    QCOMPARE(allText(e), QByteArray("    aaa\n    bbb\nccc"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceAll_documentWideIgnoresRectangleSelection()
{
    SciEdit *e = createEditor();
    e->setText("\ta\n\tb");
    e->setSelectionMode(SC_SEL_RECTANGLE);
    e->setSel(1, 5);
    // Selection-only mode bails out on rectangle selections;
    // document-wide mode ignores the selection mode and converts all.
    e->convertTabsToSpaces(false, true);
    QCOMPARE(allText(e), QByteArray("    a\n    b"));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceLeading_documentWideIgnoresSelection()
{
    SciEdit *e = createEditor();
    e->setText("\taaa x\tb\n\tyyy");
    // Select only the second line; document-wide converts leading tabs on all lines
    // while preserving non-leading tabs.
    e->setSel(10, 13);
    e->convertTabsToSpaces(true, true);
    QCOMPARE(allText(e), QByteArray("    aaa x\tb\n    yyy"));
    delete e;
}

void TabSpaceConvertTest::spaceToTabAll_documentWideIgnoresSelection()
{
    SciEdit *e = createEditor();
    // Space runs that exactly reach the next tab stop are converted; the single
    // leading space before 'c' stays because it is followed by a non-space.
    e->setText("a   b\n c  d");
    // Select partially within the first line; document-wide converts every run.
    e->setSel(0, 3);
    e->convertSpacesToTabs(false, true);
    QCOMPARE(allText(e), QByteArray("a\tb\n c\td"));
    delete e;
}

void TabSpaceConvertTest::spaceToTabLeading_documentWideIgnoresSelection()
{
    SciEdit *e = createEditor();
    e->setText("    aaa\n  bbb\n    ccc");
    // Select within the last line; document-wide converts leading spaces on all lines.
    e->setSel(14, 17);
    e->convertSpacesToTabs(true, true);
    QCOMPARE(allText(e), QByteArray("\taaa\n  bbb\n\tccc"));
    delete e;
}

void TabSpaceConvertTest::spaceToTabLeading_documentWideLeadingOnly()
{
    SciEdit *e = createEditor();
    e->setText("    a b    c");
    // document-wide + leading-only: mid-line space runs are preserved.
    e->convertSpacesToTabs(true, true);
    QCOMPARE(allText(e), QByteArray("\ta b    c"));
    delete e;
}

// ============================================================
// selection/caret position preservation
// ============================================================

void TabSpaceConvertTest::spaceToTabLeading_documentWidePreservesSelection()
{
    SciEdit *e = createEditor();
    e->setText("    123456\n    abcdefghigkl");
    // Select from '1' (pos 4) to just after 'f' (pos 21).
    e->setSelection(21, 4);
    e->convertSpacesToTabs(true, true);
    QCOMPARE(allText(e), QByteArray("\t123456\n\tabcdefghigkl"));
    QCOMPARE(e->selectionStart(), static_cast<sptr_t>(1));
    QCOMPARE(e->selectionEnd(), static_cast<sptr_t>(15));
    QCOMPARE(e->anchor(), static_cast<sptr_t>(1));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceLeading_documentWidePreservesSelection()
{
    SciEdit *e = createEditor();
    e->setText("\t123456\n\tabcdefghigkl");
    // Select from '1' (pos 1) to just after 'f' (pos 15).
    e->setSelection(15, 1);
    e->convertTabsToSpaces(true, true);
    QCOMPARE(allText(e), QByteArray("    123456\n    abcdefghigkl"));
    QCOMPARE(e->selectionStart(), static_cast<sptr_t>(4));
    QCOMPARE(e->selectionEnd(), static_cast<sptr_t>(21));
    QCOMPARE(e->anchor(), static_cast<sptr_t>(4));
    delete e;
}

void TabSpaceConvertTest::spaceToTabLeading_selectionModePreservesSelection()
{
    SciEdit *e = createEditor();
    e->setText("    abc\n    def");
    // Select from 'b' (pos 5) to 'd' (pos 12).
    e->setSelection(12, 5);
    e->convertSpacesToTabs(true);
    QCOMPARE(allText(e), QByteArray("\tabc\n\tdef"));
    QCOMPARE(e->selectionStart(), static_cast<sptr_t>(2));
    QCOMPARE(e->selectionEnd(), static_cast<sptr_t>(6));
    QCOMPARE(e->anchor(), static_cast<sptr_t>(2));
    delete e;
}

void TabSpaceConvertTest::tabToSpaceAll_documentWidePreservesCaret()
{
    SciEdit *e = createEditor();
    e->setText("\tfoo\nbar");
    e->setEmptySelection(5);
    e->convertTabsToSpaces(false, true);
    QCOMPARE(allText(e), QByteArray("    foo\nbar"));
    QCOMPARE(e->currentPos(), static_cast<sptr_t>(8));
    delete e;
}

QTEST_MAIN(TabSpaceConvertTest)
#include "tab_space_convert_test.moc"
