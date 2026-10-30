/*
 * Tests for RTF/HTML document exporters.
 */
#include "exporters.h"
#include "sciedit.h"

#include <Scintilla.h>
#include <SciLexer.h>
#include <ILexer.h>
#include <Lexilla.h>

#include <QByteArray>
#include <QTextStream>

#include <QtTest/QtTest>

using namespace Qt::Literals::StringLiterals;

class ExportersTest : public QObject
{
    Q_OBJECT

private:
    static SciEdit *createPlainEditor()
    {
        return new SciEdit; // no lexer: a single style covers the whole document
    }

    static SciEdit *createCppEditor()
    {
        auto *edit = new SciEdit;
        edit->setLexer(CreateLexer("cpp"), QStringLiteral("cpp"));
        // Give the keyword style a distinctive color so that style switches
        // appear in the generated output.
        edit->styleSetFore(SCE_C_WORD, 0x0000FF); // Scintilla colors are 0x00BBGGRR -> red
        edit->styleSetBold(SCE_C_WORD, 1);
        return edit;
    }

    static QByteArray exportAll(Exporter *exporter)
    {
        QByteArray buffer;
        QTextStream stream(&buffer);
        exporter->convert(stream);
        stream.flush();
        return buffer;
    }

    static int count(const QByteArray &haystack, const QByteArray &needle)
    {
        int result = 0;
        int pos = haystack.indexOf(needle);
        while (pos >= 0) {
            ++result;
            pos = haystack.indexOf(needle, pos + needle.size());
        }
        return result;
    }

    static int braceBalance(const QByteArray &rtf)
    {
        // Rough structural check: strip escaped braces first.
        QByteArray cleaned = rtf;
        cleaned.replace("\\{", "");
        cleaned.replace("\\}", "");
        int balance = 0;
        for (char c : cleaned) {
            if (c == '{') {
                ++balance;
            } else if (c == '}') {
                --balance;
            }
        }
        return balance;
    }

private slots:
    void rtf_structure();
    void rtf_escapesBracesAndBackslash();
    void rtf_newlines();
    void rtf_tab();
    void rtf_unicodeEscapes();
    void rtf_astralCharacter();
    void rtf_styleSwitch();
    void rtf_emptyDocument();

    void html_structure();
    void html_escapesEntities();
    void html_newlines();
    void html_preservesUnicode();
    void html_escapedTitle();
    void html_styleClass();
    void html_convertRange();
    void html_emptyDocument();
};

void ExportersTest::rtf_structure()
{
    SciEdit *edit = createCppEditor();
    edit->setText("hello world");

    RtfExporter rtfExporter(edit);
    const QByteArray rtf = exportAll(&rtfExporter);

    QVERIFY(rtf.startsWith("{\\rtf1\\ansi"));
    QVERIFY(rtf.contains("{\\fonttbl"));
    QVERIFY(rtf.contains("{\\colortbl"));
    QVERIFY(rtf.endsWith("}\r\n"));
    QCOMPARE(braceBalance(rtf), 0);
    QVERIFY(rtf.contains("hello world"));
    delete edit;
}

void ExportersTest::rtf_escapesBracesAndBackslash()
{
    SciEdit *edit = createCppEditor();
    edit->setText("a{b}c\\d");

    RtfExporter rtfExporter(edit);
    const QByteArray rtf = exportAll(&rtfExporter);

    QVERIFY(rtf.contains("a\\{b\\}c\\\\d"));
    QCOMPARE(braceBalance(rtf), 0);
    delete edit;
}

void ExportersTest::rtf_newlines()
{
    SciEdit *edit = createCppEditor();
    edit->setText("a\r\nb\nc\rd");

    RtfExporter rtfExporter(edit);
    const QByteArray rtf = exportAll(&rtfExporter);

    // CRLF counts as a single break: CRLF + LF + CR = 3 paragraphs.
    QCOMPARE(count(rtf, "\\par"), 3);
    delete edit;
}

void ExportersTest::rtf_tab()
{
    SciEdit *edit = createCppEditor();
    edit->setText("a\tb");

    RtfExporter rtfExporter(edit);
    const QByteArray rtf = exportAll(&rtfExporter);

    QVERIFY(rtf.contains("a\\tab b"));
    delete edit;
}

void ExportersTest::rtf_unicodeEscapes()
{
    SciEdit *edit = createCppEditor();
    edit->setText("héllo 中");

    RtfExporter rtfExporter(edit);
    const QByteArray rtf = exportAll(&rtfExporter);

    QVERIFY(rtf.contains("\\u233?")); // é
    QVERIFY(rtf.contains("\\u20013?")); // 中
    delete edit;
}

void ExportersTest::rtf_astralCharacter()
{
    SciEdit *edit = createCppEditor();
    const QString pineapple = QString(QChar(0xD83C)) + QString(QChar(0xDF4D)); // U+1F34D pineapple
    edit->setText(pineapple.toUtf8().constData());

    RtfExporter rtfExporter(edit);
    const QByteArray rtf = exportAll(&rtfExporter);

    // Surrogate pair written as two signed 16-bit \u escapes.
    QVERIFY(rtf.contains("\\u-10180?\\u-8371?"));
    delete edit;
}

