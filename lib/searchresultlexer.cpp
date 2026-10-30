#include "searchresultlexer.h"

#include <Scintilla.h>

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace pineapple {

namespace {

enum class LineKind {
    SearchHeader,
    FileHeader,
    Result,
};

// Fold levels, mirroring Notepad++'s enum in FindReplaceDlg.h /
// LexSearchResult.cxx: search headers fold everything below them, file
// headers fold their result lines, result lines are leaves.
constexpr int kSearchHeaderLevel = SC_FOLDLEVELBASE;
constexpr int kFileHeaderLevel = SC_FOLDLEVELBASE + 1;
constexpr int kResultLevel = SC_FOLDLEVELBASE + 2;

LineKind classifyLine(const std::string &text, size_t lineStart, size_t lineEnd)
{
    if (lineStart >= lineEnd) {
        return LineKind::Result;
    }
    const char first = text[lineStart];
    if (first == ' ') {
        return LineKind::FileHeader;
    }
    if (first == '\t') {
        return LineKind::Result;
    }
    return LineKind::SearchHeader;
}

bool isEolChar(char ch)
{
    return ch == '\r' || ch == '\n';
}

// Clamps an API-provided styling range to the document and aligns its start
// to a line boundary. Returns false for an empty range.
bool normalizeRange(Scintilla::IDocument *pAccess, Sci_Position &start, Sci_Position &end)
{
    const Sci_Position docLength = pAccess->Length();
    if (docLength <= 0 || start >= docLength || end <= start) {
        return false;
    }
    if (start < 0) {
        start = 0;
    }
    start = pAccess->LineStart(pAccess->LineFromPosition(start));
    if (start >= docLength) {
        return false;
    }
    if (end > docLength) {
        end = docLength;
    }
    return end > start;
}

void styleResultLine(const std::string &text,
                     const SearchResultMarkingLine &marking,
                     size_t lineStart,
                     size_t lineEnd,
                     size_t rangeStart,
                     std::vector<char> &styles)
{
    // Layout: "\tLine 42: matched text...". The whitespace prefix keeps the
    // default style; "Line 42:" (up to and including the colon) gets the
    // line-number style; the remainder is default with the marked segments
    // highlighted.
    size_t i = lineStart;
    while (i < lineEnd && (text[i] == ' ' || text[i] == '\t')) {
        ++i;
    }
    size_t contentStart = i;
    const size_t colon = text.find(':', i);
    if (colon != std::string::npos && colon < lineEnd) {
        contentStart = colon + 1;
        for (size_t p = i; p <= colon; ++p) {
            styles[p - rangeStart] = SearchResultStyleLineNumber;
        }
    }

    const int lineLength = static_cast<int>(lineEnd - lineStart);
    for (const SearchResultMarkingSegment &segment : marking.segments) {
        int start = segment.start;
        int end = segment.end;
        if (start < 0) {
            start = 0;
        }
        if (end > lineLength) {
            end = lineLength;
        }
        for (int p = start; p < end; ++p) {
            const size_t abs = lineStart + static_cast<size_t>(p);
            if (abs >= lineEnd) {
                break;
            }
            if (abs >= contentStart) {
                styles[abs - rangeStart] = SearchResultStyleWordToSearch;
            }
        }
    }
}

} // namespace

void SearchResultLexer::setMarkings(const std::vector<SearchResultMarkingLine> *markings)
{
    markings_ = markings;
}

int SCI_METHOD SearchResultLexer::Version() const
{
    return Scintilla::lvRelease5;
}

void SCI_METHOD SearchResultLexer::Release()
{
    --refCount_;
    if (refCount_ <= 0) {
        delete this;
    }
}

const char *SCI_METHOD SearchResultLexer::PropertyNames()
{
    return "";
}

int SCI_METHOD SearchResultLexer::PropertyType(const char *)
{
    return 0;
}

const char *SCI_METHOD SearchResultLexer::DescribeProperty(const char *)
{
    return "";
}

Sci_Position SCI_METHOD SearchResultLexer::PropertySet(const char *, const char *)
{
    return -1;
}

const char *SCI_METHOD SearchResultLexer::DescribeWordListSets()
{
    return "";
}

Sci_Position SCI_METHOD SearchResultLexer::WordListSet(int, const char *)
{
    return -1;
}

void SCI_METHOD SearchResultLexer::Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int,
                                       Scintilla::IDocument *pAccess)
{
    if (!pAccess) {
        return;
    }

    Sci_Position start = static_cast<Sci_Position>(startPos);
    Sci_Position end = start + lengthDoc;
    if (!normalizeRange(pAccess, start, end)) {
        return;
    }

    std::string text(static_cast<size_t>(end - start), '\0');
    pAccess->GetCharRange(text.data(), start, end - start);

    std::vector<char> styles(text.size(), static_cast<char>(SearchResultStyleDefault));

    size_t lineStart = 0;
    while (lineStart < text.size()) {
        size_t lineEnd = lineStart;
        while (lineEnd < text.size() && !isEolChar(text[lineEnd])) {
            ++lineEnd;
        }

        const size_t absLineStart = static_cast<size_t>(start) + lineStart;
        switch (classifyLine(text, lineStart, lineEnd)) {
        case LineKind::SearchHeader:
            for (size_t p = lineStart; p < lineEnd; ++p) {
                styles[p] = SearchResultStyleSearchHeader;
            }
            break;
        case LineKind::FileHeader:
            for (size_t p = lineStart; p < lineEnd; ++p) {
                styles[p] = SearchResultStyleFileHeader;
            }
            break;
        case LineKind::Result:
            if (markings_) {
                const Sci_Position line = pAccess->LineFromPosition(
                    static_cast<Sci_Position>(absLineStart));
                if (line >= 0 && static_cast<size_t>(line) < markings_->size()) {
                    styleResultLine(text, (*markings_)[static_cast<size_t>(line)],
                                    lineStart, lineEnd, 0, styles);
                }
                // Lines without marking data (headers, or markings not yet
                // grown to this line) simply keep the default style.
            }
            break;
        }

        if (lineEnd >= text.size()) {
            break;
        }
        // Skip the EOL run ("\r\n" counts as one line break).
        lineStart = lineEnd + 1;
        if (lineStart < text.size() && text[lineEnd] == '\r' && text[lineStart] == '\n') {
            ++lineStart;
        }
    }

    pAccess->StartStyling(start);
    pAccess->SetStyles(end - start, styles.data());
}

