#include "sciedit.h"
#include "ScintillaEdit.h"
#include "editorpopupconfig.h"
#include "languageconfigstore.h"
#include "spellcheckcontroller.h"
#include "textutil.h"
#include "themeconfigstore.h"

#include <SciLexer.h>
#include <array>

#include <QDesktopServices>
#include <QClipboard>
#include <QColor>
#include <QByteArray>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFile>
#include <QGuiApplication>
#include <QMimeData>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <QMenu>
#include <QAction>
#include <QLoggingCategory>
#include <QStringBuilder>
#include <KActionCollection>
#include <KXmlGuiWindow>
#include <KXMLGUIFactory>

#include <algorithm>
#include <random>

Q_LOGGING_CATEGORY(lcSciEdit, "pnotepad.sciedit")

static sptr_t toBgr(const QColor &color) {
    return (color.blue() << 16) | (color.green() << 8) | color.red();
}

// Fail-fast clipboard helpers. The underlying platform error (e.g. the COM
// error from OleSetClipboard when another process holds the clipboard open
// on Windows) is reported by Qt itself via qWarning; here we only record
// that the operation failed.
static bool setClipboardText(const QString &text)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);
    if (!clipboard->ownsClipboard()) {
        qCWarning(lcSciEdit) << "Failed to set clipboard text";
        return false;
    }
    return true;
}

static QString clipboardText()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    const QString text = clipboard->text();
    if (text.isEmpty()) {
        qCWarning(lcSciEdit) << "Clipboard text is empty or unavailable";
    }
    return text;
}

static constexpr int kUrlHoverIndicator = 20;
static constexpr int kSmartHighlightIndicator = 21;
static constexpr int kFindResultIndicator = 22;
static constexpr int kTagMatchIndicator = 23;
static constexpr int kMarkStyle1Indicator = 24;
static constexpr int kMarkStyle2Indicator = 25;
static constexpr int kMarkStyle3Indicator = 26;
static constexpr int kMarkStyle4Indicator = 27;
static constexpr int kMarkStyle5Indicator = 28;
static_assert(kUrlHoverIndicator == SciEdit::kUrlHoverIndicatorId);
static constexpr int kSpellCheckIndicator = SciEdit::kSpellCheckIndicatorId;
static constexpr int kBookmarkMargin = 1;
static constexpr int kBookmarkMarker = 1;
static constexpr int kHideLinesBeginMarker = 18;
static constexpr int kHideLinesEndMarker = 19;
static constexpr int kWarningMarker = 16;
static constexpr int kErrorMarker = 17;
static constexpr int kBookmarkMask = 1 << kBookmarkMarker;
static constexpr int kHideLinesMask = (1 << kHideLinesBeginMarker) | (1 << kHideLinesEndMarker);
static constexpr int kBookmarkMarginMask = kBookmarkMask | kHideLinesMask;
static constexpr int kFoldMargin = 2;
static constexpr int kChangeHistoryMargin = 3;

namespace {

struct NPCChar {
    const char* bytes;
    const char* repr;
};

constexpr std::array<NPCChar, 49> g_nonPrintingChars{{
    // Special Unicode characters not covered by Scintilla's default representations
    // Scintilla already handles C0 (0x00-0x1F), DEL (0x7F), C1 (0x80-0x9F), LS (U+2028), PS (U+2029)
    {"\xC2\xA0", "NBSP"},          // U+00A0 : no-break space
    {"\xC2\xAD", "SHY"},           // U+00AD : soft hyphen
    {"\xD8\x9C", "ALM"},           // U+061C : arabic letter mark
    {"\xDC\x8F", "SAM"},           // U+070F : syriac abbreviation mark
    {"\xE1\x9A\x80", "OSPM"},      // U+1680 : ogham space mark
    {"\xE1\xA0\x8E", "MVS"},       // U+180E : mongolian vowel separator
    {"\xE2\x80\x80", "NQSP"},      // U+2000 : en quad
    {"\xE2\x80\x81", "MQSP"},      // U+2001 : em quad
    {"\xE2\x80\x82", "ENSP"},      // U+2002 : en space
    {"\xE2\x80\x83", "EMSP"},      // U+2003 : em space
    {"\xE2\x80\x84", "3/MSP"},     // U+2004 : three-per-em space
    {"\xE2\x80\x85", "4/MSP"},     // U+2005 : four-per-em space
    {"\xE2\x80\x86", "6/MSP"},     // U+2006 : six-per-em space
    {"\xE2\x80\x87", "FSP"},       // U+2007 : figure space
    {"\xE2\x80\x88", "PSP"},       // U+2008 : punctuation space
    {"\xE2\x80\x89", "THSP"},      // U+2009 : thin space
    {"\xE2\x80\x8A", "HSP"},       // U+200A : hair space
    {"\xE2\x80\x8B", "ZWSP"},      // U+200B : zero-width space
    {"\xE2\x80\x8C", "ZWNJ"},      // U+200C : zero-width non-joiner
    {"\xE2\x80\x8D", "ZWJ"},       // U+200D : zero-width joiner
    {"\xE2\x80\x8E", "LRM"},       // U+200E : left-to-right mark
    {"\xE2\x80\x8F", "RLM"},       // U+200F : right-to-left mark
    {"\xE2\x80\xAA", "LRE"},       // U+202A : left-to-right embedding
    {"\xE2\x80\xAB", "RLE"},       // U+202B : right-to-left embedding
    {"\xE2\x80\xAC", "PDF"},       // U+202C : pop directional formatting
    {"\xE2\x80\xAD", "LRO"},       // U+202D : left-to-right override
    {"\xE2\x80\xAE", "RLO"},       // U+202E : right-to-left override
    {"\xE2\x80\xAF", "NNBSP"},     // U+202F : narrow no-break space
    {"\xE2\x81\x9F", "MMSP"},      // U+205F : medium mathematical space
    {"\xE2\x81\xA0", "WJ"},        // U+2060 : word joiner
    {"\xE2\x81\xA1", "(FA)"},      // U+2061 : function application
    {"\xE2\x81\xA2", "(IT)"},      // U+2062 : invisible times
    {"\xE2\x81\xA3", "(IS)"},      // U+2063 : invisible separator
    {"\xE2\x81\xA4", "(IP)"},      // U+2064 : invisible plus
    {"\xE2\x81\xA6", "LRI"},       // U+2066 : left-to-right isolate
    {"\xE2\x81\xA7", "RLI"},       // U+2067 : right-to-left isolate
    {"\xE2\x81\xA8", "FSI"},       // U+2068 : first strong isolate
    {"\xE2\x81\xA9", "PDI"},       // U+2069 : pop directional isolate
    {"\xE2\x81\xAA", "ISS"},       // U+206A : inhibit symmetric swapping
    {"\xE2\x81\xAB", "ASS"},       // U+206B : activate symmetric swapping
    {"\xE2\x81\xAC", "IAFS"},      // U+206C : inhibit arabic form shaping
    {"\xE2\x81\xAD", "AAFS"},      // U+206D : activate arabic form shaping
    {"\xE2\x81\xAE", "NADS"},      // U+206E : national digit shapes
    {"\xE2\x81\xAF", "NODS"},      // U+206F : nominal digit shapes
    {"\xE3\x80\x80", "IDSP"},      // U+3000 : ideographic space
    {"\xEF\xBB\xBF", "BOM"},       // U+FEFF : zero-width no-break space / BOM
    {"\xEF\xBF\xB9", "IAA"},       // U+FFF9 : interlinear annotation anchor
    {"\xEF\xBF\xBA", "IAS"},       // U+FFFA : interlinear annotation separator
    {"\xEF\xBF\xBB", "IAT"}        // U+FFFB : interlinear annotation terminator
}};

}


static bool isUrlSchemeStartChar(QChar c)
{
    return ((c >= QLatin1Char('A') && c <= QLatin1Char('Z'))
        || (c >= QLatin1Char('a') && c <= QLatin1Char('z')));
}

