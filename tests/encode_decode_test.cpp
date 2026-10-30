/*
 * Tests for the built-in encode/decode tools (Base64, URL and HEX/ASCII).
 */
#include "sciedit.h"

#include <Scintilla.h>
#include <ILexer.h>
#include <Lexilla.h>

#include <QtTest/QtTest>

class EncodeDecodeTest : public QObject
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

    static void selectRange(SciEdit *e, int start, int end)
    {
        e->setSel(start, end);
    }

private slots:
    // --- Base64 encode ---
    void base64Encode_noPadding();
    void base64Encode_withPadding();
    void base64Encode_empty();
    void base64Encode_fullGroupNeedsNoPadding();
    void base64Encode_wrap();
    void base64Encode_byLine();

    // --- Base64 decode ---
    void base64Decode_lenient();
    void base64Decode_ignoresWhitespace();
    void base64Decode_strict_valid();
    void base64Decode_strict_invalid();
    void base64Decode_byLine();
    void base64Decode_emptySelection();

    // --- Base64 round-trip ---
    void base64_roundTrip();

    // --- URL encode ---
    void urlEncode_rfc1738_space();
    void urlEncode_rfc1738_keepsUnreserved();
    void urlEncode_extended_encodesExtra();
    void urlEncode_full_encodesEverything();
    void urlEncode_rfc1738_utf8();
    void urlEncode_byLineKeepsEol();

    // --- URL decode ---
    void urlDecode_basic();
    void urlDecode_lowercaseHex();
    void urlDecode_invalidHex();
    void urlDecode_truncatedEscape();

    // --- URL round-trip ---
    void url_roundTrip();

    // --- HEX encode ---
    void hexEncode_compact();
    void hexEncode_spaceSeparated();
    void hexEncode_empty();
    void hexEncode_utf8();

    // --- HEX decode ---
    void hexDecode_compact();
    void hexDecode_spaceSeparated();
    void hexDecode_lowercase();
    void hexDecode_ignoresSeparators();

    // --- HEX round-trip ---
    void hex_roundTrip();

    // --- multi-selection ---
    void base64Encode_multiSelection();
    void urlEncode_multiSelection();
    void hexEncode_multiSelection();
};

// ============================================================
// Base64 encode
// ============================================================

void EncodeDecodeTest::base64Encode_noPadding()
{
    SciEdit *e = createEditor();
    e->setText("Mary");
    selectAll(e);
    e->base64Encode(SciEdit::Base64Padding::NoPadding);
    QCOMPARE(allText(e), QByteArray("TWFyeQ"));
    delete e;
}

void EncodeDecodeTest::base64Encode_withPadding()
{
    SciEdit *e = createEditor();
    e->setText("Mary");
    selectAll(e);
    e->base64Encode(SciEdit::Base64Padding::WithPadding);
    QCOMPARE(allText(e), QByteArray("TWFyeQ=="));
    delete e;
}

void EncodeDecodeTest::base64Encode_empty()
{
    SciEdit *e = createEditor();
    e->setText("");
    selectAll(e);
    e->base64Encode(SciEdit::Base64Padding::WithPadding);
    QCOMPARE(allText(e), QByteArray(""));
    delete e;
}

void EncodeDecodeTest::base64Encode_fullGroupNeedsNoPadding()
{
    SciEdit *e = createEditor();
    e->setText("abc");
    selectAll(e);
    e->base64Encode(SciEdit::Base64Padding::NoPadding);
    QCOMPARE(allText(e), QByteArray("YWJj"));
    delete e;
}

void EncodeDecodeTest::base64Encode_wrap()
{
    SciEdit *e = createEditor();
    // 16 'A' bytes -> 24 base64 chars with padding
    e->setText("AAAAAAAAAAAAAAAA");
    selectAll(e);
    e->base64Encode(SciEdit::Base64Padding::WithPadding, false, 4);
    QCOMPARE(allText(e), QByteArray("QUFB\nQUFB\nQUFB\nQUFB\nQUFB\nQQ=="));
    delete e;
}

void EncodeDecodeTest::base64Encode_byLine()
{
    SciEdit *e = createEditor();
    e->setText("Mary\nhad");
    selectAll(e);
    e->base64Encode(SciEdit::Base64Padding::NoPadding, true);
    QCOMPARE(allText(e), QByteArray("TWFyeQ\naGFk"));
    delete e;
}

// ============================================================
// Base64 decode
// ============================================================

void EncodeDecodeTest::base64Decode_lenient()
{
    SciEdit *e = createEditor();
    e->setText("TWFyeQ==");
    selectAll(e);
    QVERIFY(e->base64Decode());
    QCOMPARE(allText(e), QByteArray("Mary"));
    delete e;
}

void EncodeDecodeTest::base64Decode_ignoresWhitespace()
{
    SciEdit *e = createEditor();
    e->setText("TWF yeQ==\n");
    selectAll(e);
    QVERIFY(e->base64Decode());
    QCOMPARE(allText(e), QByteArray("Mary"));
    delete e;
}

