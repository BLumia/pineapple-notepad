/*
 * Tests for mark (indicator) and bookmark line functionality.
 */
#include "sciedit.h"

#include <Scintilla.h>
#include <ILexer.h>
#include <Lexilla.h>

#include <QClipboard>
#include <QGuiApplication>
#include <QThread>
#include <QtTest/QtTest>

class MarkTest : public QObject
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

    static QStringList textsOf(int styleMask, SciEdit *e)
    {
        QStringList out;
        const auto marked = e->markedTexts(styleMask);
        for (const auto &t : marked) {
            out.append(t.second);
        }
        return out;
    }

    static bool setClipboardTextRobustly(const QString &text)
    {
        for (int attempt = 0; attempt < 30; ++attempt) {
            QGuiApplication::clipboard()->setText(text);
            QCoreApplication::processEvents();
            if (QGuiApplication::clipboard()->text() == text) {
                return true;
            }
            QThread::msleep(100);
        }
        return false;
    }

    static QString clipboardTextRobustly()
    {
        for (int attempt = 0; attempt < 30; ++attempt) {
            const QString text = QGuiApplication::clipboard()->text();
            if (!text.isEmpty()) {
                return text;
            }
            QCoreApplication::processEvents();
            QThread::msleep(100);
        }
        return QGuiApplication::clipboard()->text();
    }

    static bool isClipboardUsable()
    {
        return setClipboardTextRobustly(QStringLiteral("__clipboard_probe__"));
    }

private slots:
    void markAll_plain();
    void markAll_wholeWord();
    void markAll_bookmarkLine_idempotent();
    void markAll_rangeRestriction();
    void markAll_multipleStyles();
    void clearMarks_wholeDocument();
    void clearMarks_range();
    void clearAllMarkStyles();
    void styleTokenRange();
    void joinMarkedTexts_noEol();
    void joinMarkedTexts_withEol();
    void joinMarkedTexts_single();
    void joinMarkedTexts_empty();
    void goToNextMarked_wrapsAndJumpsOut();
    void goToPreviousMarked();
    void removeBookmarkedLines();
    void removeUnbookmarkedLines();
    void inverseBookmarks();
    void copyBookmarkedLines();
    void cutBookmarkedLines_singleUndo();
    void pasteToBookmarkedLines();
};

void MarkTest::markAll_plain()
{
    SciEdit *edit = createEditor();
    edit->setText("foo bar foo baz foo");

    const int count = edit->markAllInRange(0, "foo", 0, int(edit->textLength()), 0, false);

    QCOMPARE(count, 3);
    QCOMPARE(textsOf(1 << 0, edit), QStringList({"foo", "foo", "foo"}));
    delete edit;
}

void MarkTest::markAll_wholeWord()
{
    SciEdit *edit = createEditor();
    edit->setText("foo foobar foo");

    const int count = edit->markAllInRange(SCFIND_WHOLEWORD, "foo", 0, int(edit->textLength()), 0, false);

    QCOMPARE(count, 2);
    delete edit;
}

void MarkTest::markAll_bookmarkLine_idempotent()
{
    SciEdit *edit = createEditor();
    edit->setText("alpha\nbeta\nalpha\ngamma\nalpha");

    const int count = edit->markAllInRange(0, "alpha", 0, int(edit->textLength()), 0, true);

    QCOMPARE(count, 3);
    QCOMPARE(edit->markerGet(0), sptr_t(1 << 1));
    QCOMPARE(edit->markerGet(1), sptr_t(0));
    QCOMPARE(edit->markerGet(2), sptr_t(1 << 1));
    QCOMPARE(edit->markerGet(3), sptr_t(0));
    QCOMPARE(edit->markerGet(4), sptr_t(1 << 1));

    // Marking again keeps exactly one bookmark per line.
    edit->markAllInRange(0, "alpha", 0, int(edit->textLength()), 0, true);
    QCOMPARE(edit->markerGet(0), sptr_t(1 << 1));
    delete edit;
}

void MarkTest::markAll_rangeRestriction()
{
    SciEdit *edit = createEditor();
    edit->setText("foo bar foo");

    // Restrict to the second "foo" only: positions 8..11.
    const int count = edit->markAllInRange(0, "foo", 8, 11, 0, false);

    QCOMPARE(count, 1);
    const auto marked = edit->markedTexts(1 << 0);
    QCOMPARE(marked.size(), 1);
    QCOMPARE(marked.first().first, 8);
    delete edit;
}

