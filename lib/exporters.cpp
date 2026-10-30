#include "exporters.h"

#include "sciedit.h"

#include <Scintilla.h>

#include <QColor>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QTextStream>

using namespace Qt::Literals::StringLiterals;

namespace {

QColor colorFromScintilla(sptr_t color)
{
    return QColor(static_cast<int>(color & 0xFF),
                  static_cast<int>((color >> 8) & 0xFF),
                  static_cast<int>((color >> 16) & 0xFF));
}

// Scintilla exposes document bytes through a signed char interface, so mask
// the value first when a byte value is needed.
inline int byteAt(SciEdit *editor, int pos)
{
    return static_cast<int>(editor->charAt(pos) & 0xFF);
}

inline int styleAt(SciEdit *editor, int pos)
{
    return static_cast<int>(editor->styleAt(pos) & 0xFF);
}

inline int halfPointsFromFractional(sptr_t fractionalSize)
{
    // SC_FONT_SIZE_MULTIPLIER is 100; RTF \fs takes half points.
    return static_cast<int>((fractionalSize + 50) / 100);
}

// Decodes the UTF-8 sequence starting at byte position pos.
// Returns the code point, or -1 when the sequence is invalid (in that case
// only a single byte is consumed). byteCount is set to the bytes consumed.
int decodeUtf8Sequence(SciEdit *editor, int pos, int end, int &byteCount)
{
    byteCount = 1;

    const int first = byteAt(editor, pos);
    if (first < 0x80) {
        return first;
    }

    int sequenceLength = 0;
    int codePoint = 0;
    if ((first & 0xE0) == 0xC0) {
        sequenceLength = 2;
        codePoint = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
        sequenceLength = 3;
        codePoint = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
        sequenceLength = 4;
        codePoint = first & 0x07;
    } else {
        return -1;
    }

    if (pos + sequenceLength > end) {
        return -1;
    }

    for (int i = 1; i < sequenceLength; ++i) {
        const int b = byteAt(editor, pos + i);
        if ((b & 0xC0) != 0x80) {
            return -1;
        }
        codePoint = (codePoint << 6) | (b & 0x3F);
    }
    byteCount = sequenceLength;

    const bool overlong = (sequenceLength == 2 && codePoint < 0x80)
        || (sequenceLength == 3 && codePoint < 0x800)
        || (sequenceLength == 4 && codePoint < 0x10000);
    if (overlong || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        return -1;
    }

    return codePoint;
}

// RTF \uN uses a signed 16-bit value.
inline int signedRtfCodeUnit(ushort value)
{
    return static_cast<int>(static_cast<short>(value));
}

QList<int> collectUsedStyles(SciEdit *editor, int start, int end)
{
    QList<int> usedStyles;
    QSet<int> seen;
    for (int i = start; i < end; ++i) {
        const int style = styleAt(editor, i);
        if (!seen.contains(style)) {
            seen.insert(style);
            usedStyles.append(style);
        }
    }
    return usedStyles;
}

// SciEdit stores fonts as "Family/StyleName"; exporters only want the family.
QByteArray fontFamilyBytes(SciEdit *editor, int style)
{
    QByteArray name = editor->styleFont(style);
    const int slash = name.indexOf('/');
    if (slash >= 0) {
        name = name.left(slash);
    }
    return name;
}

} // namespace

Exporter::Exporter(SciEdit *editor)
    : m_editor(editor)
{
}

Exporter::~Exporter() = default;

void Exporter::convert(QTextStream &stream)
{
    convertRange(stream, 0, static_cast<int>(m_editor->length()));
}

void Exporter::convertRange(QTextStream &stream, int start, int end)
{
    if (!m_editor) {
        return;
    }

    const int length = static_cast<int>(m_editor->length());
    start = qMax(0, start);
    end = qMin(end, length);
    if (end < start) {
        end = start;
    }

    writeRange(stream, start, end);
}