void EncodeDecodeTest::base64Decode_strict_valid()
{
    SciEdit *e = createEditor();
    e->setText("TWFyeQ==");
    selectAll(e);
    QVERIFY(e->base64Decode(true));
    QCOMPARE(allText(e), QByteArray("Mary"));
    delete e;
}

void EncodeDecodeTest::base64Decode_strict_invalid()
{
    SciEdit *e = createEditor();
    e->setText("#### not base64 ####");
    selectAll(e);
    QVERIFY(!e->base64Decode(true));
    // On failure the text is left untouched.
    QCOMPARE(allText(e), QByteArray("#### not base64 ####"));
    delete e;
}

void EncodeDecodeTest::base64Decode_byLine()
{
    SciEdit *e = createEditor();
    e->setText("TWFyeQ\naGFk");
    selectAll(e);
    QVERIFY(e->base64Decode(false, true));
    QCOMPARE(allText(e), QByteArray("Mary\nhad"));
    delete e;
}

void EncodeDecodeTest::base64Decode_emptySelection()
{
    SciEdit *e = createEditor();
    e->setText("TWFyeQ==");
    e->setEmptySelection(2);
    QVERIFY(e->base64Decode());
    QCOMPARE(allText(e), QByteArray("TWFyeQ=="));
    delete e;
}

// ============================================================
// Base64 round-trip
// ============================================================

void EncodeDecodeTest::base64_roundTrip()
{
    SciEdit *e = createEditor();
    const QByteArray original = "The quick brown fox jumps over the lazy dog.";
    e->setText(original.constData());
    selectAll(e);
    e->base64Encode(SciEdit::Base64Padding::WithPadding);
    QVERIFY(allText(e) != original);
    selectAll(e);
    QVERIFY(e->base64Decode());
    QCOMPARE(allText(e), original);
    delete e;
}

// ============================================================
// URL encode
// ============================================================

void EncodeDecodeTest::urlEncode_rfc1738_space()
{
    SciEdit *e = createEditor();
    e->setText("hello world");
    selectAll(e);
    e->urlEncode(SciEdit::UrlEncoding::Rfc1738);
    QCOMPARE(allText(e), QByteArray("hello%20world"));
    delete e;
}

void EncodeDecodeTest::urlEncode_rfc1738_keepsUnreserved()
{
    SciEdit *e = createEditor();
    // '.', '!', '*', '(', ')' and alphanumerics are NOT encoded under RFC1738.
    e->setText("a.b!*()");
    selectAll(e);
    e->urlEncode(SciEdit::UrlEncoding::Rfc1738);
    QCOMPARE(allText(e), QByteArray("a.b!*()"));
    delete e;
}

void EncodeDecodeTest::urlEncode_extended_encodesExtra()
{
    SciEdit *e = createEditor();
    e->setText("a!b");
    selectAll(e);
    e->urlEncode(SciEdit::UrlEncoding::Extended);
    QCOMPARE(allText(e), QByteArray("a%21b"));
    delete e;
}

void EncodeDecodeTest::urlEncode_full_encodesEverything()
{
    SciEdit *e = createEditor();
    e->setText("ab");
    selectAll(e);
    e->urlEncode(SciEdit::UrlEncoding::Full);
    QCOMPARE(allText(e), QByteArray("%61%62"));
    delete e;
}

void EncodeDecodeTest::urlEncode_rfc1738_utf8()
{
    SciEdit *e = createEditor();
    // 'é' is U+00E9 -> UTF-8 bytes C3 A9, both non-printable -> encoded.
    e->setText("\xC3\xA9");
    selectAll(e);
    e->urlEncode(SciEdit::UrlEncoding::Rfc1738);
    QCOMPARE(allText(e), QByteArray("%C3%A9"));
    delete e;
}

void EncodeDecodeTest::urlEncode_byLineKeepsEol()
{
    SciEdit *e = createEditor();
    e->setText("ab\ncd");
    selectAll(e);
    e->urlEncode(SciEdit::UrlEncoding::Full, true);
    QCOMPARE(allText(e), QByteArray("%61%62\n%63%64"));
    delete e;
}

// ============================================================
// URL decode
// ============================================================

void EncodeDecodeTest::urlDecode_basic()
{
    SciEdit *e = createEditor();
    e->setText("hello%20world");
    selectAll(e);
    QVERIFY(e->urlDecode());
    QCOMPARE(allText(e), QByteArray("hello world"));
    delete e;
}

void EncodeDecodeTest::urlDecode_lowercaseHex()
{
    SciEdit *e = createEditor();
    e->setText("a%2fb");
    selectAll(e);
    QVERIFY(e->urlDecode());
    QCOMPARE(allText(e), QByteArray("a/b"));
    delete e;
}

void EncodeDecodeTest::urlDecode_invalidHex()
{
    SciEdit *e = createEditor();
    e->setText("a%ZZb");
    selectAll(e);
    QVERIFY(!e->urlDecode());
    QCOMPARE(allText(e), QByteArray("a%ZZb"));
    delete e;
}