void MarkTest::markAll_multipleStyles()
{
    SciEdit *edit = createEditor();
    edit->setText("one two one two");

    edit->markAllInRange(0, "one", 0, int(edit->textLength()), 0, false);
    edit->markAllInRange(0, "two", 0, int(edit->textLength()), 1, false);

    // Style 3 alone.
    QCOMPARE(textsOf(1 << 3, edit), QStringList());
    QCOMPARE(textsOf(1 << 1, edit), QStringList({"two", "two"}));

    // Both styles merged and sorted by position.
    QCOMPARE(textsOf((1 << 0) | (1 << 1), edit),
             QStringList({"one", "two", "one", "two"}));
    delete edit;
}

void MarkTest::clearMarks_wholeDocument()
{
    SciEdit *edit = createEditor();
    edit->setText("foo bar foo");
    edit->markAllInRange(0, "foo", 0, int(edit->textLength()), 0, false);

    edit->clearMarks(0);

    QCOMPARE(edit->markedTexts(1 << 0).size(), 0);
    delete edit;
}

void MarkTest::clearMarks_range()
{
    SciEdit *edit = createEditor();
    edit->setText("foo bar foo");
    edit->markAllInRange(0, "foo", 0, int(edit->textLength()), 0, false);

    // Clear only the first occurrence.
    edit->clearMarks(0, 0, 3);

    const auto marked = edit->markedTexts(1 << 0);
    QCOMPARE(marked.size(), 1);
    QCOMPARE(marked.first().first, 8);
    delete edit;
}

void MarkTest::clearAllMarkStyles()
{
    SciEdit *edit = createEditor();
    edit->setText("foo bar");
    edit->markAllInRange(0, "foo", 0, int(edit->textLength()), 0, false);
    edit->markAllInRange(0, "bar", 0, int(edit->textLength()), 2, false);

    edit->clearAllMarkStyles();

    QCOMPARE(edit->markedTexts(SciEdit::allMarkStylesMask()).size(), 0);
    delete edit;
}

void MarkTest::styleTokenRange()
{
    SciEdit *edit = createEditor();
    edit->setText("hello world");

    edit->styleTokenRange(6, 5, 4);

    QCOMPARE(textsOf(1 << 4, edit), QStringList({"world"}));
    delete edit;
}

void MarkTest::joinMarkedTexts_noEol()
{
    QList<QPair<int, QString>> texts = {{0, QStringLiteral("abc")}, {10, QStringLiteral("def")}};
    QCOMPARE(SciEdit::joinMarkedTexts(texts), QStringLiteral("abc\r\ndef\r\n"));
}

void MarkTest::joinMarkedTexts_withEol()
{
    QList<QPair<int, QString>> texts = {{0, QStringLiteral("ab\ncd")}, {10, QStringLiteral("ef")}};
    QCOMPARE(SciEdit::joinMarkedTexts(texts), QStringLiteral("ab\ncd\r\n----\r\nef\r\n"));
}

void MarkTest::joinMarkedTexts_single()
{
    QList<QPair<int, QString>> texts = {{0, QStringLiteral("abc")}};
    QCOMPARE(SciEdit::joinMarkedTexts(texts), QStringLiteral("abc"));
}

void MarkTest::joinMarkedTexts_empty()
{
    QCOMPARE(SciEdit::joinMarkedTexts({}), QString());
}

void MarkTest::goToNextMarked_wrapsAndJumpsOut()
{
    SciEdit *edit = createEditor();
    edit->setText("aa bb aa bb aa");
    edit->markAllInRange(0, "aa", 0, int(edit->textLength()), 0, false);

    // Caret at the first segment's start counts as inside it: skip to the second.
    edit->setSel(0, 0);
    QVERIFY(edit->goToNextMarked(0, false));
    QCOMPARE(edit->selectionStart(), sptr_t(6));
    QCOMPARE(edit->selectionEnd(), sptr_t(8));

    // Caret inside the first segment: must jump out to the second one.
    edit->setSel(1, 1);
    QVERIFY(edit->goToNextMarked(0, false));
    QCOMPARE(edit->selectionStart(), sptr_t(6));

    // Caret at the last segment's start: none after it, wraps to the first one.
    edit->setSel(12, 12);
    QVERIFY(edit->goToNextMarked(0, true));
    QCOMPARE(edit->selectionStart(), sptr_t(0));

    // No wrap: fails.
    edit->setSel(12, 12);
    QVERIFY(!edit->goToNextMarked(0, false));
    delete edit;
}