void Exporter::ensureDocumentStyled(int start, int end)
{
    if (!m_editor) {
        return;
    }
    m_editor->colourise(start, end);
}

void RtfExporter::writeRange(QTextStream &stream, int start, int end)
{
    ensureDocumentStyled(start, end);
    const bool isUtf8 = (m_editor->codePage() == SC_CP_UTF8);

    struct RtfStyleInfo
    {
        int fontIndex = 0;
        int halfPoints = 20;
        int backIndex = 0;
        int foreIndex = 0;
        bool bold = false;
        bool italic = false;
        bool underline = false;
    };

    QList<QByteArray> fonts;
    QList<QColor> colors;

    auto fontIndexOf = [&fonts](const QByteArray &name) {
        const int index = fonts.indexOf(name);
        if (index >= 0) {
            return index;
        }
        fonts.append(name);
        return static_cast<int>(fonts.size()) - 1;
    };
    auto colorIndexOf = [&colors](const QColor &color) {
        const int index = colors.indexOf(color);
        if (index >= 0) {
            return index;
        }
        colors.append(color);
        return static_cast<int>(colors.size()) - 1;
    };

    RtfStyleInfo defaultInfo;
    defaultInfo.fontIndex = 0;
    fonts.append(fontFamilyBytes(m_editor, STYLE_DEFAULT));
    defaultInfo.halfPoints = halfPointsFromFractional(m_editor->styleSizeFractional(STYLE_DEFAULT));
    defaultInfo.backIndex = colorIndexOf(colorFromScintilla(m_editor->styleBack(STYLE_DEFAULT)));
    defaultInfo.foreIndex = colorIndexOf(colorFromScintilla(m_editor->styleFore(STYLE_DEFAULT)));
    defaultInfo.bold = m_editor->styleBold(STYLE_DEFAULT);
    defaultInfo.italic = m_editor->styleItalic(STYLE_DEFAULT);
    defaultInfo.underline = m_editor->styleUnderline(STYLE_DEFAULT);

    const QList<int> usedStyles = collectUsedStyles(m_editor, start, end);
    QHash<int, RtfStyleInfo> styleInfos;
    for (int style : usedStyles) {
        RtfStyleInfo info;
        const QByteArray font = fontFamilyBytes(m_editor, style);
        info.fontIndex = font.isEmpty() ? 0 : fontIndexOf(font);
        info.halfPoints = halfPointsFromFractional(m_editor->styleSizeFractional(style));
        info.backIndex = colorIndexOf(colorFromScintilla(m_editor->styleBack(style)));
        info.foreIndex = colorIndexOf(colorFromScintilla(m_editor->styleFore(style)));
        info.bold = m_editor->styleBold(style);
        info.italic = m_editor->styleItalic(style);
        info.underline = m_editor->styleUnderline(style);
        styleInfos.insert(style, info);
    }

    // RTF font names are byte strings; escape structural characters and
    // emit non-ASCII bytes as \'xx escapes.
    auto escapedRtfFontName = [](const QByteArray &name) {
        QString result;
        result.reserve(name.size());
        for (char c : name) {
            const int b = static_cast<uchar>(c);
            if (c == '{' || c == '}' || c == '\\') {
                result += u'\\';
                result += QChar::fromLatin1(c);
            } else if (b < 0x80) {
                result += QChar::fromLatin1(c);
            } else {
                result += u"\\'"_s + QString::number(b, 16).rightJustified(2, u'0');
            }
        }
        return result;
    };

    stream << "{\\rtf1\\ansi\\deff0\\deftab" << QString::number(static_cast<int>(m_editor->tabWidth()) * 120) << "\r\n";
    stream << "{\\fonttbl\r\n";
    for (int i = 0; i < fonts.size(); ++i) {
        stream << "{\\f" << QString::number(i) << ' ' << escapedRtfFontName(fonts.at(i)) << ";}\r\n";
    }
    stream << "}\r\n";
    stream << "{\\colortbl\r\n";
    for (const QColor &color : colors) {
        stream << "\\red" << QString::number(color.red())
               << "\\green" << QString::number(color.green())
               << "\\blue" << QString::number(color.blue()) << ";\r\n";
    }
    stream << "}\r\n";

    QString run;
    auto flushRun = [&stream, &run]() {
        if (!run.isEmpty()) {
            stream << run;
            run.clear();
        }
    };

    auto writeStyleSwitch = [&stream](const RtfStyleInfo &previous, const RtfStyleInfo &current) {
        QString switches;
        if (current.fontIndex != previous.fontIndex) {
            switches += u"\\f"_s + QString::number(current.fontIndex);
        }
        if (current.halfPoints != previous.halfPoints) {
            switches += u"\\fs"_s + QString::number(current.halfPoints);
        }
        if (current.backIndex != previous.backIndex) {
            switches += u"\\highlight"_s + QString::number(current.backIndex);
        }
        if (current.foreIndex != previous.foreIndex) {
            switches += u"\\cf"_s + QString::number(current.foreIndex);
        }
        if (current.bold != previous.bold) {
            switches += current.bold ? u"\\b"_s : u"\\b0"_s;
        }
        if (current.italic != previous.italic) {
            switches += current.italic ? u"\\i"_s : u"\\i0"_s;
        }
        if (current.underline != previous.underline) {
            switches += current.underline ? u"\\ul"_s : u"\\ul0"_s;
        }
        if (!switches.isEmpty()) {
            stream << switches << ' ';
        }
    };

    // Emit the default style up front (sentinel values force the font size and
    // colors to be written explicitly), then diff against it on every change.
    RtfStyleInfo initial;
    initial.fontIndex = -1;
    initial.halfPoints = -1;
    initial.backIndex = -1;
    initial.foreIndex = -1;
    writeStyleSwitch(initial, defaultInfo);

    RtfStyleInfo previousInfo = defaultInfo;
    int previousStyle = -1;
    int i = start;
    while (i < end) {
        const int style = styleAt(m_editor, i);
        if (style != previousStyle) {
            const RtfStyleInfo currentInfo = styleInfos.value(style, defaultInfo);
            flushRun();
            writeStyleSwitch(previousInfo, currentInfo);
            previousInfo = currentInfo;
            previousStyle = style;
        }

        const int b = byteAt(m_editor, i);
        switch (b) {
        case '\r':
        case '\n': {
            const bool isCrlf = (b == '\r' && i + 1 < end && byteAt(m_editor, i + 1) == '\n');
            i += isCrlf ? 2 : 1;
            flushRun();
            stream << "\\par\r\n";
            break;
        }
        case '\t':
            run += u"\\tab "_s;
            ++i;
            break;
        case '{':
            run += u"\\{"_s;
            ++i;
            break;
        case '}':
            run += u"\\}"_s;
            ++i;
            break;
        case '\\':
            run += u"\\\\"_s;
            ++i;
            break;
        default: {
            if (b < 0x20) {
                // Skip control characters that have no RTF representation.
                ++i;
                break;
            }
            if (b < 0x80) {
                run += QChar::fromLatin1(b);
                ++i;
                break;
            }

            bool written = false;
            if (isUtf8) {
                int byteCount = 1;
                const int codePoint = decodeUtf8Sequence(m_editor, i, end, byteCount);
                if (codePoint >= 0) {
                    if (codePoint <= 0xFFFF) {
                        run += u"\\u"_s + QString::number(signedRtfCodeUnit(static_cast<ushort>(codePoint))) + u"?"_s;
                    } else {
                        const uint ucs4 = static_cast<uint>(codePoint);
                        const ushort high = QChar::highSurrogate(ucs4);
                        const ushort low = QChar::lowSurrogate(ucs4);
                        run += u"\\u"_s + QString::number(signedRtfCodeUnit(high)) + u"?"_s;
                        run += u"\\u"_s + QString::number(signedRtfCodeUnit(low)) + u"?"_s;
                    }
                    i += byteCount;
                    written = true;
                }
            }
            if (!written) {
                // Not a valid UTF-8 sequence (or an 8-bit code page document):
                // keep the raw byte as a hex escape.
                run += u"\\'"_s + QString::number(b, 16).rightJustified(2, u'0');
                ++i;
            }
            break;
        }
        }
    }
    flushRun();
    stream << "}\r\n";
    stream.flush();
}