void EncodeDecodeTest::urlDecode_truncatedEscape()
{
    SciEdit *e = createEditor();
    e->setText("100%");
    selectAll(e);
    QVERIFY(!e->urlDecode());
    QCOMPARE(allText(e), QByteArray("100%"));
    delete e;
}

// ============================================================
// URL round-trip
// ============================================================

void EncodeDecodeTest::url_roundTrip()
{
    SciEdit *e = createEditor();
    const QByteArray original = "https://example.com/a b?c=d&e=f";
    e->setText(original.constData());
    selectAll(e);
    e->urlEncode(SciEdit::UrlEncoding::Rfc1738);
    QVERIFY(allText(e) != original);
    selectAll(e);
    QVERIFY(e->urlDecode());
    QCOMPARE(allText(e), original);
    delete e;
}

// ============================================================
// HEX encode
// ============================================================

void EncodeDecodeTest::hexEncode_compact()
{
    SciEdit *e = createEditor();
    e->setText("ABC");
    selectAll(e);
    e->hexEncode(false);
    QCOMPARE(allText(e), QByteArray("414243"));
    delete e;
}

void EncodeDecodeTest::hexEncode_spaceSeparated()
{
    SciEdit *e = createEditor();
    e->setText("ABC");
    selectAll(e);
    e->hexEncode(true);
    QCOMPARE(allText(e), QByteArray("41 42 43"));
    delete e;
}

void EncodeDecodeTest::hexEncode_empty()
{
    SciEdit *e = createEditor();
    e->setText("");
    selectAll(e);
    e->hexEncode(false);
    QCOMPARE(allText(e), QByteArray(""));
    delete e;
}

void EncodeDecodeTest::hexEncode_utf8()
{
    SciEdit *e = createEditor();
    // 'é' (U+00E9) -> UTF-8 bytes C3 A9 -> lowercase hex "c3a9".
    e->setText("\xC3\xA9");
    selectAll(e);
    e->hexEncode(false);
    QCOMPARE(allText(e), QByteArray("c3a9"));
    delete e;
}

// ============================================================
// HEX decode
// ============================================================

void EncodeDecodeTest::hexDecode_compact()
{
    SciEdit *e = createEditor();
    e->setText("414243");
    selectAll(e);
    e->hexDecode();
    QCOMPARE(allText(e), QByteArray("ABC"));
    delete e;
}

void EncodeDecodeTest::hexDecode_spaceSeparated()
{
    SciEdit *e = createEditor();
    e->setText("41 42 43");
    selectAll(e);
    e->hexDecode();
    QCOMPARE(allText(e), QByteArray("ABC"));
    delete e;
}

void EncodeDecodeTest::hexDecode_lowercase()
{
    SciEdit *e = createEditor();
    e->setText("616263");
    selectAll(e);
    e->hexDecode();
    QCOMPARE(allText(e), QByteArray("abc"));
    delete e;
}

void EncodeDecodeTest::hexDecode_ignoresSeparators()
{
    SciEdit *e = createEditor();
    // fromHex skips any non-hex character, so colons/dashes are tolerated.
    e->setText("41:42-43");
    selectAll(e);
    e->hexDecode();
    QCOMPARE(allText(e), QByteArray("ABC"));
    delete e;
}

// ============================================================
// HEX round-trip
// ============================================================

void EncodeDecodeTest::hex_roundTrip()
{
    SciEdit *e = createEditor();
    const QByteArray original = "Hex conversion 123!";
    e->setText(original.constData());
    selectAll(e);
    e->hexEncode(true);
    QVERIFY(allText(e) != original);
    selectAll(e);
    e->hexDecode();
    QCOMPARE(allText(e), original);
    delete e;
}

// ============================================================
// multi-selection
// ============================================================

void EncodeDecodeTest::base64Encode_multiSelection()
{
    SciEdit *e = createEditor();
    e->setText("Mary had");
    // Select "Mary" (0..4) and "had" (5..8)
    selectRange(e, 0, 4);
    e->addSelection(5, 8);
    e->base64Encode(SciEdit::Base64Padding::WithPadding);
    QCOMPARE(allText(e), QByteArray("TWFyeQ== aGFk"));
    delete e;
}

void EncodeDecodeTest::urlEncode_multiSelection()
{
    SciEdit *e = createEditor();
    e->setText("a b c d");
    // Select the two spaces at positions 1 and 3
    selectRange(e, 1, 2);
    e->addSelection(3, 4);
    e->urlEncode(SciEdit::UrlEncoding::Rfc1738);
    QCOMPARE(allText(e), QByteArray("a%20b%20c d"));
    delete e;
}

void EncodeDecodeTest::hexEncode_multiSelection()
{
    SciEdit *e = createEditor();
    e->setText("A B");
    // Select 'A' (0..1) and 'B' (2..3)
    selectRange(e, 0, 1);
    e->addSelection(2, 3);
    e->hexEncode(false);
    QCOMPARE(allText(e), QByteArray("41 42"));
    delete e;
}

QTEST_MAIN(EncodeDecodeTest)
#include "encode_decode_test.moc"