void SCI_METHOD SearchResultLexer::Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int,
                                        Scintilla::IDocument *pAccess)
{
    if (!pAccess) {
        return;
    }

    Sci_Position start = static_cast<Sci_Position>(startPos);
    Sci_Position end = start + lengthDoc;
    if (!normalizeRange(pAccess, start, end)) {
        return;
    }

    std::string text(static_cast<size_t>(end - start), '\0');
    pAccess->GetCharRange(text.data(), start, end - start);

    Sci_Position line = pAccess->LineFromPosition(start);

    size_t lineStart = 0;
    while (lineStart < text.size()) {
        size_t lineEnd = lineStart;
        while (lineEnd < text.size() && !isEolChar(text[lineEnd])) {
            ++lineEnd;
        }

        int level;
        switch (classifyLine(text, lineStart, lineEnd)) {
        case LineKind::SearchHeader:
            level = kSearchHeaderLevel | SC_FOLDLEVELHEADERFLAG;
            break;
        case LineKind::FileHeader:
            level = kFileHeaderLevel | SC_FOLDLEVELHEADERFLAG;
            break;
        default:
            level = kResultLevel;
            break;
        }
        pAccess->SetLevel(line, level);
        ++line;

        if (lineEnd >= text.size()) {
            break;
        }
        lineStart = lineEnd + 1;
        if (lineStart < text.size() && text[lineEnd] == '\r' && text[lineStart] == '\n') {
            ++lineStart;
        }
    }
}

void *SCI_METHOD SearchResultLexer::PrivateCall(int, void *)
{
    return nullptr;
}

int SCI_METHOD SearchResultLexer::LineEndTypesSupported()
{
    return 0;
}

int SCI_METHOD SearchResultLexer::AllocateSubStyles(int, int)
{
    return -1;
}

int SCI_METHOD SearchResultLexer::SubStylesStart(int)
{
    return -1;
}

int SCI_METHOD SearchResultLexer::SubStylesLength(int)
{
    return 0;
}

int SCI_METHOD SearchResultLexer::StyleFromSubStyle(int subStyle)
{
    return subStyle;
}

int SCI_METHOD SearchResultLexer::PrimaryStyleFromStyle(int style)
{
    return style;
}

void SCI_METHOD SearchResultLexer::FreeSubStyles()
{
}

void SCI_METHOD SearchResultLexer::SetIdentifiers(int, const char *)
{
}

int SCI_METHOD SearchResultLexer::DistanceToSecondaryStyles()
{
    return 0;
}

const char *SCI_METHOD SearchResultLexer::GetSubStyleBases()
{
    return "";
}

int SCI_METHOD SearchResultLexer::NamedStyles()
{
    return 5;
}

const char *SCI_METHOD SearchResultLexer::NameOfStyle(int style)
{
    static constexpr std::array<const char *, 5> names = {
        "Default",
        "SearchHeader",
        "FileHeader",
        "LineNumber",
        "WordToSearch"
    };
    if (style < 0 || style >= static_cast<int>(names.size())) {
        return "";
    }
    return names[static_cast<size_t>(style)];
}

const char *SCI_METHOD SearchResultLexer::TagsOfStyle(int)
{
    return "";
}

const char *SCI_METHOD SearchResultLexer::DescriptionOfStyle(int style)
{
    return NameOfStyle(style);
}

const char *SCI_METHOD SearchResultLexer::GetName()
{
    return "searchResult";
}

int SCI_METHOD SearchResultLexer::GetIdentifier()
{
    return 0;
}

const char *SCI_METHOD SearchResultLexer::PropertyGet(const char *)
{
    return "";
}

SearchResultLexer *createSearchResultLexer()
{
    return new SearchResultLexer();
}

} // namespace pineapple