HtmlExporter::HtmlExporter(SciEdit *editor, const QString &documentTitle)
    : Exporter(editor)
    , m_documentTitle(documentTitle)
{
}

void HtmlExporter::writeRange(QTextStream &stream, int start, int end)
{
    ensureDocumentStyled(start, end);

    const QColor defaultFore = colorFromScintilla(m_editor->styleFore(STYLE_DEFAULT));
    const QColor defaultBack = colorFromScintilla(m_editor->styleBack(STYLE_DEFAULT));
    const QString defaultFont = QString::fromUtf8(fontFamilyBytes(m_editor, STYLE_DEFAULT));
    const int defaultHalfPoints = halfPointsFromFractional(m_editor->styleSizeFractional(STYLE_DEFAULT));
    const bool defaultBold = m_editor->styleBold(STYLE_DEFAULT);
    const bool defaultItalic = m_editor->styleItalic(STYLE_DEFAULT);
    const bool defaultUnderline = m_editor->styleUnderline(STYLE_DEFAULT);

    auto cssColor = [](const QColor &color) {
        return u"rgb("_s + QString::number(color.red()) + u", "_s + QString::number(color.green())
            + u", "_s + QString::number(color.blue()) + u')';
    };
    auto pointsFromHalf = [](int halfPoints) {
        if (halfPoints % 2 == 0) {
            return QString::number(halfPoints / 2);
        }
        return QString::number(halfPoints / 2.0, 'f', 1);
    };
    auto escapedCssFont = [](const QString &font) {
        QString escaped = font;
        escaped.remove(u'\'');
        escaped.remove(u'\\');
        return escaped;
    };

    const QString title = m_documentTitle.isEmpty() ? u"Exported from Pineapple Notepad"_s : m_documentTitle;
    const QString escapedTitle = title.toHtmlEscaped();

    // Header and styles
    stream << "<!DOCTYPE html>\r\n";
    stream << "<html>\r\n";
    stream << "<head>\r\n";
    stream << "<meta charset=\"utf-8\">\r\n";
    stream << "<title>" << escapedTitle << "</title>\r\n";
    stream << "<style type=\"text/css\">\r\n";
    stream << "pre {\r\n";
    stream << "  font-family: '" << escapedCssFont(defaultFont) << "', monospace;\r\n";
    stream << "  font-size: " << pointsFromHalf(defaultHalfPoints) << "pt;\r\n";
    stream << "  color: " << cssColor(defaultFore) << ";\r\n";
    stream << "  background-color: " << cssColor(defaultBack) << ";\r\n";
    stream << "  tab-size: " << QString::number(static_cast<int>(m_editor->tabWidth())) << ";\r\n";
    stream << "  -moz-tab-size: " << QString::number(static_cast<int>(m_editor->tabWidth())) << ";\r\n";
    stream << "  white-space: pre;\r\n";
    stream << "  line-height: 1;\r\n";
    stream << "  margin: 0;\r\n";
    stream << "}\r\n";

    const QList<int> usedStyles = collectUsedStyles(m_editor, start, end);
    for (int style : usedStyles) {
        const QColor fore = colorFromScintilla(m_editor->styleFore(style));
        const QColor back = colorFromScintilla(m_editor->styleBack(style));
        const QString font = QString::fromUtf8(fontFamilyBytes(m_editor, style));
        const int halfPoints = halfPointsFromFractional(m_editor->styleSizeFractional(style));
        const bool bold = m_editor->styleBold(style);
        const bool italic = m_editor->styleItalic(style);
        const bool underline = m_editor->styleUnderline(style);

        QStringList lines;
        if (font != defaultFont && !font.isEmpty()) {
            lines << u"  font-family: '"_s + escapedCssFont(font) + u"', monospace;"_s;
        }
        if (halfPoints != defaultHalfPoints) {
            lines << u"  font-size: "_s + pointsFromHalf(halfPoints) + u"pt;"_s;
        }
        if (fore != defaultFore) {
            lines << u"  color: "_s + cssColor(fore) + u';';
        }
        if (back != defaultBack) {
            lines << u"  background-color: "_s + cssColor(back) + u';';
        }
        if (bold != defaultBold) {
            lines << (bold ? u"  font-weight: bold;"_s : u"  font-weight: normal;"_s);
        }
        if (italic != defaultItalic) {
            lines << (italic ? u"  font-style: italic;"_s : u"  font-style: normal;"_s);
        }
        if (underline != defaultUnderline) {
            lines << (underline ? u"  text-decoration: underline;"_s : u"  text-decoration: none;"_s);
        }
        if (lines.isEmpty()) {
            continue;
        }
        stream << ".s" << QString::number(style) << " {\r\n";
        for (const QString &line : lines) {
            stream << line << "\r\n";
        }
        stream << "}\r\n";
    }
    stream << "</style>\r\n";
    stream << "</head>\r\n";
    stream << "<body>\r\n";
    stream << "<pre class=\"source\">";

    // Body
    QString run;
    auto flushRun = [&stream, &run]() {
        if (!run.isEmpty()) {
            stream << run;
            run.clear();
        }
    };

    int previousStyle = -1;
    bool spanOpen = false;
    int i = start;
    while (i < end) {
        const int style = styleAt(m_editor, i);
        if (style != previousStyle) {
            previousStyle = style;
            flushRun();
            if (spanOpen) {
                stream << "</span>";
            }
            stream << "<span class=\"s" << QString::number(style) << "\">";
            spanOpen = true;
        }

        const int b = byteAt(m_editor, i);
        switch (b) {
        case '\r':
        case '\n': {
            const bool isCrlf = (b == '\r' && i + 1 < end && byteAt(m_editor, i + 1) == '\n');
            i += isCrlf ? 2 : 1;
            run += u'\n';
            break;
        }
        case '\t':
            run += u'\t';
            ++i;
            break;
        case '<':
            run += u"&lt;"_s;
            ++i;
            break;
        case '>':
            run += u"&gt;"_s;
            ++i;
            break;
        case '&':
            run += u"&amp;"_s;
            ++i;
            break;
        default: {
            if (b < 0x20) {
                // Skip control characters without a visible representation.
                ++i;
                break;
            }
            if (b < 0x80) {
                run += QChar::fromLatin1(b);
                ++i;
                break;
            }

            int byteCount = 1;
            const int codePoint = decodeUtf8Sequence(m_editor, i, end, byteCount);
            if (codePoint >= 0) {
                if (codePoint <= 0xFFFF) {
                    run += QChar(static_cast<ushort>(codePoint));
                } else {
                    const uint ucs4 = static_cast<uint>(codePoint);
                    run += QChar(QChar::highSurrogate(ucs4));
                    run += QChar(QChar::lowSurrogate(ucs4));
                }
                i += byteCount;
            } else {
                run += QChar(0xFFFD);
                ++i;
            }
            break;
        }
        }
    }
    flushRun();
    if (spanOpen) {
        stream << "</span>";
    }
    stream << "</pre>\r\n";
    stream << "</body>\r\n";
    stream << "</html>\r\n";
    stream.flush();
}