void MarkTest::goToPreviousMarked()
{
    SciEdit *edit = createEditor();
    edit->setText("aa bb aa");
    edit->markAllInRange(0, "aa", 0, int(edit->textLength()), 0, false);

    // From the end of document: previous is the second "aa".
    edit->setSel(8, 8);
    QVERIFY(edit->goToPreviousMarked(0, false));
    QCOMPARE(edit->selectionStart(), sptr_t(6));

    // Caret inside the second segment: jumps out to the first one.
    edit->setSel(7, 7);
    QVERIFY(edit->goToPreviousMarked(0, false));
    QCOMPARE(edit->selectionStart(), sptr_t(0));

    // Caret inside the first segment: wraps to the last one.
    edit->setSel(1, 1);
    QVERIFY(edit->goToPreviousMarked(0, true));
    QCOMPARE(edit->selectionStart(), sptr_t(6));

    edit->setSel(1, 1);
    QVERIFY(!edit->goToPreviousMarked(0, false));
    delete edit;
}

void MarkTest::removeBookmarkedLines()
{
    SciEdit *edit = createEditor();
    edit->setText("keep1\ndrop\nkeep2\ndrop\nkeep3");
    edit->markerAdd(1, 1);
    edit->markerAdd(3, 1);

    edit->removeBookmarkedLines();

    QCOMPARE(allText(edit), QByteArray("keep1\nkeep2\nkeep3"));
    delete edit;
}

void MarkTest::removeUnbookmarkedLines()
{
    SciEdit *edit = createEditor();
    edit->setText("drop\nkeep\ndrop\nkeep\ndrop");
    edit->markerAdd(1, 1);
    edit->markerAdd(3, 1);

    edit->removeUnbookmarkedLines();

    // The last line has no EOL of its own, so the previous line's EOL remains.
    QCOMPARE(allText(edit), QByteArray("keep\nkeep\n"));
    delete edit;
}

void MarkTest::inverseBookmarks()
{
    SciEdit *edit = createEditor();
    edit->setText("a\nb\nc\nd\ne");
    edit->markerAdd(0, 1);
    edit->markerAdd(2, 1);

    edit->inverseBookmarks();

    QCOMPARE(edit->markerGet(0), sptr_t(0));
    QCOMPARE(edit->markerGet(1), sptr_t(1 << 1));
    QCOMPARE(edit->markerGet(2), sptr_t(0));
    QCOMPARE(edit->markerGet(3), sptr_t(1 << 1));
    QCOMPARE(edit->markerGet(4), sptr_t(1 << 1));
    delete edit;
}

void MarkTest::copyBookmarkedLines()
{
    if (!setClipboardTextRobustly(QStringLiteral("__sentinel__"))) {
        QSKIP("Clipboard unavailable in this environment");
    }
    SciEdit *edit = createEditor();
    edit->setText("one\ntwo\nthree\nfour");
    edit->markerAdd(1, 1);
    edit->markerAdd(3, 1);

    edit->copyBookmarkedLines();

    const QString clip = clipboardTextRobustly();
    if (clip == QLatin1String("__sentinel__")) {
        delete edit;
        QSKIP("Clipboard write failed in this environment");
    }
    QCOMPARE(clip, QStringLiteral("two\nfour"));
    QCOMPARE(allText(edit), QByteArray("one\ntwo\nthree\nfour"));
    delete edit;
}

void MarkTest::cutBookmarkedLines_singleUndo()
{
    if (!setClipboardTextRobustly(QStringLiteral("__sentinel__"))) {
        QSKIP("Clipboard unavailable in this environment");
    }
    SciEdit *edit = createEditor();
    edit->setText("one\ntwo\nthree\nfour");
    edit->markerAdd(1, 1);
    edit->markerAdd(3, 1);

    edit->cutBookmarkedLines();

    QCOMPARE(allText(edit), QByteArray("one\nthree\n"));
    const QString clip = clipboardTextRobustly();
    if (clip == QLatin1String("__sentinel__")) {
        delete edit;
        QSKIP("Clipboard write failed in this environment");
    }
    QCOMPARE(clip, QStringLiteral("two\nfour"));

    // A single undo restores everything.
    edit->undo();
    QCOMPARE(allText(edit), QByteArray("one\ntwo\nthree\nfour"));
    delete edit;
}

void MarkTest::pasteToBookmarkedLines()
{
    if (!isClipboardUsable()) {
        QSKIP("Clipboard unavailable in this environment");
    }
    SciEdit *edit = createEditor();
    edit->setText("one\ntwo\nthree");
    edit->markerAdd(0, 1);
    edit->markerAdd(2, 1);
    if (!setClipboardTextRobustly(QStringLiteral("X"))) {
        QSKIP("Clipboard unavailable in this environment");
    }

    edit->pasteToBookmarkedLines();

    QCOMPARE(allText(edit), QByteArray("X\ntwo\nX"));

    edit->undo();
    QCOMPARE(allText(edit), QByteArray("one\ntwo\nthree"));
    delete edit;
}

QTEST_MAIN(MarkTest)
#include "mark_test.moc"