static bool isUrlSchemeDelimiter(QChar c)
{
    return !((c >= QLatin1Char('0') && c <= QLatin1Char('9'))
        || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'))
        || (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
        || (c == QLatin1Char('_')));
}

static bool isUrlTextChar(QChar c)
{
    if (c.unicode() <= QLatin1Char(' ').unicode()) {
        return false;
    }

    switch (c.unicode()) {
    case 0x0020: // SPACE
    case 0x00A0: // NO-BREAK SPACE
    case 0x2002: // EN SPACE
    case 0x2003: // EM SPACE
    case 0x3000: // IDEOGRAPHIC SPACE
    case 0x2004: // THREE-PER-EM SPACE
    case 0x2005: // FOUR-PER-EM SPACE
    case 0x2006: // SIX-PER-EM SPACE
    case 0x2007: // FIGURE SPACE
    case 0x2008: // PUNCTUATION SPACE
    case 0x2009: // THIN SPACE
    case 0x200A: // HAIR SPACE
    case 0x200B: // ZERO WIDTH SPACE
    case 0x202F: // NARROW NO-BREAK SPACE
    case 0x205F: // MEDIUM MATHEMATICAL SPACE
    case 0xFEFF: // ZERO WIDTH NO-BREAK SPACE / BOM
        return false;
    }

    switch (c.unicode()) {
    case '"':   // DQUOTE
    case '#':   // FRAGMENT START
    case '<':   // TAG OPEN
    case '>':   // TAG CLOSE
    case '{':   // BLOCK OPEN
    case '}':   // BLOCK CLOSE
    case '?':   // QUERY START
    case 0x007F: // DEL
        return false;
    }

    return true;
}

static bool isUrlQueryDelimiter(QChar c)
{
    switch (c.unicode()) {
    case '&':
    case '+':
    case '=':
    case ';':
        return true;
    default:
        return false;
    }
}

static bool isUrlSchemeSupported(const QVector<QChar> &text, int start, int remainingLength)
{
    static const QVector<QString> schemes = {
        QStringLiteral("ftp://"),
        QStringLiteral("http://"),
        QStringLiteral("https://"),
        QStringLiteral("mailto:"),
        QStringLiteral("file://")
    };

    for (const auto &scheme : schemes) {
        if (remainingLength < scheme.size()) {
            continue;
        }
        bool match = true;
        for (int i = 0; i < scheme.size(); ++i) {
            if (text.at(start + i).toLower() != scheme.at(i).toLower()) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

static bool scanToUrlStart(const QVector<QChar> &text, int textLen, int start, int *distance, int *schemeLength)
{
    int p = start;
    int p0 = 0;
    enum { sUnknown, sScheme } s = sUnknown;
    while (p < textLen) {
        switch (s) {
        case sUnknown:
            if (isUrlSchemeStartChar(text.at(p)) && ((p == 0) || isUrlSchemeDelimiter(text.at(p - 1)))) {
                p0 = p;
                s = sScheme;
            }
            break;

        case sScheme:
            if (text.at(p) == QLatin1Char(':') && isUrlSchemeSupported(text, p0, textLen - p0)) {
                *distance = p0 - start;
                *schemeLength = p - p0 + 1;
                return true;
            }
            if (!isUrlSchemeStartChar(text.at(p))) {
                s = sUnknown;
            }
            break;
        }
        p++;
    }

    *schemeLength = 0;
    *distance = p - start;
    return false;
}

static void scanToUrlEnd(const QVector<QChar> &text, int textLen, int start, int *distance)
{
    int p = start;
    QChar q = QChar();
    enum { sHostAndPath, sQuery, sQueryAfterDelimiter, sQueryQuotes, sQueryAfterQuotes, sFragment } s = sHostAndPath;
    while (p < textLen) {
        switch (s) {
        case sHostAndPath:
            if (text.at(p) == QLatin1Char('?')) {
                s = sQuery;
            } else if (text.at(p) == QLatin1Char('#')) {
                s = sFragment;
            } else if (!isUrlTextChar(text.at(p))) {
                *distance = p - start;
                return;
            }
            break;

        case sQuery:
            if (text.at(p) == QLatin1Char('#')) {
                s = sFragment;
            } else if (isUrlQueryDelimiter(text.at(p))) {
                s = sQueryAfterDelimiter;
            } else if (!isUrlTextChar(text.at(p))) {
                *distance = p - start;
                return;
            }
            break;

        case sQueryAfterDelimiter:
            if (text.at(p) == QLatin1Char('\'') || text.at(p) == QLatin1Char('"') || text.at(p) == QLatin1Char('`')) {
                q = text.at(p);
                s = sQueryQuotes;
            } else if (text.at(p) == QLatin1Char('(')) {
                q = QLatin1Char(')');
                s = sQueryQuotes;
            } else if (text.at(p) == QLatin1Char('[')) {
                q = QLatin1Char(']');
                s = sQueryQuotes;
            } else if (text.at(p) == QLatin1Char('{')) {
                q = QLatin1Char('}');
                s = sQueryQuotes;
            } else if (isUrlTextChar(text.at(p))) {
                s = sQuery;
            } else {
                *distance = p - start;
                return;
            }
            break;

        case sQueryQuotes:
            if (text.at(p) < QLatin1Char(' ')) {
                *distance = p - start;
                return;
            }
            if (text.at(p) == q) {
                s = sQueryAfterQuotes;
            }
            break;

        case sQueryAfterQuotes:
            if (isUrlQueryDelimiter(text.at(p))) {
                s = sQueryAfterDelimiter;
            } else {
                *distance = p - start;
                return;
            }
            break;

        case sFragment:
            if (text.at(p) != QLatin1Char('?') && !isUrlTextChar(text.at(p))) {
                *distance = p - start;
                return;
            }
            break;
        }
        p++;
    }
    *distance = p - start;
}

static void removeUnwantedTrailingCharFromEnclosedUrl(int start, const QVector<QChar> &text, int *length)
{
    if (start > 0 && text.at(start - 1) == QLatin1Char('\'') && text.at(start + *length - 1) == QLatin1Char('\'')) {
        *length -= 1;
    }
    if (start > 0 && text.at(start - 1) == QLatin1Char('`') && text.at(start + *length - 1) == QLatin1Char('`')) {
        *length -= 1;
    }
}

static bool removeUnwantedTrailingCharFromUrl(const QVector<QChar> &text, int start, int *length)
{
    const int l = *length - 1;
    if (l <= 0) {
        return false;
    }

    static const QString singleChars = QStringLiteral(".,:;?!#");
    if (singleChars.contains(text.at(start + l))) {
        *length = l;
        return true;
    }

    static const QString closingParenthesis = QStringLiteral(")]");
    static const QString openingParenthesis = QStringLiteral("([");
    for (int i = 0; i < closingParenthesis.size(); ++i) {
        if (text.at(start + l) == closingParenthesis.at(i)) {
            int count = 0;
            for (int j = l - 1; j >= 0; --j) {
                if (text.at(start + j) == closingParenthesis.at(i)) {
                    count++;
                }
                if (text.at(start + j) == openingParenthesis.at(i)) {
                    if (count > 0) {
                        count--;
                    } else {
                        return false;
                    }
                }
            }
            if (count != 0) {
                return false;
            }
            *length = l;
            return true;
        }
    }

    return false;
}

static bool isSupportedScheme(const QString &scheme)
{
    const QString lower = scheme.toLower();
    return lower == QStringLiteral("http")
        || lower == QStringLiteral("https")
        || lower == QStringLiteral("ftp")
        || lower == QStringLiteral("mailto")
        || lower == QStringLiteral("file");
}

static bool isUrl(const QVector<QChar> &text, int textLen, int start, int *segmentLen)
{
    int dist = 0;
    int schemeLen = 0;
    if (scanToUrlStart(text, textLen, start, &dist, &schemeLen)) {
        if (dist) {
            *segmentLen = dist;
            return false;
        }
        int len = 0;
        scanToUrlEnd(text, textLen, start + schemeLen, &len);
        if (len) {
            len += schemeLen;
            QString candidate;
            candidate.reserve(len);
            for (int i = 0; i < len; ++i) {
                candidate.append(text.at(start + i));
            }
            QUrl url(candidate, QUrl::TolerantMode);
            if (url.isValid() && isSupportedScheme(url.scheme())) {
                removeUnwantedTrailingCharFromEnclosedUrl(start, text, &len);
                while (removeUnwantedTrailingCharFromUrl(text, start, &len)) {
                }
                *segmentLen = len;
                return true;
            }
            *segmentLen = len;
            return false;
        }
        len = 1;
        const int maxLen = textLen - start;
        while ((start + len) < textLen && isUrlSchemeStartChar(text.at(start + len)) && len < maxLen) {
            len++;
        }
        *segmentLen = len;
        return false;
    }
    *segmentLen = dist;
    return false;
}

SciEdit::SciEdit(QWidget * parent)
    : ScintillaEdit(parent)
{
    setAcceptDrops(true);

    setCodePage(SC_CP_UTF8);
    setScrollWidth(1);
    setScrollWidthTracking(true);
    setMultiPaste(SC_MULTIPASTE_EACH);

    indicSetStyle(kUrlHoverIndicator, INDIC_PLAIN);
    indicSetHoverStyle(kUrlHoverIndicator, INDIC_DOTS);

    indicSetStyle(kSmartHighlightIndicator, INDIC_ROUNDBOX);
    indicSetAlpha(kSmartHighlightIndicator, 100);
    indicSetOutlineAlpha(kSmartHighlightIndicator, 150);
    indicSetUnder(kSmartHighlightIndicator, true);

    indicSetStyle(kFindResultIndicator, INDIC_ROUNDBOX);
    indicSetAlpha(kFindResultIndicator, 100);
    indicSetOutlineAlpha(kFindResultIndicator, 150);
    indicSetUnder(kFindResultIndicator, true);

    indicSetStyle(kTagMatchIndicator, INDIC_ROUNDBOX);
    indicSetAlpha(kTagMatchIndicator, 110);
    indicSetOutlineAlpha(kTagMatchIndicator, 160);
    indicSetUnder(kTagMatchIndicator, true);

    for (int i = 0; i < kMarkStyleCount; ++i) {
        const int indicator = markStyleIndicator(i);
        indicSetStyle(indicator, INDIC_ROUNDBOX);
        indicSetAlpha(indicator, 100);
        indicSetOutlineAlpha(indicator, 150);
        indicSetUnder(indicator, true);
        indicSetFore(indicator, toBgr(markStyleColor(i)));
    }

    indicSetStyle(kSpellCheckIndicator, INDIC_SQUIGGLE);
    indicSetUnder(kSpellCheckIndicator, true);

    m_urlScanTimer = new QTimer(this);
    m_urlScanTimer->setInterval(200);
    m_urlScanTimer->setSingleShot(true);
    connect(m_urlScanTimer, &QTimer::timeout, this, &SciEdit::scanVisibleUrls);

    m_spellCheckController = new SpellCheckController(this, this);
    // A fresh editor has no lexer attached yet (plain text): style 0 is the
    // only style in use, so spell checking applies to everything.
    m_styleNeedsSpellCheck[0] = true;

    connect(this, &ScintillaEditBase::notify, this, [this](Scintilla::NotificationData *pscn) {
        if (!pscn) {
            return;
        }
        if (pscn->nmhdr.code == Scintilla::Notification::IndicatorClick) {
            openUrlAtIndicatorPosition(pscn->position, pscn->modifiers);
            return;
        }
        if (pscn->nmhdr.code == Scintilla::Notification::Painted) {
            scheduleUrlScanIfTriggered();
        }
        if (pscn->nmhdr.code == Scintilla::Notification::CharAdded) {
            const int ch = pscn->ch;
            const int eolMode = eOLMode();
            if (((eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF) && ch == '\n') || (eolMode == SC_EOL_CR && ch == '\r')) {
                if (!m_macroRecording && !m_macroPlayback) {
                    autoIndentLine(lineFromPosition(currentPos()));
                }
            }
            if (!m_macroRecording && !m_macroPlayback && !m_isLargeFile) {
                showAutoCompletion();
            }
        }
    });

    connect(this, &ScintillaEditBase::marginClicked, this, [this](Scintilla::Position position, Scintilla::KeyMod, int margin) {
        const sptr_t line = lineFromPosition(position);
        if (margin == kBookmarkMargin) {
            const sptr_t mask = markerGet(line);
            if (mask & kHideLinesMask) {
                if (mask & (1 << kHideLinesBeginMarker)) {
                    showHiddenSectionFrom(line);
                } else if (mask & (1 << kHideLinesEndMarker)) {
                    sptr_t beginLine = line - 1;
                    while (beginLine >= 0 && !(markerGet(beginLine) & (1 << kHideLinesBeginMarker))) {
                        --beginLine;
                    }
                    if (beginLine >= 0) {
                        showHiddenSectionFrom(beginLine);
                    }
                }
                return;
            }
            if (mask & kBookmarkMask) {
                markerDelete(line, kBookmarkMarker);
            } else {
                markerAdd(line, kBookmarkMarker);
            }
            return;
        }
        if (margin == kFoldMargin) {
            toggleFold(line);
        }
    });

    connect(this, &ScintillaEditBase::updateUi, this, [this](Scintilla::Update updated){
        const sptr_t curPos = currentPos();
        emit cursorPosChanged(lineFromPosition(curPos) + 1, column(curPos));
        updateLineNumberMarginWidth();

        if (int(updated) & int(Scintilla::Update::VScroll)) {
            requestUrlScan();
            highlightSelectedWord();
            if (m_spellCheckController) {
                m_spellCheckController->requestScan();
            }
        }

        if (int(updated) & (int(Scintilla::Update::Selection) | int(Scintilla::Update::Content))) {
            highlightSelectedWord();
            highlightBraceMatch();
            highlightTagMatch();
        }
    });

    connect(this, QOverload<Scintilla::ModificationFlags, Scintilla::Position, Scintilla::Position, Scintilla::Position, const QByteArray &, Scintilla::Position, Scintilla::FoldLevel, Scintilla::FoldLevel>::of(&ScintillaEditBase::modified), 
            this, &SciEdit::textChanged);

    connect(this, QOverload<Scintilla::ModificationFlags, Scintilla::Position, Scintilla::Position, Scintilla::Position, const QByteArray &, Scintilla::Position, Scintilla::FoldLevel, Scintilla::FoldLevel>::of(&ScintillaEditBase::modified),
            this, [this](Scintilla::ModificationFlags type, Scintilla::Position position, Scintilla::Position length,
                          Scintilla::Position, const QByteArray &, Scintilla::Position, Scintilla::FoldLevel, Scintilla::FoldLevel) {
        using Mod = Scintilla::ModificationFlags;
        if ((type & (Mod::InsertText | Mod::DeleteText)) != Mod::None) {
            updateBeginSelectPosition((type & Mod::InsertText) != Mod::None, position, length);
            if (m_spellCheckController) {
                m_spellCheckController->textModified(position, length);
            }
        }
    });

    connect(this, &SciEdit::textChanged, this, &SciEdit::requestUrlScan);
    connect(this, &ScintillaEditBase::resized, this, &SciEdit::requestUrlScan);
    connect(this, &ScintillaEditBase::resized, this, [this]() {
        if (m_spellCheckController) {
            m_spellCheckController->requestScan();
        }
    });
    connect(this, &ScintillaEditBase::zoom, this, [this](int) {
        updateLineNumberMarginWidth();
        requestUrlScan();
        if (m_spellCheckController) {
            m_spellCheckController->requestScan();
        }
    });

    setLineNumbersEnabled(true);
    setFolding(BoxFoldType, kFoldMargin);

    setMarginTypeN(kBookmarkMargin, SC_MARGIN_SYMBOL);
    setMarginMaskN(kBookmarkMargin, kBookmarkMarginMask);
    setMarginSensitiveN(kBookmarkMargin, 1);
    setMarginWidthN(kBookmarkMargin, 16);
    setMarkerDefine(kBookmarkMarker, SC_MARK_BOOKMARK);
    setMarkerDefine(kHideLinesBeginMarker, SC_MARK_ARROW);
    setMarkerDefine(kHideLinesEndMarker, SC_MARK_ARROW);
    setMarkerDefine(kWarningMarker, SC_MARK_SHORTARROW);
    setMarkerDefine(kErrorMarker, SC_MARK_SHORTARROW);
}

void SciEdit::dragEnterEvent(QDragEnterEvent *event)
{
    if (event && event->mimeData() && event->mimeData()->hasUrls()) {
        for (const QUrl &u : event->mimeData()->urls()) {
            if (u.isLocalFile()) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    ScintillaEdit::dragEnterEvent(event);
}

void SciEdit::dropEvent(QDropEvent *event)
{
    if (event && event->mimeData() && event->mimeData()->hasUrls()) {
        QStringList paths;
        const QList<QUrl> urls = event->mimeData()->urls();
        paths.reserve(urls.size());
        for (const QUrl &u : urls) {
            if (!u.isLocalFile()) {
                continue;
            }
            const QString p = u.toLocalFile();
            if (!p.isEmpty()) {
                paths.push_back(p);
            }
        }
        if (!paths.isEmpty()) {
            emit filesDropped(paths);
            event->acceptProposedAction();
            return;
        }
    }

    ScintillaEdit::dropEvent(event);
}

void SciEdit::contextMenuEvent(QContextMenuEvent *event)
{
    using namespace Qt::Literals::StringLiterals;

    if (!event) {
        return;
    }

    // Right-click on the bookmark margin: request the bookmark context menu
    // instead of the regular editor popup.
    const int x = event->pos().x();
    const int marginsBeforeBookmark = int(marginWidthN(0));
    const int bookmarkMarginWidth = int(marginWidthN(kBookmarkMargin));
    if (x >= marginsBeforeBookmark && x < marginsBeforeBookmark + bookmarkMarginWidth) {
        emit bookmarkMarginContextMenuRequested(event->globalPos());
        event->accept();
        return;
    }

    const QPoint pos = event->pos();
    const sptr_t position = positionFromPoint(pos.x(), pos.y());
    bool inSelection = false;

    const int selectionCount = int(selections());
    const int selectionIterations = selectionCount > 0 ? selectionCount : 1;
    for (int i = 0; i < selectionIterations; ++i) {
        const sptr_t start = selectionNStart(i);
        const sptr_t end = selectionNEnd(i);
        const sptr_t minPos = qMin(start, end);
        const sptr_t maxPos = qMax(start, end);
        if (position >= 0 && position >= minPos && position <= maxPos) {
            inSelection = true;
            break;
        }
    }

    if (position >= 0 && !inSelection) {
        setSel(position, position);
    }

    // Detect a misspelled word under the caret before building the menu so
    // that both popup paths (customized and KXmlGUI default) can prepend the
    // suggestion section.
    QString misspelledWord;
    sptr_t misspellStart = -1;
    sptr_t misspellEnd = -1;
    if (position >= 0
        && m_spellCheckController
        && m_spellCheckController->enabled()
        && m_spellCheckController->speller().isValid()) {
        const int indicators = int(indicatorAllOnFor(position));
        if (indicators & (1 << kSpellCheckIndicator)) {
            misspellStart = indicatorStart(kSpellCheckIndicator, position);
            misspellEnd = indicatorEnd(kSpellCheckIndicator, position);
            if (misspellEnd > misspellStart) {
                misspelledWord = QString::fromUtf8(get_text_range(int(misspellStart), int(misspellEnd)));
            }
        }
    }

    auto *window = qobject_cast<KXmlGuiWindow *>(this->window());
    if (!window) {
        return ScintillaEdit::contextMenuEvent(event);
    }

    if (EditorPopupConfig::isCustomized()) {
        KActionCollection *actions = window->actionCollection();
        QMenu menu(this);
        if (!misspelledWord.isEmpty()) {
            addSpellCheckMenuSection(&menu, misspelledWord, misspellStart, misspellEnd);
        }
        if (actions) {
            const QStringList items = EditorPopupConfig::items();
            for (const QString &name : items) {
                if (name.isEmpty()) {
                    menu.addSeparator();
                    continue;
                }
                if (QAction *a = actions->action(name)) {
                    menu.addAction(a);
                }
            }
        }
        menu.exec(event->globalPos());
        event->accept();
        return;
    }

    auto *factory = window->guiFactory();
    auto *container = factory ? factory->container(u"editor_popup"_s, window) : nullptr;
    auto *menu = qobject_cast<QMenu *>(container);
    if (!menu) {
        return ScintillaEdit::contextMenuEvent(event);
    }

    if (misspelledWord.isEmpty()) {
        menu->exec(event->globalPos());
    } else {
        // Compose a temporary menu instead of mutating the shared KXmlGUI
        // container; QActions can safely live in several menus at once.
        QMenu popup(this);
        addSpellCheckMenuSection(&popup, misspelledWord, misspellStart, misspellEnd);
        popup.addActions(menu->actions());
        popup.exec(event->globalPos());
    }
    event->accept();
}

void SciEdit::addSpellCheckMenuSection(QMenu *menu, const QString &word, sptr_t start, sptr_t end)
{
    if (!menu || word.isEmpty() || end <= start || !m_spellCheckController) {
        return;
    }

    QStringList suggestions = m_spellCheckController->speller().suggest(word);
    if (suggestions.size() > 10) {
        suggestions = suggestions.mid(0, 10);
    }
    for (const QString &suggestion : std::as_const(suggestions)) {
        const QByteArray bytes = suggestion.toUtf8();
        menu->addAction(suggestion, this, [this, start, end, bytes]() {
            setTargetRange(start, end);
            replaceTarget(bytes.size(), bytes.constData());
        });
    }
    menu->addSeparator();
    menu->addAction(tr("Ignore Word"), this, [this, word]() {
        m_spellCheckController->speller().addToSession(word);
        m_spellCheckController->scanNow();
    });
    menu->addAction(tr("Add to Dictionary"), this, [this, word]() {
        m_spellCheckController->speller().addToPersonal(word);
        m_spellCheckController->scanNow();
    });
    menu->addSeparator();
}

void SciEdit::keyPressEvent(QKeyEvent *event)
{
    if (multipleSelection() && m_columnSel2MultiEdit && event) {
        const int key = event->key();
        const int selMode = selectionMode();
        if (selMode == SC_SEL_RECTANGLE || selMode == SC_SEL_THIN) {
            switch (key) {
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Backspace:
            case Qt::Key_Delete:
                // Convert column selection to multi-caret stream selection.
                // Two calls are needed to fix the moveExtends state; see Scintilla bug #2412.
                setSelectionMode(SC_SEL_STREAM);
                setSelectionMode(SC_SEL_STREAM);
                break;
            case Qt::Key_Escape: {
                // Collapse to a single caret at the main selection's caret position
                const sptr_t mainSel = mainSelection();
                const sptr_t caret = selectionNCaret(mainSel);
                setSel(caret, caret);
                setSelectionMode(SC_SEL_STREAM);
                break;
            }
            default:
                break;
            }
        }
    }
    ScintillaEdit::keyPressEvent(event);
}

void SciEdit::mouseMoveEvent(QMouseEvent *event)
{
    // UltraEdit-style column selection mode: when enabled, any left-button drag
    // is forced into rectangular selection mode. Scintilla's Qt layer derives the
    // rectangular modifier from QApplication::keyboardModifiers() (global state),
    // so synthetic events cannot inject it; instead we switch the current selection
    // to rectangle mode on the first drag move. Editor::ButtonMoveWithModifiers
    // extends the rectangle whenever sel.IsRectangular() is true, and a plain
    // single click (no movement) keeps stream mode, so caret placement and word/
    // line selection are unaffected.
    if (m_columnSelectMode && (event->buttons() & Qt::LeftButton)
        && selectionMode() != SC_SEL_RECTANGLE) {
        setSelectionMode(SC_SEL_RECTANGLE);
    }
    ScintillaEdit::mouseMoveEvent(event);
}

void SciEdit::setStyleFont(const QFont &font, int style)
{
    QString fontName = font.family();
    if (!font.styleName().isEmpty()) {
        fontName += QLatin1Char('/') + font.styleName();
    }
    styleSetFont(style, fontName.toUtf8().constData());
    styleSetSizeFractional(style, long(font.pointSizeF() * SC_FONT_SIZE_MULTIPLIER));
    styleSetItalic(style, font.italic());
    styleSetBold(style, font.bold());
}

void SciEdit::setFolding(FoldType foldType, int margin)
{
    if (foldType == NoFoldType) {
        setMarginWidthN(margin, 0L);
        return;
    }

    int mask = modEventMask();
    setModEventMask(mask | SC_MOD_CHANGEFOLD);
    setFoldFlags(SC_FOLDFLAG_LINEAFTER_CONTRACTED);
    setMarginTypeN(margin, SC_MARGIN_SYMBOL);
    setMarginMaskN(margin, SC_MASK_FOLDERS);
    setMarginSensitiveN(margin, 1);

    setMarkerDefine(SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
    setMarkerDefine(SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
    setMarkerDefine(SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
    setMarkerDefine(SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
    setMarkerDefine(SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
    setMarkerDefine(SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
    setMarkerDefine(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);

    setMarginWidthN(margin, 14);
    return;
}

void SciEdit::foldCurrentLevel()
{
    const sptr_t currentLine = lineFromPosition(currentPos());

    sptr_t foldHeader = foldParent(currentLine);
    if (foldHeader < 0) {
        const sptr_t currentFoldLevel = foldLevel(currentLine);
        if (currentFoldLevel & SC_FOLDLEVELHEADERFLAG) {
            foldHeader = currentLine;
        }
    }

    if (foldHeader >= 0) {
        foldChildren(foldHeader, SC_FOLDACTION_CONTRACT);
    }
}

void SciEdit::unfoldCurrentLevel()
{
    const sptr_t currentLine = lineFromPosition(currentPos());

    sptr_t foldHeader = foldParent(currentLine);
    if (foldHeader < 0) {
        const sptr_t currentFoldLevel = foldLevel(currentLine);
        if (currentFoldLevel & SC_FOLDLEVELHEADERFLAG) {
            foldHeader = currentLine;
        }
    }

    if (foldHeader >= 0) {
        foldChildren(foldHeader, SC_FOLDACTION_EXPAND);
    }
}

void SciEdit::setLexer(Scintilla::ILexer5 *lexer)
{
    setLexer(lexer, QString());
}

void SciEdit::setLexer(Scintilla::ILexer5 *lexer, const QString &languageIdOverride, bool colouriseNow)
{
    clearTagMatchHighlight();
    setILexer((sptr_t)lexer);
    if (lexer == nullptr) {
        m_languageId.clear();
        clearDocumentStyle();
        // Plain text: style 0 is the only style in use, check everything.
        m_styleNeedsSpellCheck.fill(false);
        m_styleNeedsSpellCheck[0] = true;
    } else if (!languageIdOverride.isEmpty()) {
        m_languageId = languageIdOverride;
    } else {
        m_languageId = QString::fromUtf8(lexer->GetName());
    }
    setProperty("fold", "1");
    setProperty("fold.compact", "1");
    setProperty("fold.comment", "1");
    setAutomaticFold(SC_AUTOMATICFOLD_SHOW | SC_AUTOMATICFOLD_CLICK | SC_AUTOMATICFOLD_CHANGE);
    applyLexerProperties();
    applyTheme();
    applyLexerKeywords();
    if (colouriseNow) {
        colourise(0, -1);
    }
}

void SciEdit::setLineNumbersEnabled(bool enabled)
{
    m_lineNumbersEnabled = enabled;
    setMarginTypeN(0, SC_MARGIN_NUMBER);
    m_lastLineNumberDigits = 0;
    m_lastLineNumberZoom = std::numeric_limits<int>::min();
    m_lastLineNumberMarginWidth = -1;
    updateLineNumberMarginWidth();
}

void SciEdit::setEditorFont(const QFont &font)
{
    m_editorFont = font;
    m_lastLineNumberDigits = 0;
    m_lastLineNumberZoom = std::numeric_limits<int>::min();
    m_lastLineNumberMarginWidth = -1;
    applyTheme();
}

void SciEdit::setThemeId(const QString &themeId)
{
    QString id = themeId.trimmed();
    if (id.isEmpty()) {
        id = QStringLiteral("Pineapple Notepad Light");
    }
    if (m_themeId == id) {
        return;
    }
    m_themeId = id;
    m_lastLineNumberDigits = 0;
    m_lastLineNumberZoom = std::numeric_limits<int>::min();
    m_lastLineNumberMarginWidth = -1;
    applyTheme();
}

void SciEdit::applyTheme()
{
    const QString baseThemeId = m_themeId.isEmpty() ? QStringLiteral("Pineapple Notepad Light") : m_themeId;

    const QColor bg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.BackgroundColor"));
    const QColor fg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("Normal"));
    const QColor selectionBg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.TextSelection"));
    const QColor caretLineBg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.CurrentLine"));
    const QColor lineNumberFg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.LineNumbers"));
    const QColor lineNumberBg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.IconBorder"));
    const QColor currentLineNumberFg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.CurrentLineNumber"));
    const QColor urlHoverFg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("Information"));
    const QColor foldMarkerFg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.CodeFolding"));
    const QColor foldMarkerBg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.CodeFolding.bg"));
    const QColor foldHighlight = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.CodeFolding.highlight"));
    const QColor searchHighlightColor = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.SearchHighlight"));
    const QColor braceMatchBg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.BracketMatching"));
    const QColor braceMatchFg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.BracketMatching.fg"));
    const QColor braceBadFg = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("Error"));
    const QColor indentGuideColor = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.IndentationLine"));
    const QColor tabMarkerColor = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.TabMarker"));
    const QColor modifiedLinesColor = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.ModifiedLines"));
    const QColor bookmarkColor = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.MarkBookmark"));
    const QColor warningColor = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.MarkWarning"));
    const QColor errorColor = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.MarkError"));

    styleResetDefault();
    if (!m_editorFont.family().isEmpty()) {
        setStyleFont(m_editorFont, STYLE_DEFAULT);
    }
    styleSetFore(STYLE_DEFAULT, toBgr(fg));
    styleSetBack(STYLE_DEFAULT, toBgr(bg));
    styleClearAll();

    setCaretFore(toBgr(fg));
    setSelFore(true, toBgr(fg));
    setSelBack(true, toBgr(selectionBg));
    setCaretLineVisible(true);
    setCaretLineVisibleAlways(true);
    setCaretLineBack(toBgr(caretLineBg));

    styleSetFore(STYLE_LINENUMBER, toBgr(lineNumberFg));
    styleSetBack(STYLE_LINENUMBER, toBgr(lineNumberBg));
    setMarginBackN(0, toBgr(lineNumberBg));
    setMarginBackN(kBookmarkMargin, toBgr(lineNumberBg));
    setMarginBackN(kFoldMargin, toBgr(lineNumberBg));
    setMarginBackN(kChangeHistoryMargin, toBgr(lineNumberBg));
    sendColor(SCI_SETFOLDMARGINCOLOUR, 1, lineNumberBg);
    sendColor(SCI_SETFOLDMARGINHICOLOUR, 1, lineNumberBg);

    indicSetFore(kUrlHoverIndicator, toBgr(urlHoverFg));
    indicSetHoverFore(kUrlHoverIndicator, toBgr(urlHoverFg));

    indicSetFore(kSmartHighlightIndicator, toBgr(searchHighlightColor));
    indicSetFore(kFindResultIndicator, toBgr(searchHighlightColor));

    const QColor spellCheckColor = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.SpellChecking"));
    indicSetFore(kSpellCheckIndicator, toBgr(spellCheckColor));

    const QColor tagMatchColor = ThemeConfigStore::colorForRole(baseThemeId, QStringLiteral("editor.tagMatchHighlight"));
    indicSetFore(kTagMatchIndicator, toBgr(tagMatchColor));

    styleSetFore(STYLE_BRACELIGHT, toBgr(braceMatchFg));
    styleSetBack(STYLE_BRACELIGHT, toBgr(braceMatchBg));
    styleSetBold(STYLE_BRACELIGHT, true);
    styleSetFore(STYLE_BRACEBAD, toBgr(braceBadFg));
    styleSetBold(STYLE_BRACEBAD, true);

    styleSetFore(STYLE_INDENTGUIDE, toBgr(indentGuideColor));
    styleSetBack(STYLE_INDENTGUIDE, toBgr(bg));

    sendColor(SCI_SETWHITESPACEFORE, 1, tabMarkerColor);

    static constexpr int foldMarkers[] = {
        SC_MARKNUM_FOLDEROPEN,
        SC_MARKNUM_FOLDER,
        SC_MARKNUM_FOLDERSUB,
        SC_MARKNUM_FOLDERTAIL,
        SC_MARKNUM_FOLDEREND,
        SC_MARKNUM_FOLDEROPENMID,
        SC_MARKNUM_FOLDERMIDTAIL,
    };
    for (int markerNumber : foldMarkers) {
        sendColor(SCI_MARKERSETFORE, markerNumber, foldMarkerFg);
        sendColor(SCI_MARKERSETBACK, markerNumber, foldMarkerBg);
        sendColor(SCI_MARKERSETBACKSELECTED, markerNumber, foldHighlight);
    }
    send(SCI_MARKERENABLEHIGHLIGHT, 1, 0);
    sendColor(SCI_MARKERSETFORE, kBookmarkMarker, bg);
    sendColor(SCI_MARKERSETBACK, kBookmarkMarker, bookmarkColor);
    sendColor(SCI_MARKERSETFORE, kWarningMarker, bg);
    sendColor(SCI_MARKERSETBACK, kWarningMarker, warningColor);
    sendColor(SCI_MARKERSETFORE, kErrorMarker, bg);
    sendColor(SCI_MARKERSETBACK, kErrorMarker, errorColor);
    const QColor hideLineColor(0x77, 0xCC, 0x77);
    sendColor(SCI_MARKERSETFORE, kHideLinesBeginMarker, hideLineColor);
    sendColor(SCI_MARKERSETBACK, kHideLinesBeginMarker, hideLineColor);
    sendColor(SCI_MARKERSETFORE, kHideLinesEndMarker, hideLineColor);
    sendColor(SCI_MARKERSETBACK, kHideLinesEndMarker, hideLineColor);
    setElementColour(SC_ELEMENT_HIDDEN_LINE, toBgr(hideLineColor) | 0xFF000000);

    sendColor(SCI_MARKERSETFORE, SC_MARKNUM_HISTORY_MODIFIED, modifiedLinesColor);
    sendColor(SCI_MARKERSETBACK, SC_MARKNUM_HISTORY_MODIFIED, bg);

    setElementColour(SC_ELEMENT_CARET_LINE_BACK, toBgr(caretLineBg) | 0xFF000000);
    setCaretLineFrame(0);

    updateLineNumberMarginWidth();
    applyLexerStyles();
}

void SciEdit::toggleBookmark()
{
    const sptr_t line = lineFromPosition(currentPos());
    const sptr_t mask = markerGet(line);
    if (mask & kBookmarkMask) {
        markerDelete(line, kBookmarkMarker);
    } else {
        markerAdd(line, kBookmarkMarker);
    }
}

bool SciEdit::goToNextBookmark(bool wrap)
{
    const sptr_t totalLines = lineCount();
    if (totalLines <= 0) {
        return false;
    }

    const sptr_t currentLine = lineFromPosition(currentPos());
    sptr_t target = markerNext(currentLine + 1, kBookmarkMask);
    if (target < 0 && wrap) {
        target = markerNext(0, kBookmarkMask);
    }
    if (target < 0) {
        return false;
    }

    gotoLine(target);
    scrollCaret();
    return true;
}

bool SciEdit::goToPreviousBookmark(bool wrap)
{
    const sptr_t totalLines = lineCount();
    if (totalLines <= 0) {
        return false;
    }

    const sptr_t currentLine = lineFromPosition(currentPos());
    sptr_t startLine = currentLine - 1;
    if (startLine < 0) {
        startLine = 0;
    }

    sptr_t target = markerPrevious(startLine, kBookmarkMask);
    if (target < 0 && wrap) {
        target = markerPrevious(totalLines - 1, kBookmarkMask);
    }
    if (target < 0) {
        return false;
    }

    gotoLine(target);
    scrollCaret();
    return true;
}

void SciEdit::clearAllBookmarks()
{
    markerDeleteAll(kBookmarkMarker);
}

void SciEdit::clearBookmarksInRange(int start, int end)
{
    if (start < 0 || end <= start) {
        return;
    }
    const sptr_t firstLine = lineFromPosition(start);
    const sptr_t lastLine = lineFromPosition(end - 1);
    for (sptr_t line = firstLine; line <= lastLine; ++line) {
        while (markerGet(line) & kBookmarkMask) {
            markerDelete(line, kBookmarkMarker);
        }
    }
}

void SciEdit::inverseBookmarks()
{
    const sptr_t total = lineCount();
    for (sptr_t line = 0; line < total; ++line) {
        if (markerGet(line) & kBookmarkMask) {
            markerDelete(line, kBookmarkMarker);
        } else {
            markerAdd(line, kBookmarkMarker);
        }
    }
}

QString SciEdit::bookmarkedLinesText()
{
    QString text;
    const sptr_t total = lineCount();
    for (sptr_t line = 0; line < total; ++line) {
        if (markerGet(line) & kBookmarkMask) {
            const sptr_t start = positionFromLine(line);
            const sptr_t len = lineLength(line);
            text += QString::fromUtf8(get_text_range(int(start), int(start + len)));
        }
    }
    return text;
}

void SciEdit::copyBookmarkedLines()
{
    const QString text = bookmarkedLinesText();
    if (!text.isEmpty()) {
        setClipboardText(text);
    }
}

void SciEdit::cutBookmarkedLines()
{
    const QString text = bookmarkedLinesText();

    beginUndoAction();
    for (sptr_t line = lineCount() - 1; line >= 0; --line) {
        if (markerGet(line) & kBookmarkMask) {
            markerDelete(line, kBookmarkMarker);
            const sptr_t start = positionFromLine(line);
            const sptr_t len = lineLength(line);
            setTargetRange(start, start + len);
            replaceTarget(0, "");
        }
    }
    endUndoAction();

    if (!text.isEmpty()) {
        setClipboardText(text);
    }
}

void SciEdit::pasteToBookmarkedLines()
{
    const QString text = clipboardText();
    if (text.isEmpty()) {
        return;
    }
    const QByteArray bytes = text.toUtf8();

    beginUndoAction();
    for (sptr_t line = lineCount() - 1; line >= 0; --line) {
        if (markerGet(line) & kBookmarkMask) {
            setTargetRange(positionFromLine(line), lineEndPosition(line));
            replaceTarget(bytes.size(), bytes.constData());
        }
    }
    endUndoAction();
}

void SciEdit::removeLinesByBookmark(bool marked)
{
    beginUndoAction();
    for (sptr_t line = lineCount() - 1; line >= 0; --line) {
        const bool has = (markerGet(line) & kBookmarkMask) != 0;
        if (has == marked) {
            markerDelete(line, kBookmarkMarker);
            const sptr_t start = positionFromLine(line);
            const sptr_t len = lineLength(line);
            setTargetRange(start, start + len);
            replaceTarget(0, "");
        }
    }
    endUndoAction();
}

void SciEdit::removeBookmarkedLines()
{
    removeLinesByBookmark(true);
}

void SciEdit::removeUnbookmarkedLines()
{
    removeLinesByBookmark(false);
}

int SciEdit::markStyleIndicator(int styleIndex)
{
    switch (styleIndex) {
    case 0: return kMarkStyle1Indicator;
    case 1: return kMarkStyle2Indicator;
    case 2: return kMarkStyle3Indicator;
    case 3: return kMarkStyle4Indicator;
    default: return kMarkStyle5Indicator;
    }
}

QColor SciEdit::markStyleColor(int styleIndex)
{
    static const QColor colors[kMarkStyleCount] = {
        QColor(0xFF, 0x00, 0x00),
        QColor(0x00, 0xFF, 0xFF),
        QColor(0xFF, 0xA5, 0x00),
        QColor(0xFF, 0xE4, 0x00),
        QColor(0x80, 0x00, 0x80),
    };
    return colors[qBound(0, styleIndex, kMarkStyleCount - 1)];
}

int SciEdit::markStyleMask(int styleIndex)
{
    return 1 << qBound(0, styleIndex, kMarkStyleCount - 1);
}

int SciEdit::allMarkStylesMask()
{
    return (1 << kMarkStyleCount) - 1;
}

int SciEdit::markAllInRange(int flags, const QByteArray &needle, int start, int end,
                            int styleIndex, bool bookmarkLine)
{
    if (needle.isEmpty() || start < 0 || end <= start) {
        return 0;
    }

    setIndicatorCurrent(markStyleIndicator(styleIndex));
    setSearchFlags(flags);

    const sptr_t oldModEventMask = send(SCI_GETMODEVENTMASK);
    send(SCI_SETMODEVENTMASK, 0);

    int count = 0;
    sptr_t pos = start;
    while (pos <= end) {
        setTargetRange(pos, end);
        const sptr_t targetStart = searchInTarget(needle.size(), needle.constData());
        if (targetStart < 0) {
            break;
        }
        const sptr_t targetEndPos = targetEnd();
        if (targetEndPos > end) {
            break;
        }
        const sptr_t foundLen = targetEndPos - targetStart;
        if (foundLen > 0) {
            indicatorFillRange(targetStart, foundLen);
            ++count;
            if (bookmarkLine) {
                const sptr_t lineStart = lineFromPosition(targetStart);
                const sptr_t lineEnd = lineFromPosition(targetEndPos - 1);
                for (sptr_t line = lineStart; line <= lineEnd; ++line) {
                    if (!(markerGet(line) & kBookmarkMask)) {
                        markerAdd(line, kBookmarkMarker);
                    }
                }
            }
        }
        if (targetEndPos >= end) {
            break;
        }
        pos = (foundLen == 0) ? targetStart + 1 : targetEndPos;
    }

    send(SCI_SETMODEVENTMASK, oldModEventMask);
    return count;
}

void SciEdit::clearMarks(int styleIndex, int start, int end)
{
    setIndicatorCurrent(markStyleIndicator(styleIndex));
    if (start < 0 || end <= start) {
        indicatorClearRange(0, length());
    } else {
        indicatorClearRange(start, end - start);
    }
}

void SciEdit::clearAllMarkStyles()
{
    for (int i = 0; i < kMarkStyleCount; ++i) {
        clearMarks(i);
    }
}

void SciEdit::styleTokenRange(int start, int length, int styleIndex)
{
    if (length <= 0) {
        return;
    }
    setIndicatorCurrent(markStyleIndicator(styleIndex));
    indicatorFillRange(start, length);
}

QList<QPair<int, QString>> SciEdit::markedTexts(int styleMask)
{
    QList<QPair<int, QString>> result;
    const sptr_t docLen = length();

    for (int style = 0; style < kMarkStyleCount; ++style) {
        if (!(styleMask & (1 << style))) {
            continue;
        }
        const int indicator = markStyleIndicator(style);
        sptr_t pos = 0;
        while (pos < docLen) {
            if (indicatorValueAt(indicator, pos)) {
                const sptr_t segStart = indicatorStart(indicator, pos);
                const sptr_t segEnd = indicatorEnd(indicator, pos);
                if (segEnd > segStart) {
                    result.append({int(segStart),
                                   QString::fromUtf8(get_text_range(int(segStart), int(segEnd)))});
                    pos = segEnd;
                    continue;
                }
            }
            const sptr_t next = indicatorEnd(indicator, pos);
            pos = (next > pos) ? next : pos + 1;
        }
    }

    std::sort(result.begin(), result.end(),
              [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                  return a.first < b.first;
              });
    return result;
}

QString SciEdit::joinMarkedTexts(const QList<QPair<int, QString>> &texts)
{
    if (texts.isEmpty()) {
        return QString();
    }

    bool containsEol = false;
    for (const auto &t : texts) {
        if (t.second.contains(QLatin1Char('\n')) || t.second.contains(QLatin1Char('\r'))) {
            containsEol = true;
            break;
        }
    }

    const QString delim = (containsEol && texts.size() > 1)
                              ? QStringLiteral("\r\n----\r\n")
                              : QStringLiteral("\r\n");

    QString out;
    for (int i = 0; i < texts.size(); ++i) {
        if (i > 0) {
            out += delim;
        }
        out += texts.at(i).second;
    }
    if (texts.size() > 1) {
        out += QLatin1String("\r\n");
    }
    return out;
}

bool SciEdit::goToNextMarked(int styleIndex, bool wrap)
{
    const int indicator = markStyleIndicator(styleIndex);
    const sptr_t docLen = length();
    const sptr_t caret = currentPos();

    sptr_t bestStart = -1;
    sptr_t bestEnd = -1;
    sptr_t firstStart = -1;
    sptr_t firstEnd = -1;

    sptr_t pos = 0;
    while (pos < docLen) {
        if (indicatorValueAt(indicator, pos)) {
            const sptr_t s = indicatorStart(indicator, pos);
            const sptr_t e = indicatorEnd(indicator, pos);
            if (e <= s) {
                break;
            }
            if (firstStart < 0) {
                firstStart = s;
                firstEnd = e;
            }
            // Notepad++ semantics: the caret sitting at a segment's start
            // counts as being inside it, so only segments starting strictly
            // after the caret qualify as "next".
            if (s > caret) {
                bestStart = s;
                bestEnd = e;
                break;
            }
            pos = e;
        } else {
            const sptr_t next = indicatorEnd(indicator, pos);
            pos = (next > pos) ? next : pos + 1;
        }
    }

    if (bestStart < 0 && wrap && firstStart >= 0) {
        bestStart = firstStart;
        bestEnd = firstEnd;
    }
    if (bestStart < 0) {
        return false;
    }

    ensureVisibleEnforcePolicy(lineFromPosition(bestStart));
    setSel(bestStart, bestEnd);
    scrollCaret();
    return true;
}

bool SciEdit::goToPreviousMarked(int styleIndex, bool wrap)
{
    const int indicator = markStyleIndicator(styleIndex);
    const sptr_t docLen = length();
    const sptr_t caret = currentPos();

    sptr_t bestStart = -1;
    sptr_t bestEnd = -1;
    sptr_t lastStart = -1;
    sptr_t lastEnd = -1;

    sptr_t pos = 0;
    while (pos < docLen) {
        if (indicatorValueAt(indicator, pos)) {
            const sptr_t s = indicatorStart(indicator, pos);
            const sptr_t e = indicatorEnd(indicator, pos);
            if (e <= s) {
                break;
            }
            lastStart = s;
            lastEnd = e;
            if (e <= caret) {
                bestStart = s;
                bestEnd = e;
            }
            pos = e;
        } else {
            const sptr_t next = indicatorEnd(indicator, pos);
            pos = (next > pos) ? next : pos + 1;
        }
    }

    if (bestStart < 0 && wrap && lastStart >= 0) {
        bestStart = lastStart;
        bestEnd = lastEnd;
    }
    if (bestStart < 0) {
        return false;
    }

    ensureVisibleEnforcePolicy(lineFromPosition(bestStart));
    setSel(bestStart, bestEnd);
    scrollCaret();
    return true;
}

void SciEdit::beginOrEndSelect(bool columnMode)
{
    const sptr_t currPos = currentPos();
    if (m_beginSelectPosition < 0) {
        m_beginSelectPosition = currPos;
        m_beginSelectColumnMode = columnMode;
    } else {
        const bool useColumn = columnMode;
        setSelectionMode(useColumn ? SC_SEL_RECTANGLE : SC_SEL_STREAM);
        if (useColumn) {
            setAnchor(m_beginSelectPosition);
        } else {
            setSel(m_beginSelectPosition, currPos);
        }
        m_beginSelectPosition = -1;
        m_beginSelectColumnMode = false;
    }
}

bool SciEdit::isBeginEndSelectActive() const
{
    return m_beginSelectPosition >= 0;
}

bool SciEdit::isBeginEndSelectColumnMode() const
{
    return m_beginSelectColumnMode;
}

void SciEdit::cancelBeginEndSelect()
{
    m_beginSelectPosition = -1;
    m_beginSelectColumnMode = false;
}

void SciEdit::updateBeginSelectPosition(bool isInsert, sptr_t position, sptr_t length)
{
    if (m_beginSelectPosition < 0 || position >= m_beginSelectPosition - 1) {
        return;
    }
    if (isInsert) {
        m_beginSelectPosition += length;
    } else {
        m_beginSelectPosition -= length;
    }
    if (m_beginSelectPosition < 0) {
        m_beginSelectPosition = 0;
    }
}

void SciEdit::hideSelectedLines()
{
    sptr_t startLine = lineFromPosition(send(SCI_GETSELECTIONSTART));
    sptr_t endLine = lineFromPosition(send(SCI_GETSELECTIONEND));

    const sptr_t nbLines = lineCount();
    if (nbLines < 3)
        return;

    // First and last lines must remain visible as anchors
    if (startLine == 0)
        ++startLine;

    if (endLine == nbLines - 1)
        --endLine;

    if (startLine > endLine)
        return;

    // scope tracks the balance between open and close markers found.
    // Positive means more opens than closes, negative means more closes than opens.
    // recentMarkerWasOpen tracks whether the last removed marker was an open marker,
    // used to handle the case where scope returns to 0 after removing an open marker.
    int scope = 0;
    bool recentMarkerWasOpen = false;

    const auto removeMarker = [this, &scope, &recentMarkerWasOpen](sptr_t line) {
        const sptr_t state = markerGet(line);
        const bool closePresent = (state & (1 << kHideLinesEndMarker)) != 0;
        const bool openPresent = (state & (1 << kHideLinesBeginMarker)) != 0;

        if (closePresent) {
            markerDelete(line, kHideLinesEndMarker);
            recentMarkerWasOpen = false;
            --scope;
        }
        if (openPresent) {
            markerDelete(line, kHideLinesBeginMarker);
            recentMarkerWasOpen = true;
            ++scope;
        }
    };

    sptr_t startMarker = startLine - 1;
    sptr_t endMarker = endLine + 1;

    // Remove existing markers in and around the selection range
    removeMarker(startMarker);
    for (sptr_t i = startLine; i <= endLine; ++i)
        removeMarker(i);
    removeMarker(endMarker);

    // If scope is 0 and last removed was an open marker, it means we removed a
    // matching pair inside the selection. We need to expand outward to find and
    // remove the enclosing pair that contained our selection.
    if (scope == 0 && recentMarkerWasOpen) {
        while (scope == 0 && startMarker >= 0)
            removeMarker(--startMarker);
        while (scope != 0 && endMarker < nbLines)
            removeMarker(++endMarker);
    } else {
        // Otherwise, balance any unmatched markers by expanding outward
        while (scope < 0 && startMarker >= 0)
            removeMarker(--startMarker);
        while (scope > 0 && endMarker < nbLines)
            removeMarker(++endMarker);
    }

    // Place new markers around the expanded range
    markerAdd(startMarker, kHideLinesBeginMarker);
    markerAdd(endMarker, kHideLinesEndMarker);

    hideLines(startLine, endLine);
}

void SciEdit::showAllHiddenLines()
{
    const sptr_t totalLines = lineCount();
    sptr_t line = 0;
    while (line < totalLines) {
        line = markerNext(line, kHideLinesMask);
        if (line < 0)
            break;
        markerDelete(line, kHideLinesBeginMarker);
        markerDelete(line, kHideLinesEndMarker);
        ++line;
    }
    showLines(0, totalLines - 1);
}

bool SciEdit::hasHiddenLines()
{
    return markerNext(0, 1 << kHideLinesBeginMarker) >= 0;
}

void SciEdit::showHiddenSectionFrom(sptr_t beginLine)
{
    const sptr_t totalLines = lineCount();
    sptr_t endLine = beginLine + 1;
    while (endLine < totalLines) {
        if (markerGet(endLine) & (1 << kHideLinesEndMarker))
            break;
        ++endLine;
    }
    if (endLine >= totalLines)
        return;

    showLines(beginLine + 1, endLine - 1);
    markerDelete(beginLine, kHideLinesBeginMarker);
    markerDelete(endLine, kHideLinesEndMarker);
}

QPair<int, int> SciEdit::findLast(int flags, const char *needle, int scanStart, int endExclusive)
{
    int bestStart = -1;
    int bestEnd = -1;
    int scanPos = scanStart;
    while (scanPos < endExclusive) {
        const QPair<int, int> found = findText(flags, needle, scanPos, endExclusive);
        if (found.first < 0 || found.first >= endExclusive) {
            break;
        }
        bestStart = found.first;
        bestEnd = found.second;
        scanPos = found.first + 1;
    }
    return QPair<int, int>(bestStart, bestEnd);
}

void SciEdit::sendColor(unsigned int iMessage, uptr_t wParam, const QColor &color) const
{
    send(iMessage, wParam, toBgr(color));
}

void SciEdit::setMarkerDefine(int markerNumber, int markerSymbol)
{
    markerDefine(markerNumber, markerSymbol);
}

void SciEdit::updateLineNumberMarginWidth()
{
    if (!m_lineNumbersEnabled) {
        setMarginWidthN(0, 0);
        return;
    }

    const int lines = qMax<sptr_t>(1, lineCount());
    const int digits = QString::number(lines).size();
    const int zoomLevel = int(zoom());
    if (digits == m_lastLineNumberDigits && zoomLevel == m_lastLineNumberZoom && m_lastLineNumberMarginWidth == marginWidthN(0) && m_lastLineNumberMarginWidth > 0) {
        return;
    }
    m_lastLineNumberDigits = digits;
    m_lastLineNumberZoom = zoomLevel;

    const QString sample = QString(digits, QLatin1Char('9'));
    const QByteArray sampleBytes = sample.toUtf8();
    const sptr_t width = textWidth(STYLE_LINENUMBER, sampleBytes.constData()) + 10;
    m_lastLineNumberMarginWidth = int(width);
    setMarginWidthN(0, width);
}

void SciEdit::applyLexerStyles()
{
    if (m_languageId.isEmpty()) {
        // Plain text (no lexer attached): style 0 is the only style in use,
        // everything is spell-checked.
        m_styleNeedsSpellCheck.fill(false);
        m_styleNeedsSpellCheck[0] = true;
        return;
    }

    m_styleNeedsSpellCheck.fill(false);

    const LanguageSpec *langSpec = LanguageConfigStore::languageSpec(m_languageId);
    LanguageSpec fallback;
    if (!langSpec) {
        fallback = LanguageConfigStore::baseLanguageSpec(QString::fromUtf8(lexerLanguage()));
        if (!fallback.styles.isEmpty()) {
            langSpec = &fallback;
        }
    }
    if (!langSpec) {
        return;
    }

    const QString baseThemeId = m_themeId.isEmpty() ? QStringLiteral("Pineapple Notepad Light") : m_themeId;
    auto roleColor = [&](const QString &role) -> QColor {
        return ThemeConfigStore::colorForRole(baseThemeId, role);
    };

    for (auto itStyle = langSpec->styles.constBegin(); itStyle != langSpec->styles.constEnd(); ++itStyle) {
        const int styleId = itStyle.key();
        const StyleSpec &spec = itStyle.value();

        const QColor fg = ThemeConfigStore::colorForTextStyle(baseThemeId, spec.textStyle);
        styleSetFore(styleId, toBgr(fg));
        styleSetBold(styleId, spec.bold);
        styleSetItalic(styleId, spec.italic);
        styleSetUnderline(styleId, spec.underline);
        styleSetEOLFilled(styleId, spec.eolFilled);
        if (spec.spellCheck && styleId >= 0 && styleId < 256) {
            m_styleNeedsSpellCheck[styleId] = true;
        }
    }

    if (lexerLanguage() == QByteArrayLiteral("ksyntaxhighlighting")) {
        applyKshExtendedStyles(baseThemeId, roleColor);
    }
}

void SciEdit::applyKshExtendedStyles(const QString &baseThemeId,
                                     const std::function<QColor(const QString &)> &roleColor)
{
    setProperty("lexer.ksyntaxhighlighting.theme", baseThemeId.toUtf8().constData());
    setProperty("lexer.ksyntaxhighlighting.tabwidth", QByteArray::number(tabWidth()));
    const QByteArray blob = property("lexer.ksyntaxhighlighting.styles");
    if (blob.isEmpty())
        return;

    const QColor bg = roleColor(QStringLiteral("editor.BackgroundColor"));
    const sptr_t bgBgr = toBgr(bg);

    for (const QByteArray &entry : blob.split(';')) {
        if (entry.isEmpty())
            continue;
        const QList<QByteArray> fields = entry.split(':');
        if (fields.size() != 3)
            continue;

        bool ok = false;
        const int styleId = fields[0].toInt(&ok);
        if (!ok || styleId < 0 || styleId > 255)
            continue;
        const int textStyleId = fields[1].toInt(&ok);
        if (!ok || textStyleId < 0)
            continue;
        const QByteArray &flags = fields[2];
        if (flags.size() != 5)
            continue;

        // flags.at(4) is the spellCheck flag ('C' or '-'); base-style entries
        // (styleId 0-30) exist in the serialization only for spell checking,
        // so record them before the extended-style-only filter below.
        if (flags.at(4) == 'C')
            m_styleNeedsSpellCheck[styleId] = true;

        if (styleId < STYLE_LASTPREDEFINED + 1)
            continue;

        // textStyleId is the numeric KSH Theme::TextStyle (0..Others); cast directly
        // instead of round-tripping through nameOfStyle() + colorForRole().
        const auto ts = static_cast<KSyntaxHighlighting::Theme::TextStyle>(textStyleId);
        const QColor fg = ThemeConfigStore::colorForTextStyle(baseThemeId, ts);
        styleSetFore(styleId, toBgr(fg));
        styleSetBack(styleId, bgBgr);

        styleSetBold(styleId, flags.at(0) == 'B');
        styleSetItalic(styleId, flags.at(1) == 'I');
        styleSetUnderline(styleId, flags.at(2) == 'U');
        // Scintilla has no per-style strikeThrough; flag is parsed but not applied.
    }
}

void SciEdit::toggleComment()
{
    if (m_languageId.isEmpty()) {
        return;
    }

    const CommentSpec comment = LanguageConfigStore::commentSpec(m_languageId);
    const bool hasLineComment = !comment.singleLineComment.trimmed().isEmpty();
    const bool hasStreamComment = !comment.multiLineComment.start.isEmpty() && !comment.multiLineComment.end.isEmpty();

    if (!hasLineComment && !hasStreamComment) {
        return;
    }

    // Determine which comment style to use based on context
    sptr_t selStart = selectionStart();
    sptr_t selEnd = selectionEnd();
    if (selStart > selEnd) {
        std::swap(selStart, selEnd);
    }

    const bool hasSelection = (selStart != selEnd);

    if (!hasSelection) {
        // No selection: treat as current line -> line comment
        if (hasLineComment) {
            toggleLineComment();
        } else if (hasStreamComment) {
            toggleStreamComment();
        }
        return;
    }

    const sptr_t startLine = lineFromPosition(selStart);
    sptr_t endLine = lineFromPosition(selEnd);
    // If selEnd is at the very start of a line (and it's not a single-line
    // selection), treat the previous line as the actual end line.  This
    // mirrors the adjustment in toggleLineComment().
    if (selStart != selEnd && selEnd == positionFromLine(endLine)) {
        endLine = qMax<sptr_t>(startLine, endLine - 1);
    }
    const sptr_t startLineBegin = positionFromLine(startLine);
    const sptr_t endLineEnd = positionFromLine(endLine + 1); // start of next line

    // Check if selection covers full lines (from line start to line end)
    const bool isFullLineSelection = (selStart == startLineBegin) &&
                                     (selEnd == endLineEnd || selEnd == lineEndPosition(endLine));

    if (isFullLineSelection && hasLineComment) {
        toggleLineComment();
    } else if (hasStreamComment) {
        toggleStreamComment();
    } else if (hasLineComment) {
        toggleLineComment();
    }
}

void SciEdit::toggleLineComment()
{
    if (m_languageId.isEmpty()) {
        return;
    }

    const CommentSpec comment = LanguageConfigStore::commentSpec(m_languageId);
    const QString prefix = comment.singleLineComment.trimmed();
    if (prefix.isEmpty()) {
        return;
    }

    const QByteArray prefixBytes = prefix.toUtf8();
    const QByteArray prefixWithSpace = prefixBytes + QByteArrayLiteral(" ");

    sptr_t selStart = selectionStart();
    sptr_t selEnd = selectionEnd();
    if (selStart > selEnd) {
        std::swap(selStart, selEnd);
    }

    const sptr_t startLine = lineFromPosition(selStart);
    sptr_t endLine = lineFromPosition(selEnd);
    if (selStart != selEnd && selEnd == positionFromLine(endLine)) {
        endLine = qMax<sptr_t>(startLine, endLine - 1);
    }

    auto isWhitespace = [](sptr_t c) {
        return c == ' ' || c == '\t';
    };

    auto matchesAt = [&](sptr_t pos, const QByteArray &needle) -> bool {
        if (needle.isEmpty()) {
            return false;
        }
        for (int i = 0; i < needle.size(); ++i) {
            if (charAt(pos + i) != static_cast<unsigned char>(needle.at(i))) {
                return false;
            }
        }
        return true;
    };

    bool allCommented = true;
    for (sptr_t line = startLine; line <= endLine; ++line) {
        const sptr_t lineStart = positionFromLine(line);
        const sptr_t lineEnd = lineEndPosition(line);

        sptr_t pos = lineStart;
        while (pos < lineEnd && isWhitespace(charAt(pos))) {
            ++pos;
        }

        if (pos == lineEnd) {
            continue;
        }

        if (!matchesAt(pos, prefixBytes)) {
            allCommented = false;
            break;
        }
    }

    // Track selection adjustment: insertText (SCI_INSERTTEXT) clears the
    // selection via SetEmptySelection, so we must restore it manually.
    sptr_t adjustForStartLine = 0;
    sptr_t adjustForAll = 0;

    beginUndoAction();

    for (sptr_t line = endLine; line >= startLine; --line) {
        const sptr_t lineStart = positionFromLine(line);
        const sptr_t lineEnd = lineEndPosition(line);

        sptr_t pos = lineStart;
        while (pos < lineEnd && isWhitespace(charAt(pos))) {
            ++pos;
        }

        if (allCommented) {
            if (pos == lineEnd) {
                continue;
            }
            if (!matchesAt(pos, prefixBytes)) {
                continue;
            }
            sptr_t removeLen = prefixBytes.size();
            if (pos + removeLen < lineEnd && charAt(pos + removeLen) == ' ') {
                ++removeLen;
            }
            deleteRange(pos, removeLen);
            adjustForAll -= removeLen;
            if (line == startLine && selStart >= pos) {
                adjustForStartLine -= removeLen;
            }
        } else {
            if (pos == lineEnd) {
                continue;
            }
            insertText(pos, prefixWithSpace.constData());
            adjustForAll += prefixWithSpace.size();
            if (line == startLine && selStart >= pos) {
                adjustForStartLine += prefixWithSpace.size();
            }
        }

        if (line == startLine) {
            break;
        }
    }

    endUndoAction();
    setSel(selStart + adjustForStartLine, selEnd + adjustForAll);
}

void SciEdit::toggleStreamComment()
{
    if (m_languageId.isEmpty()) {
        return;
    }

    const CommentSpec comment = LanguageConfigStore::commentSpec(m_languageId);
    const QString startStr = comment.multiLineComment.start;
    const QString endStr = comment.multiLineComment.end;
    if (startStr.isEmpty() || endStr.isEmpty()) {
        return;
    }

    const QByteArray startBytes = startStr.toUtf8();
    const QByteArray endBytes = endStr.toUtf8();

    sptr_t selStart = selectionStart();
    sptr_t selEnd = selectionEnd();
    if (selStart > selEnd) {
        std::swap(selStart, selEnd);
    }

    const sptr_t docLength = length();

    auto matchesAt = [&](sptr_t pos, const QByteArray &needle) -> bool {
        if (needle.isEmpty()) {
            return false;
        }
        if (pos < 0 || pos + needle.size() > docLength) {
            return false;
        }
        for (int i = 0; i < needle.size(); ++i) {
            if (charAt(pos + i) != static_cast<unsigned char>(needle.at(i))) {
                return false;
            }
        }
        return true;
    };

    enum class Mode { Add, RemoveEnclosed, RemoveSelf };
    Mode mode = Mode::Add;
    sptr_t commentStartPos = -1;
    sptr_t commentEndPos = -1;

    // Case 1: selection itself starts with /* and ends with */
    if (matchesAt(selStart, startBytes) && matchesAt(selEnd - endBytes.size(), endBytes)
        && (selEnd - selStart >= startBytes.size() + endBytes.size())) {
        mode = Mode::RemoveSelf;
        commentStartPos = selStart;
        commentEndPos = selEnd - endBytes.size();
    } else {
        // Case 2: search for an enclosing /* ... */ pair around the selection
        // Search backwards from selStart for startBytes, but stop if we hit a
        // closing endBytes first (which means any earlier startBytes is already closed).
        for (sptr_t pos = selStart - startBytes.size(); pos >= 0; --pos) {
            if (matchesAt(pos, endBytes)) {
                break;
            }
            if (matchesAt(pos, startBytes)) {
                commentStartPos = pos;
                break;
            }
        }

        // Search forwards from selEnd for endBytes, but stop if we hit an
        // opening startBytes first (which means we've left the current comment).
        if (commentStartPos >= 0) {
            for (sptr_t pos = selEnd; pos + endBytes.size() <= docLength; ++pos) {
                if (matchesAt(pos, startBytes)) {
                    break;
                }
                if (matchesAt(pos, endBytes)) {
                    commentEndPos = pos;
                    break;
                }
            }
        }

        if (commentStartPos >= 0 && commentEndPos >= 0) {
            mode = Mode::RemoveEnclosed;
        }
    }

    beginUndoAction();

    if (mode == Mode::RemoveSelf || mode == Mode::RemoveEnclosed) {
        // RemoveSelf:  selection IS /* content */ → commentStartPos=selStart, commentEndPos=selEnd-endBytes.size()
        // RemoveEnclosed: selection is inside /* content */ → commentStartPos=selStart-startBytes.size(), commentEndPos=selEnd
        const bool hasSpaceBefore = (commentEndPos > 0 && charAt(commentEndPos - 1) == ' ');
        const bool hasSpaceAfter = (commentStartPos + startBytes.size() < docLength && charAt(commentStartPos + startBytes.size()) == ' ');

        // Delete right side first (*/), then left side (/*) to keep positions stable
        const sptr_t rightStart = commentEndPos - (hasSpaceBefore ? 1 : 0);
        const sptr_t rightLen = endBytes.size() + (hasSpaceBefore ? 1 : 0);
        deleteRange(rightStart, rightLen);

        const sptr_t leftLen = startBytes.size() + (hasSpaceAfter ? 1 : 0);
        deleteRange(commentStartPos, leftLen);

        // Content that was between the markers is now at commentStartPos
        const sptr_t contentLen = (commentEndPos - (hasSpaceBefore ? 1 : 0))
                                - (commentStartPos + startBytes.size() + (hasSpaceAfter ? 1 : 0));
        setSel(commentStartPos, commentStartPos + contentLen);
    } else {
        // Add stream comment: /* SELECTED */
        insertText(selEnd, " */");
        insertText(selStart, "/* ");
        setSel(selStart, selEnd + startBytes.size() + endBytes.size() + 2);
    }

    endUndoAction();
}

void SciEdit::trimTrailingWhitespace()
{
    sptr_t selStart = selectionStart();
    sptr_t selEnd = selectionEnd();
    if (selStart > selEnd) {
        std::swap(selStart, selEnd);
    }

    sptr_t startLine = 0;
    sptr_t endLine = lineCount() - 1;
    if (selStart != selEnd) {
        startLine = lineFromPosition(selStart);
        endLine = lineFromPosition(selEnd);
        if (selEnd == positionFromLine(endLine)) {
            endLine = qMax<sptr_t>(startLine, endLine - 1);
        }
    }

    beginUndoAction();

    for (sptr_t line = endLine; line >= startLine; --line) {
        const sptr_t lineStart = positionFromLine(line);
        const sptr_t lineEnd = lineEndPosition(line);
        if (lineEnd <= lineStart) {
            if (line == startLine) {
                break;
            }
            continue;
        }

        sptr_t pos = lineEnd - 1;
        while (pos >= lineStart) {
            const char ch = charAt(pos);
            if (ch != ' ' && ch != '\t') {
                break;
            }
            --pos;
        }
        const sptr_t trimStart = pos + 1;
        if (trimStart < lineEnd) {
            deleteRange(trimStart, lineEnd - trimStart);
        }

        if (line == startLine) {
            break;
        }
    }

    endUndoAction();
}

void SciEdit::trimLeadingWhitespace()
{
    sptr_t selStart = selectionStart();
    sptr_t selEnd = selectionEnd();
    if (selStart > selEnd) {
        std::swap(selStart, selEnd);
    }

    sptr_t startLine = 0;
    sptr_t endLine = lineCount() - 1;
    if (selStart != selEnd) {
        startLine = lineFromPosition(selStart);
        endLine = lineFromPosition(selEnd);
        if (selEnd == positionFromLine(endLine)) {
            endLine = qMax<sptr_t>(startLine, endLine - 1);
        }
    }

    beginUndoAction();

    for (sptr_t line = endLine; line >= startLine; --line) {
        const sptr_t lineStart = positionFromLine(line);
        const sptr_t lineEnd = lineEndPosition(line);
        if (lineEnd <= lineStart) {
            if (line == startLine) {
                break;
            }
            continue;
        }

        sptr_t pos = lineStart;
        while (pos < lineEnd) {
            const char ch = charAt(pos);
            if (ch != ' ' && ch != '\t') {
                break;
            }
            ++pos;
        }
        if (pos > lineStart) {
            deleteRange(lineStart, pos - lineStart);
        }

        if (line == startLine) {
            break;
        }
    }

    endUndoAction();
}

void SciEdit::trimTrailingAndLeadingWhitespace()
{
    sptr_t selStart = selectionStart();
    sptr_t selEnd = selectionEnd();
    if (selStart > selEnd) {
        std::swap(selStart, selEnd);
    }

    sptr_t startLine = 0;
    sptr_t endLine = lineCount() - 1;
    if (selStart != selEnd) {
        startLine = lineFromPosition(selStart);
        endLine = lineFromPosition(selEnd);
        if (selEnd == positionFromLine(endLine)) {
            endLine = qMax<sptr_t>(startLine, endLine - 1);
        }
    }

    beginUndoAction();

    for (sptr_t line = endLine; line >= startLine; --line) {
        const sptr_t lineStart = positionFromLine(line);
        const sptr_t lineEnd = lineEndPosition(line);
        if (lineEnd <= lineStart) {
            if (line == startLine) {
                break;
            }
            continue;
        }

        sptr_t endPos = lineEnd - 1;
        while (endPos >= lineStart) {
            const char ch = charAt(endPos);
            if (ch != ' ' && ch != '\t') {
                break;
            }
            --endPos;
        }
        const sptr_t trimEndStart = endPos + 1;
        if (trimEndStart < lineEnd) {
            deleteRange(trimEndStart, lineEnd - trimEndStart);
        }

        const sptr_t newLineStart = positionFromLine(line);
        const sptr_t newLineEnd = lineEndPosition(line);
        sptr_t startPos = newLineStart;
        while (startPos < newLineEnd) {
            const char ch = charAt(startPos);
            if (ch != ' ' && ch != '\t') {
                break;
            }
            ++startPos;
        }
        if (startPos > newLineStart) {
            deleteRange(newLineStart, startPos - newLineStart);
        }

        if (line == startLine) {
            break;
        }
    }

    endUndoAction();
}

namespace {

sptr_t leadingWhitespaceLength(const QByteArray &text)
{
    sptr_t n = 0;
    while (n < text.size() && (text.at(n) == ' ' || text.at(n) == '\t')) {
        ++n;
    }
    return n;
}

class LeadingWhitespaceSelectionAdjuster
{
public:
    explicit LeadingWhitespaceSelectionAdjuster(SciEdit *editor)
        : m_anchor(editor->anchor())
        , m_caret(editor->currentPos())
        , m_multiSelection(editor->selections() > 1)
    {
    }

    void onLineReplaced(sptr_t lineStart, const QByteArray &oldText, const QByteArray &newText)
    {
        const sptr_t delta = static_cast<sptr_t>(newText.size()) - oldText.size();
        if (delta == 0) {
            return;
        }
        const sptr_t oldLeading = leadingWhitespaceLength(oldText);
        const sptr_t newLeading = leadingWhitespaceLength(newText);
        m_anchor = adjusted(m_anchor, lineStart, oldLeading, newLeading, delta);
        m_caret = adjusted(m_caret, lineStart, oldLeading, newLeading, delta);
    }

    void apply(SciEdit *editor) const
    {
        if (m_multiSelection) {
            return;
        }
        editor->setSelection(m_caret, m_anchor);
    }

private:
    static sptr_t adjusted(sptr_t pos, sptr_t lineStart, sptr_t oldLeading,
                           sptr_t newLeading, sptr_t delta)
    {
        if (pos <= lineStart) {
            return pos;
        }
        if (pos - lineStart <= oldLeading) {
            return lineStart + newLeading;
        }
        return pos + delta;
    }

    sptr_t m_anchor;
    sptr_t m_caret;
    bool m_multiSelection;
};

}

void SciEdit::convertTabsToSpaces(bool leadingOnly, bool documentWide)
{
    if (!documentWide && (selectionMode() == SC_SEL_RECTANGLE || selectionMode() == SC_SEL_THIN)) {
        return;
    }

    const sptr_t tabSize = tabWidth();
    sptr_t selStart = selectionStart();
    sptr_t selEnd = selectionEnd();
    if (selStart > selEnd) {
        std::swap(selStart, selEnd);
    }

    sptr_t startLine = 0;
    sptr_t endLine = lineCount() - 1;
    bool hasSelection = (selStart != selEnd);

    if (hasSelection && !documentWide) {
        startLine = lineFromPosition(selStart);
        endLine = lineFromPosition(selEnd);
        if (selEnd == positionFromLine(endLine)) {
            endLine = qMax<sptr_t>(startLine, endLine - 1);
        }
    }

    beginUndoAction();

    LeadingWhitespaceSelectionAdjuster adjuster(this);

    for (sptr_t line = endLine; line >= startLine; --line) {
        const sptr_t lineStart = positionFromLine(line);
        const sptr_t lineEnd = lineEndPosition(line);
        if (lineEnd <= lineStart) {
            if (line == startLine) {
                break;
            }
            continue;
        }

        QByteArray lineText = get_text_range(lineStart, lineEnd);
        if (!lineText.contains('\t')) {
            if (line == startLine) {
                break;
            }
            continue;
        }

        QByteArray result;
        result.reserve(lineText.size() * tabSize);
        sptr_t column = 0;
        bool inLeading = true;

        for (int i = 0; i < lineText.size(); ++i) {
            const char ch = lineText.at(i);
            if (ch == '\t') {
                if (!leadingOnly || inLeading) {
                    const sptr_t spacesToInsert = tabSize - (column % tabSize);
                    result.append(QByteArray(spacesToInsert, ' '));
                    column += spacesToInsert;
                } else {
                    result.append(ch);
                    column = ((column / tabSize) + 1) * tabSize;
                }
            } else {
                result.append(ch);
                if (leadingOnly && inLeading && ch != ' ' && ch != '\t') {
                    inLeading = false;
                }
                if ((ch & 0xC0) != 0x80) {
                    ++column;
                }
            }
        }

        setTargetRange(lineStart, lineEnd);
        replaceTarget(-1, result.constData());
        adjuster.onLineReplaced(lineStart, lineText, result);

        if (line == startLine) {
            break;
        }
    }

    endUndoAction();
    adjuster.apply(this);
}

void SciEdit::convertSpacesToTabs(bool leadingOnly, bool documentWide)
{
    if (!documentWide && (selectionMode() == SC_SEL_RECTANGLE || selectionMode() == SC_SEL_THIN)) {
        return;
    }

    const sptr_t tabSize = tabWidth();
    sptr_t selStart = selectionStart();
    sptr_t selEnd = selectionEnd();
    if (selStart > selEnd) {
        std::swap(selStart, selEnd);
    }

    sptr_t startLine = 0;
    sptr_t endLine = lineCount() - 1;
    bool hasSelection = (selStart != selEnd);

    if (hasSelection && !documentWide) {
        startLine = lineFromPosition(selStart);
        endLine = lineFromPosition(selEnd);
        if (selEnd == positionFromLine(endLine)) {
            endLine = qMax<sptr_t>(startLine, endLine - 1);
        }
    }

    beginUndoAction();

    LeadingWhitespaceSelectionAdjuster adjuster(this);

    for (sptr_t line = endLine; line >= startLine; --line) {
        const sptr_t lineStart = positionFromLine(line);
        const sptr_t lineEnd = lineEndPosition(line);
        if (lineEnd <= lineStart) {
            if (line == startLine) {
                break;
            }
            continue;
        }

        QByteArray lineText = get_text_range(lineStart, lineEnd);
        QByteArray result;
        result.reserve(lineText.size());

        sptr_t column = 0;
        sptr_t spaceStart = -1;
        sptr_t spaceCount = 0;
        bool inLeading = true;

        for (int i = 0; i <= lineText.size(); ++i) {
            const char ch = (i < lineText.size()) ? lineText.at(i) : '\0';

            if (ch == ' ') {
                if (spaceStart < 0) {
                    spaceStart = column;
                }
                ++spaceCount;
                ++column;
            } else {
                if (spaceStart >= 0 && spaceCount > 0) {
                    sptr_t spacesProcessed = 0;
                    while (spacesProcessed < spaceCount) {
                        const sptr_t nextTabStop = ((spaceStart + spacesProcessed) / tabSize + 1) * tabSize;
                        const sptr_t spacesToTab = nextTabStop - (spaceStart + spacesProcessed);
                        if (spacesToTab <= spaceCount - spacesProcessed) {
                            if (inLeading || !leadingOnly) {
                                result.append('\t');
                            } else {
                                result.append(QByteArray(spacesToTab, ' '));
                            }
                            spacesProcessed += spacesToTab;
                        } else {
                            const sptr_t remaining = spaceCount - spacesProcessed;
                            if (inLeading || !leadingOnly) {
                                if (remaining > 0 && i < lineText.size() && (lineText.at(i) == ' ' || lineText.at(i) == '\t')) {
                                    result.append('\t');
                                    spacesProcessed = spaceCount;
                                } else {
                                    result.append(QByteArray(remaining, ' '));
                                    spacesProcessed = spaceCount;
                                }
                            } else {
                                result.append(QByteArray(remaining, ' '));
                                spacesProcessed = spaceCount;
                            }
                        }
                    }
                    spaceStart = -1;
                    spaceCount = 0;
                }

                if (ch == '\t') {
                    result.append('\t');
                    column = ((column / tabSize) + 1) * tabSize;
                } else if (ch != '\0') {
                    result.append(ch);
                    if (leadingOnly && inLeading && ch != ' ' && ch != '\t') {
                        inLeading = false;
                    }
                    if ((ch & 0xC0) != 0x80) {
                        ++column;
                    }
                }

                if (ch == '\n' || ch == '\r') {
                    inLeading = true;
                    column = 0;
                }
            }
        }

        if (result != lineText) {
            setTargetRange(lineStart, lineEnd);
            replaceTarget(-1, result.constData());
            adjuster.onLineReplaced(lineStart, lineText, result);
        }

        if (line == startLine) {
            break;
        }
    }

    endUndoAction();
    adjuster.apply(this);
}

void SciEdit::convertEolToSpace()
{
    if (selectionMode() == SC_SEL_RECTANGLE || selectionMode() == SC_SEL_THIN) {
        return;
    }

    sptr_t selStart = selectionStart();
    sptr_t selEnd = selectionEnd();
    if (selStart > selEnd) {
        std::swap(selStart, selEnd);
    }

    bool hasSelection = (selStart != selEnd);

    if (hasSelection) {
        sptr_t startLine = lineFromPosition(selStart);
        sptr_t endLine = lineFromPosition(selEnd);
        if (selEnd == positionFromLine(endLine)) {
            endLine = qMax<sptr_t>(startLine, endLine - 1);
        }
        selStart = positionFromLine(startLine);
        selEnd = lineEndPosition(endLine);
    } else {
        selStart = 0;
        selEnd = length();
    }

    if (selEnd <= selStart) {
        return;
    }

    QByteArray text = get_text_range(selStart, selEnd);
    if (text.isEmpty()) {
        return;
    }

    QByteArray result;
    result.reserve(text.size());

    for (int i = 0; i < text.size(); ++i) {
        const char ch = text.at(i);
        if (ch == '\r') {
            result.append(' ');
            if (i + 1 < text.size() && text.at(i + 1) == '\n') {
                ++i;
            }
        } else if (ch == '\n') {
            result.append(' ');
        } else {
            result.append(ch);
        }
    }

    beginUndoAction();
    setTargetRange(selStart, selEnd);
    replaceTarget(-1, result.constData());
    endUndoAction();
}

void SciEdit::removeConsecutiveDuplicateLines()
{
    const sptr_t totalLines = lineCount();
    if (totalLines < 2) {
        return;
    }

    sptr_t selStart = selectionStart();
    sptr_t selEnd = selectionEnd();
    if (selStart > selEnd) {
        std::swap(selStart, selEnd);
    }

    sptr_t startLine = 0;
    sptr_t endLine = totalLines - 1;
    bool hasSelection = (selStart != selEnd);

    if (hasSelection) {
        startLine = lineFromPosition(selStart);
        endLine = lineFromPosition(selEnd);
        if (selEnd == positionFromLine(endLine)) {
            endLine = qMax<sptr_t>(startLine, endLine - 1);
        }
    }

    if (startLine >= endLine) {
        return;
    }

    beginUndoAction();

    sptr_t i = startLine;
    while (i <= endLine) {
        const sptr_t lineStart = positionFromLine(i);
        const sptr_t lineEnd = lineEndPosition(i);
        const QByteArray currentLine = get_text_range(lineStart, lineEnd);

        if (i < endLine) {
            const sptr_t nextLineStart = positionFromLine(i + 1);
            const sptr_t nextLineEnd = lineEndPosition(i + 1);
            const QByteArray nextLine = get_text_range(nextLineStart, nextLineEnd);

            if (currentLine == nextLine) {
                sptr_t deleteStart = lineEnd;
                sptr_t deleteEnd = nextLineEnd;

                sptr_t j = i + 1;
                while (j < endLine) {
                    const sptr_t jNextStart = positionFromLine(j + 1);
                    const sptr_t jNextEnd = lineEndPosition(j + 1);
                    const QByteArray jNextLine = get_text_range(jNextStart, jNextEnd);

                    if (currentLine == jNextLine) {
                        deleteEnd = jNextEnd;
                        ++j;
                    } else {
                        break;
                    }
                }

                deleteRange(deleteStart, deleteEnd - deleteStart);
                endLine -= (j - i);
            }
        }

        ++i;
    }

    endUndoAction();
}

void SciEdit::removeAnyDuplicateLines()
{
    const sptr_t totalLines = lineCount();
    if (totalLines < 2) {
        return;
    }

    sptr_t selStart = selectionStart();
    sptr_t selEnd = selectionEnd();
    if (selStart > selEnd) {
        std::swap(selStart, selEnd);
    }

    sptr_t startLine = 0;
    sptr_t endLine = totalLines - 1;
    bool hasSelection = (selStart != selEnd);

    if (hasSelection) {
        startLine = lineFromPosition(selStart);
        endLine = lineFromPosition(selEnd);
        if (selEnd == positionFromLine(endLine)) {
            endLine = qMax<sptr_t>(startLine, endLine - 1);
        }
    }

    if (startLine >= endLine) {
        return;
    }

    QHash<QByteArray, int> lineCountMap;

    for (sptr_t i = startLine; i <= endLine; ++i) {
        const sptr_t lineStart = positionFromLine(i);
        const sptr_t lineEnd = lineEndPosition(i);
        const QByteArray lineText = get_text_range(lineStart, lineEnd);
        lineCountMap[lineText]++;
    }

    beginUndoAction();

    for (sptr_t i = endLine; i >= startLine; --i) {
        const sptr_t lineStart = positionFromLine(i);
        const sptr_t lineEnd = lineEndPosition(i);
        const QByteArray lineText = get_text_range(lineStart, lineEnd);

        if (lineCountMap.value(lineText, 0) > 1) {
            sptr_t deleteStart = lineStart;
            sptr_t deleteEnd;

            if (i < endLine) {
                deleteEnd = positionFromLine(i + 1);
            } else if (i > startLine) {
                deleteStart = lineEndPosition(i - 1);
                deleteEnd = lineEnd;
            } else {
                deleteEnd = lineEnd;
                if (deleteEnd < length()) {
                    deleteEnd++;
                }
            }

            deleteRange(deleteStart, deleteEnd - deleteStart);
            lineCountMap[lineText]--;
        }
    }

    endUndoAction();
}

void SciEdit::splitLines()
{
    if (selections() == 1) {
        const sptr_t selStart = selectionStart();
        const sptr_t selEnd = selectionEnd();
        const sptr_t startLine = lineFromPosition(selStart);
        const sptr_t endLine = lineFromPosition(selEnd);

        const sptr_t anchorPos = positionFromLine(startLine);
        const sptr_t caretPos = lineEndPosition(endLine);

        setSelection(caretPos, anchorPos);
        setTargetRange(anchorPos, caretPos);

        if (edgeMode() == EDGE_NONE) {
            linesSplit(0);
        } else {
            const sptr_t charWidth = textWidth(STYLE_DEFAULT, "P");
            linesSplit(charWidth * (edgeColumn() + 1));
        }
    }
}

void SciEdit::joinLines()
{
    const sptr_t selStart = selectionStart();
    const sptr_t selEnd = selectionEnd();
    const sptr_t startLine = lineFromPosition(selStart);
    const sptr_t endLine = lineFromPosition(selEnd);

    if (startLine != endLine) {
        const sptr_t anchorPos = positionFromLine(startLine);
        const sptr_t caretPos = lineEndPosition(endLine);
        setSelection(caretPos, anchorPos);
        setTargetRange(anchorPos, caretPos);
        linesJoin();
    }
}

void SciEdit::moveCurrentLineUp()
{
    const sptr_t currentLine = lineFromPosition(currentPos());
    if (currentLine <= 0) {
        return;
    }

    const sptr_t prevLineStart = positionFromLine(currentLine - 1);
    const sptr_t prevLineEnd = lineEndPosition(currentLine - 1);
    const sptr_t currentLineStart = positionFromLine(currentLine);
    const sptr_t currentLineEnd = lineEndPosition(currentLine);

    const QByteArray prevLineText = get_text_range(prevLineStart, prevLineEnd);
    const QByteArray currentLineText = get_text_range(currentLineStart, currentLineEnd);

    const bool prevHasEol = (prevLineEnd < length() && charAt(prevLineEnd) == '\r') ||
                            (prevLineEnd < length() && charAt(prevLineEnd) == '\n');
    const bool currentHasEol = (currentLineEnd < length() && charAt(currentLineEnd) == '\r') ||
                               (currentLineEnd < length() && charAt(currentLineEnd) == '\n');

    QByteArray replacement;
    replacement.append(currentLineText);
    if (prevHasEol && !currentHasEol) {
        if (prevLineEnd < length() && charAt(prevLineEnd) == '\r') {
            replacement.append('\r');
            if (prevLineEnd + 1 < length() && charAt(prevLineEnd + 1) == '\n') {
                replacement.append('\n');
            }
        } else if (prevLineEnd < length() && charAt(prevLineEnd) == '\n') {
            replacement.append('\n');
        }
    }
    replacement.append(prevLineText);

    const sptr_t deleteEnd = (currentHasEol && !prevHasEol) ? currentLineEnd : lineEndPosition(currentLine);
    const sptr_t actualDeleteEnd = (currentLine < lineCount() - 1) ? positionFromLine(currentLine + 1) : deleteEnd + 1;

    beginUndoAction();

    setTargetRange(prevLineStart, actualDeleteEnd);
    replaceTarget(-1, replacement.constData());

    const sptr_t newPos = positionFromLine(currentLine - 1) + (currentPos() - currentLineStart);
    gotoPos(newPos);

    endUndoAction();
}

void SciEdit::moveCurrentLineDown()
{
    const sptr_t totalLines = lineCount();
    const sptr_t currentLine = lineFromPosition(currentPos());
    if (currentLine >= totalLines - 1) {
        return;
    }

    const sptr_t currentLineStart = positionFromLine(currentLine);
    const sptr_t currentLineEnd = lineEndPosition(currentLine);
    const sptr_t nextLineStart = positionFromLine(currentLine + 1);
    const sptr_t nextLineEnd = lineEndPosition(currentLine + 1);

    const QByteArray currentLineText = get_text_range(currentLineStart, currentLineEnd);
    const QByteArray nextLineText = get_text_range(nextLineStart, nextLineEnd);

    const bool currentHasEol = (currentLineEnd < length() && charAt(currentLineEnd) == '\r') ||
                               (currentLineEnd < length() && charAt(currentLineEnd) == '\n');
    const bool nextHasEol = (nextLineEnd < length() && charAt(nextLineEnd) == '\r') ||
                            (nextLineEnd < length() && charAt(nextLineEnd) == '\n');

    QByteArray replacement;
    replacement.append(nextLineText);
    if (currentHasEol && !nextHasEol) {
        if (currentLineEnd < length() && charAt(currentLineEnd) == '\r') {
            replacement.append('\r');
            if (currentLineEnd + 1 < length() && charAt(currentLineEnd + 1) == '\n') {
                replacement.append('\n');
            }
        } else if (currentLineEnd < length() && charAt(currentLineEnd) == '\n') {
            replacement.append('\n');
        }
    }
    replacement.append(currentLineText);

    const sptr_t deleteEnd = (currentLine + 1 < totalLines - 1) ? positionFromLine(currentLine + 2) : nextLineEnd + 1;

    beginUndoAction();

    setTargetRange(currentLineStart, deleteEnd);
    replaceTarget(-1, replacement.constData());

    const sptr_t newPos = positionFromLine(currentLine + 1) + (currentPos() - currentLineStart);
    gotoPos(newPos);

    endUndoAction();
}

void SciEdit::removeEmptyLines(bool includingBlank)
{
    const sptr_t totalLines = lineCount();
    if (totalLines < 1) {
        return;
    }

    beginUndoAction();

    for (sptr_t i = totalLines - 1; i >= 0; --i) {
        const sptr_t lineStart = positionFromLine(i);
        const sptr_t lineEnd = lineEndPosition(i);

        bool isEmpty = false;
        if (lineEnd <= lineStart) {
            isEmpty = true;
        } else if (includingBlank) {
            bool allBlank = true;
            for (sptr_t pos = lineStart; pos < lineEnd; ++pos) {
                const char ch = charAt(pos);
                if (ch != ' ' && ch != '\t') {
                    allBlank = false;
                    break;
                }
            }
            isEmpty = allBlank;
        }

        if (isEmpty) {
            sptr_t deleteStart = lineStart;
            sptr_t deleteEnd;

            if (i < totalLines - 1) {
                deleteEnd = positionFromLine(i + 1);
            } else if (i > 0) {
                deleteStart = lineEndPosition(i - 1);
                deleteEnd = lineEnd;
                if (deleteEnd < length()) {
                    deleteEnd++;
                }
            } else {
                deleteEnd = lineEnd;
                if (deleteEnd < length()) {
                    deleteEnd++;
                }
            }

            if (deleteEnd > deleteStart) {
                deleteRange(deleteStart, deleteEnd - deleteStart);
            }
        }
    }

    endUndoAction();
}

QByteArray SciEdit::eolString() const
{
    switch (eOLMode()) {
    case SC_EOL_CRLF: return "\r\n";
    case SC_EOL_CR: return "\r";
    default: return "\n";
    }
}

void SciEdit::insertEmptyLineAbove()
{
    const QByteArray eol = eolString();
    const sptr_t currentLine = lineFromPosition(currentPos());

    beginUndoAction();

    if (currentLine == 0) {
        insertText(0, eol.constData());
    } else {
        const sptr_t insertPos = positionFromLine(currentLine) - eol.length();
        insertText(insertPos, eol.constData());
    }

    gotoPos(positionFromLine(currentLine));
    endUndoAction();
}

void SciEdit::insertEmptyLineBelow()
{
    const QByteArray eol = eolString();
    const sptr_t currentLine = lineFromPosition(currentPos());
    const sptr_t totalLines = lineCount();

    beginUndoAction();

    if (currentLine >= totalLines - 1) {
        insertText(length(), eol.constData());
    } else {
        const sptr_t insertPos = positionFromLine(currentLine + 1);
        insertText(insertPos, eol.constData());
    }

    gotoPos(positionFromLine(currentLine + 1));
    endUndoAction();
}

bool SciEdit::getSortLineRange(sptr_t &fromLine, sptr_t &toLine)
{
    const sptr_t totalLines = lineCount();
    if (totalLines < 2) {
        return false;
    }

    const sptr_t selStart = selectionStart();
    const sptr_t selEnd = selectionEnd();

    if (selStart != selEnd) {
        fromLine = lineFromPosition(qMin(selStart, selEnd));
        toLine = lineFromPosition(qMax(selStart, selEnd));
        if (fromLine == toLine) {
            return false;
        }
    } else {
        fromLine = 0;
        toLine = totalLines - 1;
    }

    return true;
}

void SciEdit::reorderLines(const std::function<void(QStringList&)> &reorderFunc)
{
    sptr_t fromLine = 0;
    sptr_t toLine = 0;
    if (!getSortLineRange(fromLine, toLine)) {
        return;
    }

    const sptr_t totalLines = lineCount();
    const bool isEntireDocument = (toLine == totalLines - 1);
    const sptr_t startPos = positionFromLine(fromLine);
    const sptr_t endPos = positionFromLine(toLine) + lineLength(toLine);

    const QByteArray textBytes = get_text_range(startPos, endPos);
    const QString eolStr = QString::fromUtf8(eolString());
    const QString text = QString::fromUtf8(textBytes.constData(), textBytes.size());

    QStringList lines = text.split(eolStr);

    if (!isEntireDocument && !lines.isEmpty() && lines.last().isEmpty()) {
        lines.removeLast();
    }

    reorderFunc(lines);

    QString joined = lines.join(eolStr);
    if (!isEntireDocument) {
        joined += eolStr;
    }

    const QByteArray joinedBytes = joined.toUtf8();

    beginUndoAction();
    setTargetRange(startPos, endPos);
    replaceTarget(-1, joinedBytes.constData());
    endUndoAction();
}

void SciEdit::reverseLines()
{
    reorderLines([](QStringList &lines) {
        std::reverse(lines.begin(), lines.end());
    });
}

void SciEdit::randomizeLines()
{
    reorderLines([](QStringList &lines) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(lines.begin(), lines.end(), g);
    });
}

void SciEdit::sortLinesAscending()
{
    reorderLines([](QStringList &lines) {
        std::sort(lines.begin(), lines.end(), [](const QString &a, const QString &b) {
            return a.compare(b) < 0;
        });
    });
}

void SciEdit::sortLinesDescending()
{
    reorderLines([](QStringList &lines) {
        std::sort(lines.begin(), lines.end(), [](const QString &a, const QString &b) {
            return a.compare(b) > 0;
        });
    });
}

static bool isSingleQuoteChar(QChar c)
{
    return c == u'\'' || c == u'\u2019' || c == u'\u2018';
}

static void properCaseConvert(QString &s, bool blend)
{
    const int len = s.size();
    for (int i = 0; i < len; ++i) {
        if (!s.at(i).isLetter())
            continue;
        if (i >= 2 && isSingleQuoteChar(s.at(i - 1)) && s.at(i - 2).isLetterOrNumber()) {
            if (!blend)
                s[i] = s.at(i).toLower();
        } else if (i < 1 || !s.at(i - 1).isLetterOrNumber()) {
            s[i] = s.at(i).toUpper();
        } else if (!blend) {
            s[i] = s.at(i).toLower();
        }
    }
}

static void sentenceCaseConvert(QString &s, bool blend)
{
    const int len = s.size();
    bool isNewSentence = true;
    bool wasEolR = false;
    bool wasEolN = false;
    for (int i = 0; i < len; ++i) {
        const QChar ch = s.at(i);
        if (ch.isLetter()) {
            if (isNewSentence) {
                s[i] = ch.toUpper();
                isNewSentence = false;
            } else if (!blend) {
                s[i] = ch.toLower();
            }
            wasEolR = false;
            wasEolN = false;
            if (ch == u'i') {
                const bool leftOk = (i < 1) ? false
                    : (s.at(i - 1).isSpace() || s.at(i - 1) == u'(' || s.at(i - 1) == u'"');
                const bool rightOk = (i + 1 == len) ? false
                    : (s.at(i + 1).isSpace() || s.at(i + 1) == u'\'');
                if (leftOk && rightOk)
                    s[i] = u'I';
            }
        } else if (ch == u'.' || ch == u'!' || ch == u'?') {
            isNewSentence = (i + 1 == len) ? true : !s.at(i + 1).isLetterOrNumber();
        } else if (ch == u'\r') {
            if (wasEolR)
                isNewSentence = true;
            else
                wasEolR = true;
        } else if (ch == u'\n') {
            if (wasEolN)
                isNewSentence = true;
            else
                wasEolN = true;
        }
    }
}

static void invertCaseConvert(QString &s)
{
    for (int i = 0; i < s.size(); ++i) {
        if (s.at(i).isLower())
            s[i] = s.at(i).toUpper();
        else if (s.at(i).isUpper())
            s[i] = s.at(i).toLower();
    }
}

static void randomCaseConvert(QString &s)
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 1);
    for (int i = 0; i < s.size(); ++i) {
        if (s.at(i).isLetter()) {
            if (dist(rng))
                s[i] = s.at(i).toUpper();
            else
                s[i] = s.at(i).toLower();
        }
    }
}

// Strip the four whitespace characters ignored by base64 decoders (<space> <tab> <lf> <cr>).
static QByteArray stripBase64Whitespace(const QByteArray &input)
{
    QByteArray out;
    out.reserve(input.size());
    for (char c : input) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
            out += c;
    }
    return out;
}

// Base64 encode with options for padding, per-line encoding and line wrapping.
// byLine encodes each input line as its own (unpadded) base64 string, preserving EOL bytes.
// wrapLength (>0, ignored when byLine) inserts Unix newlines every wrapLength output characters.
static QByteArray base64EncodeData(const QByteArray &input, bool withPadding, bool byLine, int wrapLength)
{
    if (byLine) {
        QByteArray result;
        int pos = 0;
        while (pos < input.size()) {
            const int eol = input.indexOf('\n', pos);
            const int lineEnd = (eol == -1) ? input.size() : eol;
            int contentEnd = lineEnd;
            if (contentEnd > pos && input.at(contentEnd - 1) == '\r')
                --contentEnd;
            const QByteArray line = input.mid(pos, contentEnd - pos);
            result += line.toBase64(QByteArray::Base64Encoding | QByteArray::OmitTrailingEquals);
            result += input.mid(contentEnd, lineEnd - contentEnd);
            if (eol != -1) {
                result += '\n';
                pos = eol + 1;
            } else {
                pos = input.size();
            }
        }
        return result;
    }

    QByteArray::Base64Options opt = QByteArray::Base64Encoding;
    if (!withPadding)
        opt |= QByteArray::OmitTrailingEquals;
    QByteArray encoded = input.toBase64(opt);
    if (wrapLength > 0 && encoded.size() > wrapLength) {
        QByteArray wrapped;
        wrapped.reserve(encoded.size() + encoded.size() / wrapLength);
        for (int i = 0; i < encoded.size(); i += wrapLength) {
            if (i > 0)
                wrapped += '\n';
            wrapped += encoded.mid(i, wrapLength);
        }
        encoded = wrapped;
    }
    return encoded;
}

// Base64 decode. The lenient mode ignores whitespace; strict mode rejects malformed input;
// whitespaceReset decodes each line independently (reversing the "by line" encode).
static std::optional<QByteArray> base64DecodeData(const QByteArray &input, bool strict, bool whitespaceReset)
{
    if (whitespaceReset) {
        QByteArray result;
        int pos = 0;
        while (pos < input.size()) {
            const int eol = input.indexOf('\n', pos);
            const int lineEnd = (eol == -1) ? input.size() : eol;
            int contentEnd = lineEnd;
            if (contentEnd > pos && input.at(contentEnd - 1) == '\r')
                --contentEnd;
            const QByteArray segment = stripBase64Whitespace(input.mid(pos, contentEnd - pos));
            result += QByteArray::fromBase64(segment);
            result += input.mid(contentEnd, lineEnd - contentEnd);
            if (eol != -1) {
                result += '\n';
                pos = eol + 1;
            } else {
                pos = input.size();
            }
        }
        return result;
    }

    const QByteArray cleaned = stripBase64Whitespace(input);
    if (strict) {
        const auto decoded = QByteArray::fromBase64Encoding(
            cleaned, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
        if (decoded.decodingStatus != QByteArray::Base64DecodingStatus::Ok)
            return std::nullopt;
        return decoded.decoded;
    }
    return QByteArray::fromBase64(cleaned);
}

// Percent-encode following the MIME Tools methods, built on QUrl::toPercentEncoding.
// QUrl's unreserved set (ALPHA / DIGIT / "-" / "." / "_" / "~") is adjusted via
// exclude/include to match each method:
//  - Rfc1738: leave ! $ ' ( ) * + , unencoded, but force-encode "~" (unsafe per RFC 1738)
//  - Extended: force-encode "~" only (everything non-unreserved is already encoded)
//  - Full: force-encode the whole unreserved set so every byte is encoded
// byLine keeps CR/LF unencoded so that line structure is preserved.
static QByteArray urlEncodeData(const QByteArray &input, SciEdit::UrlEncoding method, bool byLine)
{
    const QString text = QString::fromUtf8(input);

    QByteArray exclude;
    QByteArray include;

    switch (method) {
    case SciEdit::UrlEncoding::Rfc1738:
        exclude = "!$'()*+,";
        include = "~";
        break;
    case SciEdit::UrlEncoding::Extended:
        include = "~";
        break;
    case SciEdit::UrlEncoding::Full:
        include = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
        break;
    }

    if (byLine)
        exclude += "\r\n";

    return QUrl::toPercentEncoding(text, exclude, include);
}

// Percent-decode. Returns nullopt on a malformed %XX escape (invalid hex or truncated).
static std::optional<QByteArray> urlDecodeData(const QByteArray &input)
{
    static const auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    };

    QByteArray out;
    out.reserve(input.size());
    int i = 0;
    while (i < input.size()) {
        const char c = input.at(i);
        if (c == '%') {
            if (i + 2 >= input.size())
                return std::nullopt;
            const int h1 = hexVal(input.at(i + 1));
            const int h2 = hexVal(input.at(i + 2));
            if (h1 < 0 || h2 < 0)
                return std::nullopt;
            out += static_cast<char>((h1 << 4) | h2);
            i += 3;
        } else {
            out += c;
            i += 1;
        }
    }
    return out;
}

void SciEdit::properCase()
{
    const int selCount = int(selections());
    if (selCount <= 0)
        return;
    beginUndoAction();
    for (int i = selCount - 1; i >= 0; --i) {
        const sptr_t start = selectionNStart(i);
        const sptr_t end = selectionNEnd(i);
        if (start == end)
            continue;
        const QByteArray bytes = get_text_range(int(start), int(end));
        QString s = QString::fromUtf8(bytes);
        properCaseConvert(s, false);
        setTargetRange(int(start), int(end));
        const QByteArray out = s.toUtf8();
        replaceTarget(out.size(), out.constData());
    }
    endUndoAction();
}

void SciEdit::properCaseBlend()
{
    const int selCount = int(selections());
    if (selCount <= 0)
        return;
    beginUndoAction();
    for (int i = selCount - 1; i >= 0; --i) {
        const sptr_t start = selectionNStart(i);
        const sptr_t end = selectionNEnd(i);
        if (start == end)
            continue;
        const QByteArray bytes = get_text_range(int(start), int(end));
        QString s = QString::fromUtf8(bytes);
        properCaseConvert(s, true);
        setTargetRange(int(start), int(end));
        const QByteArray out = s.toUtf8();
        replaceTarget(out.size(), out.constData());
    }
    endUndoAction();
}

void SciEdit::sentenceCase()
{
    const int selCount = int(selections());
    if (selCount <= 0)
        return;
    beginUndoAction();
    for (int i = selCount - 1; i >= 0; --i) {
        const sptr_t start = selectionNStart(i);
        const sptr_t end = selectionNEnd(i);
        if (start == end)
            continue;
        const QByteArray bytes = get_text_range(int(start), int(end));
        QString s = QString::fromUtf8(bytes);
        sentenceCaseConvert(s, false);
        setTargetRange(int(start), int(end));
        const QByteArray out = s.toUtf8();
        replaceTarget(out.size(), out.constData());
    }
    endUndoAction();
}

void SciEdit::sentenceCaseBlend()
{
    const int selCount = int(selections());
    if (selCount <= 0)
        return;
    beginUndoAction();
    for (int i = selCount - 1; i >= 0; --i) {
        const sptr_t start = selectionNStart(i);
        const sptr_t end = selectionNEnd(i);
        if (start == end)
            continue;
        const QByteArray bytes = get_text_range(int(start), int(end));
        QString s = QString::fromUtf8(bytes);
        sentenceCaseConvert(s, true);
        setTargetRange(int(start), int(end));
        const QByteArray out = s.toUtf8();
        replaceTarget(out.size(), out.constData());
    }
    endUndoAction();
}

void SciEdit::invertCase()
{
    const int selCount = int(selections());
    if (selCount <= 0)
        return;
    beginUndoAction();
    for (int i = selCount - 1; i >= 0; --i) {
        const sptr_t start = selectionNStart(i);
        const sptr_t end = selectionNEnd(i);
        if (start == end)
            continue;
        const QByteArray bytes = get_text_range(int(start), int(end));
        QString s = QString::fromUtf8(bytes);
        invertCaseConvert(s);
        setTargetRange(int(start), int(end));
        const QByteArray out = s.toUtf8();
        replaceTarget(out.size(), out.constData());
    }
    endUndoAction();
}

void SciEdit::randomCase()
{
    const int selCount = int(selections());
    if (selCount <= 0)
        return;
    beginUndoAction();
    for (int i = selCount - 1; i >= 0; --i) {
        const sptr_t start = selectionNStart(i);
        const sptr_t end = selectionNEnd(i);
        if (start == end)
            continue;
        const QByteArray bytes = get_text_range(int(start), int(end));
        QString s = QString::fromUtf8(bytes);
        randomCaseConvert(s);
        setTargetRange(int(start), int(end));
        const QByteArray out = s.toUtf8();
        replaceTarget(out.size(), out.constData());
    }
    endUndoAction();
}

// Applies a byte transform to every non-empty selection, replacing the selection
// with the transformed text. Selections are processed in reverse order so that
// position offsets remain valid. Returns false if any selection could not be
// transformed (its text is left untouched in that case).
bool SciEdit::transformSelections(const std::function<std::optional<QByteArray>(const QByteArray &)> &transform)
{
    const int selCount = int(selections());
    if (selCount <= 0)
        return true;

    bool allOk = true;
    beginUndoAction();
    for (int i = selCount - 1; i >= 0; --i) {
        const sptr_t start = selectionNStart(i);
        const sptr_t end = selectionNEnd(i);
        if (start == end)
            continue;
        const QByteArray bytes = get_text_range(int(start), int(end));
        const std::optional<QByteArray> result = transform(bytes);
        if (!result.has_value()) {
            allOk = false;
            continue;
        }
        setTargetRange(int(start), int(end));
        replaceTarget(result->size(), result->constData());
    }
    endUndoAction();
    return allOk;
}

void SciEdit::base64Encode(Base64Padding padding, bool byLine, int wrapLength)
{
    const bool withPadding = (padding == Base64Padding::WithPadding);
    transformSelections([withPadding, byLine, wrapLength](const QByteArray &input) {
        return std::optional<QByteArray>(base64EncodeData(input, withPadding, byLine, wrapLength));
    });
}

bool SciEdit::base64Decode(bool strict, bool whitespaceReset)
{
    return transformSelections([strict, whitespaceReset](const QByteArray &input) {
        return base64DecodeData(input, strict, whitespaceReset);
    });
}

void SciEdit::urlEncode(UrlEncoding method, bool byLine)
{
    transformSelections([method, byLine](const QByteArray &input) {
        return std::optional<QByteArray>(urlEncodeData(input, method, byLine));
    });
}

bool SciEdit::urlDecode()
{
    return transformSelections([](const QByteArray &input) {
        return urlDecodeData(input);
    });
}

void SciEdit::hexEncode(bool spaceSeparated)
{
    transformSelections([spaceSeparated](const QByteArray &input) {
        return std::optional<QByteArray>(spaceSeparated ? input.toHex(' ') : input.toHex());
    });
}

void SciEdit::hexDecode()
{
    transformSelections([](const QByteArray &input) {
        return std::optional<QByteArray>(QByteArray::fromHex(input));
    });
}

void SciEdit::clearLexerKeywords()
{
    static constexpr int maxKeywordSets = 9;
    for (int i = 0; i < maxKeywordSets; i++) {
        setKeyWords(i, "");
    }
}

void SciEdit::applyLexerKeywords()
{
    clearLexerKeywords();

    if (m_languageId.isEmpty()) {
        return;
    }

    const LanguageSpec *langSpec = LanguageConfigStore::languageSpec(m_languageId);
    if (!langSpec) {
        return;
    }

    for (auto itKw = langSpec->keywords.constBegin(); itKw != langSpec->keywords.constEnd(); ++itKw) {
        setKeyWords(itKw.key(), itKw.value().constData());
    }
}

void SciEdit::applyLexerProperties()
{
    if (m_languageId.isEmpty()) {
        return;
    }

    const LanguageSpec *langSpec = LanguageConfigStore::languageSpec(m_languageId);
    if (!langSpec) {
        return;
    }

    for (auto itProp = langSpec->properties.constBegin(); itProp != langSpec->properties.constEnd(); ++itProp) {
        setProperty(itProp.key().constData(), itProp.value().constData());
    }
}

void SciEdit::requestUrlScan()
{
    if (!m_urlScanEnabled) {
        return;
    }
    m_urlScanTriggered = true;
    if (m_urlScanSuspendCount > 0) {
        return;
    }
}

void SciEdit::beginUrlScanBatch()
{
    ++m_urlScanSuspendCount;
    m_urlScanForceOnResume = true;
}

void SciEdit::endUrlScanBatch()
{
    if (m_urlScanSuspendCount <= 0) {
        return;
    }
    --m_urlScanSuspendCount;
    if (m_urlScanSuspendCount == 0 && m_urlScanForceOnResume) {
        m_urlScanTriggered = true;
        m_urlScanForceOnResume = false;
    }
    scheduleUrlScanIfTriggered();
}

void SciEdit::setUrlScanEnabled(bool enabled)
{
    if (m_urlScanEnabled == enabled) {
        return;
    }
    m_urlScanEnabled = enabled;
    if (!m_urlScanEnabled) {
        m_urlScanTriggered = false;
        clearUrlIndicators();
    } else {
        requestUrlScan();
    }
}

bool SciEdit::urlScanEnabled() const
{
    return m_urlScanEnabled;
}

void SciEdit::setSpellCheckEnabled(bool enabled)
{
    if (!m_spellCheckController) {
        return;
    }
    m_spellCheckController->setEnabled(enabled);
}

bool SciEdit::spellCheckEnabled() const
{
    return m_spellCheckController && m_spellCheckController->enabled();
}

void SciEdit::refreshSpellChecker()
{
    if (!m_spellCheckController) {
        return;
    }
    m_spellCheckController->refreshSpeller();
}

bool SciEdit::styleNeedsSpellCheck(int styleId) const
{
    if (styleId < 0 || styleId >= int(m_styleNeedsSpellCheck.size())) {
        return false;
    }
    return m_styleNeedsSpellCheck[styleId];
}

void SciEdit::beginSpellCheckBatch()
{
    if (m_spellCheckController) {
        m_spellCheckController->suspendBatch();
    }
}

void SciEdit::endSpellCheckBatch()
{
    if (m_spellCheckController) {
        m_spellCheckController->resumeBatch();
    }
}

void SciEdit::clearUrlIndicators()
{
    setIndicatorCurrent(kUrlHoverIndicator);
    indicatorClearRange(0, length());
}

void SciEdit::scheduleUrlScanIfTriggered()
{
    if (!m_urlScanEnabled || m_urlScanSuspendCount > 0) {
        return;
    }
    if (!m_urlScanTriggered) {
        return;
    }
    m_urlScanTriggered = false;
    if (m_urlScanTimer) {
        m_urlScanTimer->start();
        return;
    }
    scanVisibleUrls();
}

void SciEdit::scanVisibleUrls()
{
    setIndicatorCurrent(kUrlHoverIndicator);

    const QRect viewRect = rect();
    sptr_t startPos = positionFromPoint(0, 0);
    if (startPos < 0) {
        return;
    }
    sptr_t line = lineFromPosition(startPos);
    startPos = positionFromLine(line);

    sptr_t endPos = positionFromPoint(viewRect.width(), viewRect.height());
    if (endPos < 0) {
        return;
    }
    line = lineFromPosition(endPos);
    endPos = lineEndPosition(line);

    if (endPos <= startPos) {
        return;
    }

    indicatorClearRange(startPos, endPos - startPos);

    const QByteArray bytes = get_text_range(int(startPos), int(endPos));
    const Utf8DecodedText decoded = decodeUtf8WithBytePositions(bytes);
    const int textLen = decoded.chars.size();
    if (textLen == 0) {
        return;
    }

    int index = 0;
    while (index < textLen) {
        int segmentLen = 0;
        const bool found = isUrl(decoded.chars, textLen, index, &segmentLen);
        if (segmentLen <= 0) {
            break;
        }
        if (found && index + segmentLen <= decoded.bytePositions.size() - 1) {
            const sptr_t fillStart = startPos + decoded.bytePositions.at(index);
            const sptr_t fillEnd = startPos + decoded.bytePositions.at(index + segmentLen);
            if (fillEnd > fillStart) {
                indicatorFillRange(fillStart, fillEnd - fillStart);
            }
        }
        index += segmentLen;
    }
}

void SciEdit::openUrlAtIndicatorPosition(sptr_t pos, Scintilla::KeyMod modifiers)
{
    if (!(int(modifiers) & int(Scintilla::KeyMod::Ctrl))) {
        return;
    }

    const int indicators = int(indicatorAllOnFor(pos));
    if (!(indicators & (1 << kUrlHoverIndicator))) {
        return;
    }

    const sptr_t indicatorStartPos = indicatorStart(kUrlHoverIndicator, pos);
    const sptr_t indicatorEndPos = indicatorEnd(kUrlHoverIndicator, pos);
    if (indicatorEndPos <= indicatorStartPos) {
        return;
    }

    const QByteArray bytes = get_text_range(int(indicatorStartPos), int(indicatorEndPos));
    const Utf8DecodedText decoded = decodeUtf8WithBytePositions(bytes);
    if (decoded.chars.isEmpty()) {
        return;
    }

    int len = decoded.chars.size();
    while (removeUnwantedTrailingCharFromUrl(decoded.chars, 0, &len)) {
    }
    if (len <= 0) {
        return;
    }

    QString candidate;
    candidate.reserve(len);
    for (int i = 0; i < len; ++i) {
        candidate.append(decoded.chars.at(i));
    }

    QUrl url(candidate, QUrl::TolerantMode);
    if (!url.isValid() || !isSupportedScheme(url.scheme())) {
        return;
    }

    QDesktopServices::openUrl(url);
}

void SciEdit::setViewNPC(bool visible)
{
    if (m_viewNPC == visible) {
        return;
    }
    m_viewNPC = visible;
    if (visible) {
        for (const auto& charSpec : g_nonPrintingChars) {
            setRepresentation(charSpec.bytes, charSpec.repr);
            setRepresentationAppearance(charSpec.bytes, SC_REPRESENTATION_BLOB);
        }
    } else {
        for (const auto& charSpec : g_nonPrintingChars) {
            clearRepresentation(charSpec.bytes);
        }
    }
}

bool SciEdit::viewNPC() const
{
    return m_viewNPC;
}

void SciEdit::setVerticalEdgeEnabled(bool enabled)
{
    setEdgeMode(enabled ? EDGE_LINE : EDGE_NONE);
}

bool SciEdit::verticalEdgeEnabled() const
{
    return edgeMode() != EDGE_NONE;
}

void SciEdit::setChangeHistoryEnabled(bool enabled)
{
    if (m_changeHistoryEnabled == enabled) {
        return;
    }
    m_changeHistoryEnabled = enabled;

    if (enabled) {
        setMarginTypeN(kChangeHistoryMargin, SC_MARGIN_SYMBOL);
        setMarginMaskN(kChangeHistoryMargin, SC_MASK_HISTORY);
        setMarginWidthN(kChangeHistoryMargin, 4);
        setMarginSensitiveN(kChangeHistoryMargin, 0);
        setChangeHistory(SC_CHANGE_HISTORY_ENABLED | SC_CHANGE_HISTORY_MARKERS);
    } else {
        setMarginWidthN(kChangeHistoryMargin, 0);
        setChangeHistory(SC_CHANGE_HISTORY_DISABLED);
    }
}

bool SciEdit::changeHistoryEnabled() const
{
    return m_changeHistoryEnabled;
}

void SciEdit::setChangeHistorySavePoint()
{
    if (m_changeHistoryEnabled) {
        setSavePoint();
    }
}

bool SciEdit::goToNextChange(bool wrap)
{
    if (!m_changeHistoryEnabled) {
        return false;
    }
    const sptr_t currentLine = lineFromPosition(currentPos());
    const sptr_t totalLines = lineCount();
    for (sptr_t line = currentLine + 1; line < totalLines; ++line) {
        const sptr_t level = foldLevel(line);
        if (level & SC_FOLDLEVELHEADERFLAG) {
            continue;
        }
        const sptr_t marker = markerGet(line);
        if (marker & SC_MASK_HISTORY) {
            gotoPos(positionFromLine(line));
            return true;
        }
    }
    if (wrap && currentLine > 0) {
        for (sptr_t line = 0; line <= currentLine; ++line) {
            const sptr_t level = foldLevel(line);
            if (level & SC_FOLDLEVELHEADERFLAG) {
                continue;
            }
            const sptr_t marker = markerGet(line);
            if (marker & SC_MASK_HISTORY) {
                gotoPos(positionFromLine(line));
                return true;
            }
        }
    }
    return false;
}

bool SciEdit::goToPreviousChange(bool wrap)
{
    if (!m_changeHistoryEnabled) {
        return false;
    }
    const sptr_t currentLine = lineFromPosition(currentPos());
    for (sptr_t line = currentLine - 1; line >= 0; --line) {
        const sptr_t level = foldLevel(line);
        if (level & SC_FOLDLEVELHEADERFLAG) {
            continue;
        }
        const sptr_t marker = markerGet(line);
        if (marker & SC_MASK_HISTORY) {
            gotoPos(positionFromLine(line));
            return true;
        }
    }
    const sptr_t totalLines = lineCount();
    if (wrap && currentLine < totalLines - 1) {
        for (sptr_t line = totalLines - 1; line >= currentLine; --line) {
            const sptr_t level = foldLevel(line);
            if (level & SC_FOLDLEVELHEADERFLAG) {
                continue;
            }
            const sptr_t marker = markerGet(line);
            if (marker & SC_MASK_HISTORY) {
                gotoPos(positionFromLine(line));
                return true;
            }
        }
    }
    return false;
}

void SciEdit::clearChangeHistory()
{
    if (!m_changeHistoryEnabled) {
        return;
    }
    emptyUndoBuffer();
    setSavePoint();
}

void SciEdit::setAutoIndentMode(AutoIndentMode mode)
{
    m_autoIndentMode = mode;
}

SciEdit::AutoIndentMode SciEdit::autoIndentMode() const
{
    return m_autoIndentMode;
}

void SciEdit::setIndentUseTabs(bool useTabs)
{
    m_indentUseTabs = useTabs;
    setUseTabs(useTabs);
}

bool SciEdit::indentUseTabs() const
{
    return m_indentUseTabs;
}

void SciEdit::autoIndentLine(int line)
{
    if (m_autoIndentMode == AutoIndentNone) {
        return;
    }

    const int eolMode = eOLMode();
    const int prevLine = line - 1;

    if (prevLine < 0) {
        return;
    }

    const sptr_t prevLineStart = positionFromLine(prevLine);
    const sptr_t prevLineEnd = lineEndPosition(prevLine);
    if (prevLineEnd - prevLineStart == 0) {
        return;
    }

    if (m_autoIndentMode == AutoIndentBasic) {
        const int prevIndent = lineIndentation(prevLine);
        if (prevIndent > 0) {
            insertIndentation(line, prevIndent);
        }
    } else if (m_autoIndentMode == AutoIndentAdvanced) {
        const sptr_t curPos = currentPos();
        const sptr_t prevCharPos = curPos - (eolMode == SC_EOL_CRLF ? 3 : 2);
        const int prevChar = charAt(prevCharPos);
        const int nextChar = charAt(curPos);

        autoIndentLineAdvanced(line, static_cast<wchar_t>(prevChar));

        if (nextChar == L'}') {
            const sptr_t matchPos = findMatchingBracePos(curPos, L'{', L'}');
            if (matchPos >= 0) {
                const int matchLine = lineFromPosition(matchPos);
                if (matchLine != line) {
                    const int matchIndent = lineIndentation(matchLine);
                    insertIndentation(line, matchIndent);
                }
            }
        }
    }
}

void SciEdit::autoIndentLineAdvanced(int line, wchar_t ch)
{
    const int prevLine = line - 1;
    if (prevLine < 0) {
        return;
    }

    const int prevIndent = lineIndentation(prevLine);
    const int tabSize = tabWidth();

    int newIndent = prevIndent;

    if (ch == L'{') {
        const int nextChar = charAt(currentPos());
        if (nextChar == L'}') {
            const int eolMode = eOLMode();
            const char* eolChars = (eolMode == SC_EOL_CRLF) ? "\r\n" : (eolMode == SC_EOL_LF) ? "\n" : "\r";
            insertText(currentPos(), eolChars);
            setLineIndentation(line + 1, prevIndent);
        }
        newIndent = prevIndent + tabSize;
    } else if (isConditionExprLine(prevLine)) {
        newIndent = prevIndent + tabSize;
    } else {
        if (prevIndent > 0 && prevLine > 0) {
            if (isConditionExprLine(prevLine - 1)) {
                const int grandParentIndent = lineIndentation(prevLine - 1);
                if (prevIndent >= tabSize) {
                    newIndent = prevIndent - tabSize;
                } else {
                    newIndent = grandParentIndent;
                }
            }
        }
    }

    insertIndentation(line, newIndent);
}

bool SciEdit::isConditionExprLine(int line)
{
    if (line < 0) {
        return false;
    }

    const sptr_t lineStart = positionFromLine(line);
    const sptr_t lineEnd = lineEndPosition(line);
    const sptr_t lineLen = lineEnd - lineStart;

    if (lineLen <= 0) {
        return false;
    }

    const QByteArray lineBytes = get_text_range(lineStart, lineEnd);
    QString lineText = QString::fromUtf8(lineBytes).trimmed();

    if (lineText.isEmpty()) {
        return false;
    }

    const QStringList conditionKeywords = {
        QStringLiteral("if"),
        QStringLiteral("else"),
        QStringLiteral("for"),
        QStringLiteral("while"),
        QStringLiteral("do"),
        QStringLiteral("switch"),
        QStringLiteral("catch"),
        QStringLiteral("foreach"),
        QStringLiteral("case"),
        QStringLiteral("default")
    };

    for (const QString& keyword : conditionKeywords) {
        if (lineText.startsWith(keyword + QLatin1Char('(')) ||
            lineText.startsWith(keyword + QLatin1Char(' ')) ||
            lineText == keyword) {
            return true;
        }
    }

    return false;
}

sptr_t SciEdit::findMatchingBracePos(sptr_t pos, char openBrace, char closeBrace) const
{
    int braceCount = 1;
    sptr_t currentPos = pos;

    while (currentPos >= 0 && braceCount > 0) {
        const int ch = charAt(currentPos);
        if (ch == closeBrace) {
            braceCount++;
        } else if (ch == openBrace) {
            braceCount--;
        }
        if (braceCount > 0) {
            currentPos--;
        }
    }

    return (braceCount == 0) ? currentPos : -1;
}

void SciEdit::insertIndentation(int line, int indentAmount)
{
    if (indentAmount < 0) {
        indentAmount = 0;
    }

    setLineIndentation(line, indentAmount);
    gotoPos(findColumn(line, indentAmount));
}

void SciEdit::highlightSelectedWord()
{
    if (m_findResultHighlightActive) {
        return;
    }

    setIndicatorCurrent(kSmartHighlightIndicator);
    indicatorClearRange(0, length());

    if (!m_smartHighlightEnabled || selectionEmpty()) {
        return;
    }

    const sptr_t curPos = currentPos();
    const sptr_t selStart = selectionStart();
    const sptr_t selEnd = selectionEnd();

    if (selStart == selEnd) {
        return;
    }

    if (m_smartHighlightWholeWord) {
        const sptr_t wordStart = wordStartPosition(curPos, true);
        const sptr_t wordEnd = wordEndPosition(wordStart, true);

        if (wordStart == wordEnd || wordStart != selStart || wordEnd != selEnd) {
            return;
        }
    } else {
        const sptr_t line = lineFromPosition(curPos);
        const sptr_t lineLen = lineLength(line);
        const sptr_t textLen = selEnd - selStart;
        if (textLen > lineLen) {
            return;
        }
    }

    const QByteArray selText = get_text_range(selStart, selEnd);
    if (selText.isEmpty()) {
        return;
    }

    static constexpr int kMaxHighlightLines = 400;

    const sptr_t firstLine = firstVisibleLine();
    const sptr_t visibleLineCount = linesOnScreen();
    const sptr_t maxLines = std::min(visibleLineCount, static_cast<sptr_t>(kMaxHighlightLines)) + 1;

    sptr_t prevDocLine = -1;

    for (sptr_t i = 0; i < maxLines; ++i) {
        const sptr_t docLine = docLineFromVisible(firstLine + i);
        if (docLine == prevDocLine) {
            continue;
        }
        prevDocLine = docLine;

        const sptr_t lineStart = positionFromLine(docLine);
        const sptr_t lineEnd = lineEndPosition(docLine);

        Sci_TextToFind ttf;
        ttf.chrg.cpMin = lineStart;
        ttf.chrg.cpMax = lineEnd;
        ttf.lpstrText = selText.constData();
        ttf.chrgText.cpMin = -1;
        ttf.chrgText.cpMax = -1;

        int flags = 0;
        if (m_smartHighlightMatchCase) {
            flags |= SCFIND_MATCHCASE;
        }
        if (m_smartHighlightWholeWord) {
            flags |= SCFIND_WHOLEWORD;
        }

        while (send(SCI_FINDTEXT, flags, reinterpret_cast<sptr_t>(&ttf)) != -1) {
            if (ttf.chrgText.cpMin >= 0 && ttf.chrgText.cpMax > ttf.chrgText.cpMin) {
                indicatorFillRange(ttf.chrgText.cpMin, ttf.chrgText.cpMax - ttf.chrgText.cpMin);
            }
            if (ttf.chrgText.cpMax >= lineEnd) {
                break;
            }
            ttf.chrg.cpMin = ttf.chrgText.cpMax;
        }
    }
}

void SciEdit::clearSmartHighlight()
{
    setIndicatorCurrent(kSmartHighlightIndicator);
    indicatorClearRange(0, length());
}

void SciEdit::setSmartHighlightEnabled(bool enabled)
{
    m_smartHighlightEnabled = enabled;
    if (!enabled) {
        clearSmartHighlight();
    }
}

bool SciEdit::smartHighlightEnabled() const
{
    return m_smartHighlightEnabled;
}

void SciEdit::setSmartHighlightMatchCase(bool matchCase)
{
    m_smartHighlightMatchCase = matchCase;
}

bool SciEdit::smartHighlightMatchCase() const
{
    return m_smartHighlightMatchCase;
}

void SciEdit::setSmartHighlightWholeWord(bool wholeWord)
{
    m_smartHighlightWholeWord = wholeWord;
}

bool SciEdit::smartHighlightWholeWord() const
{
    return m_smartHighlightWholeWord;
}

void SciEdit::setFindResultHighlightEnabled(bool enabled)
{
    m_findResultHighlightEnabled = enabled;
    if (!enabled) {
        clearFindResultHighlight();
    }
}

bool SciEdit::findResultHighlightEnabled() const
{
    return m_findResultHighlightEnabled;
}

void SciEdit::highlightFindResults(const QByteArray &text, int flags)
{
    clearFindResultHighlight();

    if (!m_findResultHighlightEnabled) {
        return;
    }

    clearSmartHighlight();

    m_findResultHighlightActive = true;

    if (text.isEmpty()) {
        return;
    }

    setIndicatorCurrent(kFindResultIndicator);

    Sci_TextToFind ttf;
    ttf.chrg.cpMin = 0;
    ttf.chrg.cpMax = static_cast<Sci_PositionCR>(length());
    ttf.lpstrText = text.constData();
    ttf.chrgText.cpMin = -1;
    ttf.chrgText.cpMax = -1;

    while (send(SCI_FINDTEXT, flags, reinterpret_cast<sptr_t>(&ttf)) != -1) {
        if (ttf.chrgText.cpMin >= 0 && ttf.chrgText.cpMax > ttf.chrgText.cpMin) {
            indicatorFillRange(ttf.chrgText.cpMin, ttf.chrgText.cpMax - ttf.chrgText.cpMin);
        }
        if (ttf.chrgText.cpMax >= ttf.chrg.cpMax) {
            break;
        }
        // Zero-length match protection: advance by one character to avoid infinite loop
        // when regex produces zero-length matches (e.g. $, ^, \b)
        if (ttf.chrgText.cpMax == ttf.chrgText.cpMin) {
            ttf.chrg.cpMin = ttf.chrgText.cpMin + 1;
        } else {
            ttf.chrg.cpMin = ttf.chrgText.cpMax;
        }
    }
}

void SciEdit::clearFindResultHighlight()
{
    setIndicatorCurrent(kFindResultIndicator);
    indicatorClearRange(0, length());
    m_findResultHighlightActive = false;
}

void SciEdit::setSkipNextSmartHighlight(bool skip)
{
    m_findResultHighlightActive = skip;
}

void SciEdit::highlightBraceMatch()
{
    const sptr_t curPos = currentPos();
    const sptr_t docLen = length();
    
    sptr_t braceAt = -1;
    sptr_t matchPos = -1;
    
    if (curPos > 0) {
        const int chBefore = static_cast<int>(charAt(curPos - 1));
        if (chBefore == '(' || chBefore == ')' || chBefore == '[' || chBefore == ']' ||
            chBefore == '{' || chBefore == '}') {
            braceAt = curPos - 1;
            matchPos = braceMatch(braceAt, 0);
        }
    }
    
    if (braceAt < 0 && curPos < docLen) {
        const int chAfter = static_cast<int>(charAt(curPos));
        if (chAfter == '(' || chAfter == ')' || chAfter == '[' || chAfter == ']' ||
            chAfter == '{' || chAfter == '}') {
            braceAt = curPos;
            matchPos = braceMatch(braceAt, 0);
        }
    }
    
    if (braceAt >= 0 && matchPos >= 0) {
        braceHighlight(braceAt, matchPos);
    } else if (braceAt >= 0) {
        braceBadLight(braceAt);
    } else {
        braceHighlight(-1, -1);
    }
}

bool SciEdit::isXmlTagLexer() const
{
    return m_languageId == QLatin1String("xml") ||
           m_languageId == QLatin1String("hypertext");
}

struct TagInfo {
    sptr_t tagStart;      // position of '<'
    sptr_t nameStart;     // position after '<' or '</'
    sptr_t nameEnd;       // position after the tag name
    sptr_t tagEnd;        // position of '>'
    bool isClosing;       // true for </tag>
    QByteArray name;      // tag name bytes
};

static sptr_t findTagEnd(SciEdit *editor, sptr_t from)
{
    // Find the closing '>' of a tag, handling quoted attribute values
    const sptr_t docLen = editor->length();
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    for (sptr_t p = from; p < docLen; ++p) {
        const char ch = static_cast<char>(editor->charAt(p));
        if (ch == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
        } else if (ch == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
        } else if (ch == '>' && !inSingleQuote && !inDoubleQuote) {
            return p;
        }
    }
    return -1;
}

static std::optional<TagInfo> parseTagAt(SciEdit *editor, sptr_t pos)
{
    // Find the '<' that starts the tag containing 'pos'
    const sptr_t docLen = editor->length();

    // Search backward for '<' that is not inside a string/attribute
    sptr_t tagStart = -1;
    bool inDoubleQuote = false;
    bool inSingleQuote = false;

    // First scan forward from pos to understand quote state at this point
    // We need to find the opening '<' of the tag we're inside.
    // Walk backward to find '<', being careful about quotes.
    // Simple approach: walk backward looking for '<', then forward for '>'.

    // Find the nearest '<' before or at pos
    sptr_t p = pos;
    while (p >= 0) {
        const char ch = static_cast<char>(editor->charAt(p));
        if (ch == '<') {
            tagStart = p;
            break;
        }
        if (ch == '>') {
            // We've gone past a tag boundary; not inside a tag
            return std::nullopt;
        }
        if (p == 0) break;
        --p;
    }

    if (tagStart < 0) {
        return std::nullopt;
    }

    // Check if this is a valid tag start
    sptr_t namePos = tagStart + 1;
    if (namePos >= docLen) return std::nullopt;

    bool isClosing = false;
    if (static_cast<char>(editor->charAt(namePos)) == '/') {
        isClosing = true;
        ++namePos;
    }

    if (namePos >= docLen) return std::nullopt;

    // Check first char of tag name (must be letter or underscore)
    {
        const char firstCh = static_cast<char>(editor->charAt(namePos));
        if (!QChar::fromLatin1(firstCh).isLetter() && firstCh != '_' && firstCh != ':') {
            return std::nullopt;
        }
    }

    // Read the tag name
    sptr_t nameStart = namePos;
    sptr_t nameEnd = namePos;
    while (nameEnd < docLen) {
        const char ch = static_cast<char>(editor->charAt(nameEnd));
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '/' || ch == '>') {
            break;
        }
        ++nameEnd;
    }

    if (nameEnd == nameStart) return std::nullopt;

    QByteArray name = editor->get_text_range(nameStart, nameEnd);
    // Tag names shouldn't contain certain characters
    if (name.contains('<') || name.contains('>')) return std::nullopt;

    // Find the tag end
    sptr_t tagEnd = findTagEnd(editor, tagStart + 1);
    if (tagEnd < 0) return std::nullopt;

    // Make sure pos is within [tagStart, tagEnd]
    if (pos > tagEnd) return std::nullopt;

    return TagInfo{tagStart, nameStart, nameEnd, tagEnd, isClosing, name};
}

void SciEdit::clearTagMatchHighlight()
{
    setIndicatorCurrent(kTagMatchIndicator);
    indicatorClearRange(0, length());
}

void SciEdit::highlightTagMatch()
{
    if (!m_tagMatchHighlightEnabled || !isXmlTagLexer()) {
        return;
    }

    setIndicatorCurrent(kTagMatchIndicator);
    indicatorClearRange(0, length());

    const sptr_t curPos = currentPos();

    // Try to parse the tag at current position
    auto tagOpt = parseTagAt(this, curPos);
    if (!tagOpt.has_value()) {
        return;
    }

    const TagInfo &currentTag = tagOpt.value();

    // Skip self-closing tags, comments, CDATA, DOCTYPE, processing instructions
    {
        const QByteArray &name = currentTag.name;
        if (name.startsWith('!') || name.startsWith('?')) {
            return;
        }
        // Check if self-closing: ends with />
        {
            sptr_t p = currentTag.tagEnd - 1;
            while (p > currentTag.nameEnd) {
                const char ch = static_cast<char>(charAt(p));
                if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                    --p;
                    continue;
                }
                if (ch == '/') {
                    // Self-closing tag
                    return;
                }
                break;
            }
        }
    }

    const QByteArray &tagName = currentTag.name;
    const sptr_t docLen = length();

    // Helper: highlight a tag region — whole tag when attributes are on,
    // or tag-name portion plus the closing '>' when attributes are off.
    auto fillTagHighlight = [&](const TagInfo &tag) {
        if (m_tagMatchHighlightAttributes) {
            indicatorFillRange(tag.tagStart, tag.tagEnd + 1 - tag.tagStart);
        } else {
            indicatorFillRange(tag.tagStart, tag.nameEnd - tag.tagStart);
            indicatorFillRange(tag.tagEnd, 1);
        }
    };

    if (currentTag.isClosing) {
        // Current tag is </tag> - find the matching opening <tag>
        int depth = 1;
        sptr_t searchPos = currentTag.tagStart - 1;

        while (searchPos >= 0) {
            // Find previous '<'
            sptr_t prevLt = -1;
            for (sptr_t i = searchPos; i >= 0; --i) {
                if (static_cast<char>(charAt(i)) == '<') {
                    prevLt = i;
                    break;
                }
            }
            if (prevLt < 0) break;

            auto prevTagOpt = parseTagAt(this, prevLt);
            if (!prevTagOpt.has_value()) {
                searchPos = prevLt - 1;
                continue;
            }

            const TagInfo &prevTag = prevTagOpt.value();

            // Skip comments, CDATA, etc.
            if (prevTag.name.startsWith('!') || prevTag.name.startsWith('?')) {
                searchPos = prevLt - 1;
                continue;
            }

            // Check if self-closing
            bool selfClosing = false;
            {
                sptr_t p = prevTag.tagEnd - 1;
                while (p > prevTag.nameEnd) {
                    const char ch = static_cast<char>(charAt(p));
                    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                        --p;
                        continue;
                    }
                    if (ch == '/') {
                        selfClosing = true;
                    }
                    break;
                }
            }

            if (selfClosing) {
                searchPos = prevLt - 1;
                continue;
            }

            if (prevTag.name == tagName) {
                if (prevTag.isClosing) {
                    ++depth;
                } else {
                    --depth;
                    if (depth == 0) {
                        // Found matching opening tag
                        fillTagHighlight(prevTag);
                        fillTagHighlight(currentTag);
                        return;
                    }
                }
            }
            searchPos = prevLt - 1;
        }
    } else {
        // Current tag is <tag> - find the matching closing </tag>
        int depth = 1;
        sptr_t searchPos = currentTag.tagEnd + 1;

        while (searchPos < docLen) {
            // Find next '<'
            sptr_t nextLt = -1;
            for (sptr_t i = searchPos; i < docLen; ++i) {
                if (static_cast<char>(charAt(i)) == '<') {
                    nextLt = i;
                    break;
                }
            }
            if (nextLt < 0) break;

            auto nextTagOpt = parseTagAt(this, nextLt);
            if (!nextTagOpt.has_value()) {
                searchPos = nextLt + 1;
                continue;
            }

            const TagInfo &nextTag = nextTagOpt.value();

            // Skip comments, CDATA, etc.
            if (nextTag.name.startsWith('!') || nextTag.name.startsWith('?')) {
                searchPos = nextTag.tagEnd + 1;
                continue;
            }

            // Check if self-closing
            bool selfClosing = false;
            {
                sptr_t p = nextTag.tagEnd - 1;
                while (p > nextTag.nameEnd) {
                    const char ch = static_cast<char>(charAt(p));
                    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                        --p;
                        continue;
                    }
                    if (ch == '/') {
                        selfClosing = true;
                    }
                    break;
                }
            }

            if (selfClosing) {
                searchPos = nextTag.tagEnd + 1;
                continue;
            }

            if (nextTag.name == tagName) {
                if (!nextTag.isClosing) {
                    ++depth;
                } else {
                    --depth;
                    if (depth == 0) {
                        // Found matching closing tag
                        fillTagHighlight(currentTag);
                        fillTagHighlight(nextTag);
                        return;
                    }
                }
            }
            searchPos = nextTag.tagEnd + 1;
        }
    }
}

void SciEdit::setTagMatchHighlightEnabled(bool enabled)
{
    m_tagMatchHighlightEnabled = enabled;
    if (!enabled) {
        clearTagMatchHighlight();
    }
}

bool SciEdit::tagMatchHighlightEnabled() const
{
    return m_tagMatchHighlightEnabled;
}

void SciEdit::setTagMatchHighlightAttributes(bool attributes)
{
    m_tagMatchHighlightAttributes = attributes;
}

bool SciEdit::tagMatchHighlightAttributes() const
{
    return m_tagMatchHighlightAttributes;
}

void SciEdit::startRecord()
{
    m_macroRecording = true;
    ScintillaEdit::startRecord();
}

void SciEdit::stopRecord()
{
    ScintillaEdit::stopRecord();
    m_macroRecording = false;
}

bool SciEdit::isMacroRecording() const
{
    return m_macroRecording;
}

void SciEdit::setMacroPlayback(bool playback)
{
    m_macroPlayback = playback;
}

bool SciEdit::isMacroPlayingBack() const
{
    return m_macroPlayback;
}

void SciEdit::setAutoCompletionEnabled(bool enabled)
{
    m_autoCompletionEnabled = enabled;
    if (!enabled && autoCActive()) {
        autoCCancel();
    }
}

bool SciEdit::autoCompletionEnabled() const
{
    return m_autoCompletionEnabled;
}

void SciEdit::setAutoCompletionTriggerLength(int length)
{
    m_autoCompletionTriggerLength = qBound(1, length, 9);
}

int SciEdit::autoCompletionTriggerLength() const
{
    return m_autoCompletionTriggerLength;
}

void SciEdit::setAutoCompletionIgnoreNumbers(bool ignore)
{
    m_autoCompletionIgnoreNumbers = ignore;
}

bool SciEdit::autoCompletionIgnoreNumbers() const
{
    return m_autoCompletionIgnoreNumbers;
}

void SciEdit::setAutoCompletionBrief(bool brief)
{
    m_autoCompletionBrief = brief;
}

bool SciEdit::autoCompletionBrief() const
{
    return m_autoCompletionBrief;
}

void SciEdit::setIsLargeFile(bool isLarge)
{
    m_isLargeFile = isLarge;
    if (isLarge && autoCActive()) {
        autoCCancel();
    }
}

bool SciEdit::isLargeFile() const
{
    return m_isLargeFile;
}

void SciEdit::setColumnSel2MultiEdit(bool enabled)
{
    m_columnSel2MultiEdit = enabled;
}

bool SciEdit::columnSel2MultiEdit() const
{
    return m_columnSel2MultiEdit;
}

void SciEdit::setMultiSelection(bool enabled)
{
    setMultipleSelection(enabled);
    setAdditionalSelectionTyping(enabled);
}

void SciEdit::setColumnSelectMode(bool enabled)
{
    m_columnSelectMode = enabled;
}

bool SciEdit::columnSelectMode() const
{
    return m_columnSelectMode;
}

bool SciEdit::shouldShowAutoCompletion() const
{
    if (!m_autoCompletionEnabled) {
        return false;
    }
    if (m_isLargeFile) {
        return false;
    }
    if (selectionEmpty()) {
        return true;
    }
    return false;
}

void SciEdit::showAutoCompletion()
{
    if (!shouldShowAutoCompletion()) {
        return;
    }

    if (!m_autoCompletionBrief && autoCActive()) {
        return;
    }

    const sptr_t curPos = currentPos();
    const sptr_t wordStart = wordStartPosition(curPos, true);
    const sptr_t wordEnd = wordEndPosition(wordStart, true);

    const sptr_t wordLen = curPos - wordStart;
    if (wordLen < m_autoCompletionTriggerLength) {
        return;
    }

    if (wordStart == wordEnd) {
        return;
    }

    const QByteArray currentWord = get_text_range(wordStart, curPos);
    if (currentWord.isEmpty()) {
        return;
    }

    QSet<QByteArray> words;

    auto addWord = [this, &words](const QByteArray &word) {
        if (word.isEmpty() || word.size() < m_autoCompletionTriggerLength) {
            return;
        }
        if (m_autoCompletionIgnoreNumbers) {
            bool isNumber = true;
            for (int i = 0; i < word.size(); ++i) {
                const char c = word.at(i);
                if (!((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E' || c == 'x' || c == 'X' || c == 'a' || c == 'A' || c == 'b' || c == 'B' || c == 'c' || c == 'C' || c == 'd' || c == 'D' || c == 'f' || c == 'F')) {
                    isNumber = false;
                    break;
                }
            }
            if (isNumber) {
                return;
            }
        }
        words.insert(word);
    };

    const sptr_t docLen = length();
    const sptr_t docStart = 0;

    Sci_TextToFind ttf;
    ttf.chrg.cpMin = docStart;
    ttf.chrg.cpMax = curPos - wordLen;
    ttf.lpstrText = currentWord.constData();
    ttf.chrgText.cpMin = -1;
    ttf.chrgText.cpMax = -1;

    QByteArray pattern = currentWord;
    pattern.append("[a-zA-Z0-9_]*");

    ttf.lpstrText = pattern.constData();
    int flags = SCFIND_WORDSTART;

    ttf.lpstrText = currentWord.constData();
    flags = 0;

    while (send(SCI_FINDTEXT, flags, reinterpret_cast<sptr_t>(&ttf)) != -1) {
        if (ttf.chrgText.cpMin >= 0 && ttf.chrgText.cpMax > ttf.chrgText.cpMin) {
            const sptr_t matchStart = ttf.chrgText.cpMin;
            const sptr_t matchEnd = wordEndPosition(matchStart, true);
            if (matchEnd > matchStart) {
                const QByteArray matchWord = get_text_range(matchStart, matchEnd);
                if (matchWord != currentWord) {
                    addWord(matchWord);
                }
            }
        }
        if (ttf.chrgText.cpMax >= ttf.chrg.cpMax) {
            break;
        }
        ttf.chrg.cpMin = ttf.chrgText.cpMax;
    }

    ttf.chrg.cpMin = curPos;
    ttf.chrg.cpMax = docLen;
    ttf.lpstrText = currentWord.constData();

    while (send(SCI_FINDTEXT, flags, reinterpret_cast<sptr_t>(&ttf)) != -1) {
        if (ttf.chrgText.cpMin >= 0 && ttf.chrgText.cpMax > ttf.chrgText.cpMin) {
            const sptr_t matchStart = ttf.chrgText.cpMin;
            const sptr_t matchEnd = wordEndPosition(matchStart, true);
            if (matchEnd > matchStart) {
                const QByteArray matchWord = get_text_range(matchStart, matchEnd);
                if (matchWord != currentWord) {
                    addWord(matchWord);
                }
            }
        }
        if (ttf.chrgText.cpMax >= ttf.chrg.cpMax) {
            break;
        }
        ttf.chrg.cpMin = ttf.chrgText.cpMax;
    }

    if (words.isEmpty()) {
        return;
    }

    QList<QByteArray> sortedWords = words.values();
    std::sort(sortedWords.begin(), sortedWords.end());

    QByteArray wordList;
    for (const QByteArray &w : sortedWords) {
        if (!wordList.isEmpty()) {
            wordList.append(' ');
        }
        wordList.append(w);
    }

    autoCSetSeparator(' ');
    autoCSetIgnoreCase(true);
    autoCSetOrder(SC_ORDER_PERFORMSORT);
    autoCSetMaxHeight(10);
    autoCShow(static_cast<int>(wordLen), wordList.constData());
}