void ExportersTest::rtf_styleSwitch()
{
    SciEdit *edit = createCppEditor();
    edit->setText("int x;");

    RtfExporter rtfExporter(edit);
    const QByteArray rtf = exportAll(&rtfExporter);

    // The keyword style color was registered in the color table...
    QVERIFY(rtf.contains("\\red255\\green0\\blue0;"));
    // ...and switched to for the keyword run, with bold turned on.
    QVERIFY(rtf.contains("\\cf"));
    QVERIFY(rtf.contains("\\b "));
    delete edit;
}

void ExportersTest::rtf_emptyDocument()
{
    SciEdit *edit = createCppEditor();

    RtfExporter rtfExporter(edit);
    const QByteArray rtf = exportAll(&rtfExporter);

    QVERIFY(rtf.startsWith("{\\rtf1\\ansi"));
    QCOMPARE(braceBalance(rtf), 0);
    delete edit;
}

void ExportersTest::html_structure()
{
    SciEdit *edit = createPlainEditor();
    edit->setText("hello world");

    HtmlExporter htmlExporter(edit, QStringLiteral("test.txt"));
    const QByteArray html = exportAll(&htmlExporter);

    QVERIFY(html.startsWith("<!DOCTYPE html>"));
    QVERIFY(html.contains("<meta charset=\"utf-8\">"));
    QVERIFY(html.contains("<title>test.txt</title>"));
    QVERIFY(html.contains("<pre class=\"source\">"));
    QVERIFY(html.contains("hello world"));
    QVERIFY(html.contains("</pre>"));
    QVERIFY(html.endsWith("</html>\r\n"));
    delete edit;
}

void ExportersTest::html_escapesEntities()
{
    SciEdit *edit = createPlainEditor();
    edit->setText("<a> & \"b\"");

    HtmlExporter htmlExporter(edit);
    const QByteArray html = exportAll(&htmlExporter);

    QVERIFY(html.contains("&lt;a&gt; &amp; \"b\""));
    delete edit;
}

void ExportersTest::html_newlines()
{
    SciEdit *edit = createCppEditor();
    edit->setText("a\r\nb\nc");

    QByteArray buffer;
    QTextStream stream(&buffer);
    HtmlExporter(edit).convert(stream);
    stream.flush();

    const int start = buffer.indexOf("<pre class=\"source\">");
    const int end = buffer.indexOf("</pre>");
    QVERIFY(start >= 0 && end > start);
    const QByteArray body = buffer.mid(start, end - start);
    QCOMPARE(count(body, "\n"), 2);
    delete edit;
}

void ExportersTest::html_preservesUnicode()
{
    SciEdit *edit = createPlainEditor();
    edit->setText("héllo 中");

    HtmlExporter htmlExporter(edit);
    const QByteArray html = exportAll(&htmlExporter);

    QVERIFY(html.contains("héllo 中")); // UTF-8 characters are kept as-is
    delete edit;
}

void ExportersTest::html_escapedTitle()
{
    SciEdit *edit = createCppEditor();
    edit->setText("x");

    HtmlExporter htmlExporter(edit, QStringLiteral("a<b>&c"));
    const QByteArray html = exportAll(&htmlExporter);

    QVERIFY(html.contains("<title>a&lt;b&gt;&amp;c</title>"));
    delete edit;
}

void ExportersTest::html_styleClass()
{
    SciEdit *edit = createCppEditor();
    edit->setText("int x;");

    HtmlExporter htmlExporter(edit);
    const QByteArray html = exportAll(&htmlExporter);

    QVERIFY(html.contains("<span class=\"s"));   // style runs become spans
    QVERIFY(html.contains(".s"));                // ...with CSS classes
    QVERIFY(html.contains("color: rgb(255, 0, 0)"));
    QVERIFY(html.contains("font-weight: bold"));
    delete edit;
}

void ExportersTest::html_convertRange()
{
    SciEdit *edit = createCppEditor();
    edit->setText("hello int world");

    QByteArray buffer;
    QTextStream stream(&buffer);
    HtmlExporter(edit).convertRange(stream, 6, 9);
    stream.flush();

    QVERIFY(buffer.contains("int"));
    QVERIFY(!buffer.contains("world"));
    delete edit;
}

void ExportersTest::html_emptyDocument()
{
    SciEdit *edit = createCppEditor();

    HtmlExporter htmlExporter(edit, QStringLiteral("empty"));
    const QByteArray html = exportAll(&htmlExporter);

    QVERIFY(html.startsWith("<!DOCTYPE html>"));
    QVERIFY(html.contains("<pre class=\"source\"></pre>"));
    delete edit;
}

QTEST_MAIN(ExportersTest)
#include "exporters_test.moc"
