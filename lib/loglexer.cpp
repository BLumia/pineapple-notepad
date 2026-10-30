#include "loglexer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace {

enum LogStyles {
    SCE_LOG_DEFAULT = 0,
    SCE_LOG_DATETIME = 1,
    SCE_LOG_LEVEL = 2,
    SCE_LOG_STRING = 3,
    SCE_LOG_NUMBER = 4,
    SCE_LOG_NAMESPACE = 5,
    SCE_LOG_FUNCTION = 6,
    SCE_LOG_SYMBOL = 7,
    SCE_LOG_SPECIAL = 8,
    SCE_LOG_PATH = 9
};

constexpr int kPriorityDefault = 0;
constexpr int kPriorityNumber = 10;
constexpr int kPriorityNamespace = 20;
constexpr int kPriorityFunction = 30;
constexpr int kPriorityLevel = 40;
constexpr int kPrioritySpecial = 50;
constexpr int kPriorityDateTime = 60;
constexpr int kPriorityPath = 70;
constexpr int kPriorityString = 80;

void applyRange(
    std::vector<char> &styles,
    std::vector<unsigned char> &priorities,
    const size_t start,
    const size_t end,
    const char style,
    const unsigned char priority)
{
    if (start >= end || start >= styles.size()) {
        return;
    }

    const size_t safeEnd = std::min(end, styles.size());
    for (size_t i = start; i < safeEnd; ++i) {
        if (priority >= priorities[i]) {
            styles[i] = style;
            priorities[i] = priority;
        }
    }
}

bool isAsciiAlpha(const char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool isAsciiDigit(const char ch)
{
    return ch >= '0' && ch <= '9';
}

bool isWordStart(const char ch)
{
    return isAsciiAlpha(ch) || ch == '_';
}

bool isWordChar(const char ch)
{
    return isWordStart(ch) || isAsciiDigit(ch);
}

bool isHexDigitChar(const char ch)
{
    return isAsciiDigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

bool isBoundaryBefore(const std::string &line, const size_t index)
{
    return index == 0 || !isWordChar(line[index - 1]);
}

bool isBoundaryAfter(const std::string &line, const size_t indexAfterToken)
{
    return indexAfterToken >= line.size() || !isWordChar(line[indexAfterToken]);
}

char toUpperAscii(const char ch)
{
    if (ch >= 'a' && ch <= 'z') {
        return static_cast<char>(ch - 'a' + 'A');
    }
    return ch;
}

bool equalsIgnoreCaseAscii(const std::string &line, const size_t start, const char *text)
{
    size_t i = 0;
    while (text[i] != '\0') {
        if (start + i >= line.size()) {
            return false;
        }
        if (toUpperAscii(line[start + i]) != toUpperAscii(text[i])) {
            return false;
        }
        ++i;
    }
    return true;
}

size_t parseIdentifierChain(const std::string &line, const size_t start)
{
    if (start >= line.size() || !isWordStart(line[start])) {
        return 0;
    }

    size_t i = start + 1;
    while (i < line.size() && isWordChar(line[i])) {
        ++i;
    }

    size_t segments = 1;
    while (i < line.size()) {
        size_t separatorLength = 0;
        if (line[i] == '.') {
            separatorLength = 1;
        } else if (line[i] == ':' && i + 1 < line.size() && line[i + 1] == ':') {
            separatorLength = 2;
        } else {
            break;
        }
        const size_t next = i + separatorLength;
        if (next >= line.size() || !isWordStart(line[next])) {
            break;
        }
        i = next + 1;
        while (i < line.size() && isWordChar(line[i])) {
            ++i;
        }
        ++segments;
    }

    if (segments < 2) {
        return 0;
    }
    return i - start;
}

bool isPathLeadBoundary(const std::string &line, const size_t index)
{
    if (index == 0) {
        return true;
    }
    const char prev = line[index - 1];
    return std::isspace(static_cast<unsigned char>(prev)) != 0
        || prev == '('
        || prev == '['
        || prev == '{';
}

bool isPathTerminator(const char ch)
{
    return std::isspace(static_cast<unsigned char>(ch)) != 0
        || ch == '"'
        || ch == '\''
        || ch == '<'
        || ch == '>';
}

size_t parseWindowsPathLength(const std::string &line, const size_t start)
{
    if (start + 3 > line.size()) {
        return 0;
    }
    if (!isAsciiAlpha(line[start]) || line[start + 1] != ':' || line[start + 2] != '\\') {
        return 0;
    }

    size_t i = start + 3;
    bool hasChar = false;
    while (i < line.size()) {
        const char ch = line[i];
        if (isPathTerminator(ch) || ch == '|' || ch == '*' || ch == '?' || ch == ':') {
            break;
        }
        if (ch == '\\') {
            ++i;
            continue;
        }
        hasChar = true;
        ++i;
    }
    if (!hasChar) {
        return 0;
    }
    return i - start;
}

size_t parseHomePathLength(const std::string &line, const size_t start)
{
    if (start + 2 > line.size() || line[start] != '~' || line[start + 1] != '/') {
        return 0;
    }
    size_t i = start + 2;
    bool hasChar = false;
    while (i < line.size()) {
        const char ch = line[i];
        if (isPathTerminator(ch) || ch == '|') {
            break;
        }
        hasChar = true;
        ++i;
    }
    if (!hasChar) {
        return 0;
    }
    return i - start;
}

size_t parseUnixAbsolutePathLength(const std::string &line, const size_t start)
{
    if (start >= line.size() || line[start] != '/') {
        return 0;
    }

    size_t i = start + 1;
    bool hasSegment = false;
    bool inSegment = false;
    while (i < line.size()) {
        const char ch = line[i];
        if (isPathTerminator(ch) || ch == '"' || ch == '\'' || ch == '|') {
            break;
        }
        if (ch == '/') {
            if (!inSegment) {
                break;
            }
            inSegment = false;
            ++i;
            continue;
        }
        if (ch == '<' || ch == '>') {
            break;
        }
        inSegment = true;
        hasSegment = true;
        ++i;
    }
    if (!hasSegment) {
        return 0;
    }
    return i - start;
}

void applyStringRanges(
    const std::string &line,
    const size_t lineStartOffset,
    std::vector<char> &styles,
    std::vector<unsigned char> &priorities)
{
    size_t i = 0;
    while (i < line.size()) {
        const char ch = line[i];
        if (ch != '"') {
            ++i;
            continue;
        }

        const char quote = ch;
        size_t j = i + 1;
        while (j < line.size()) {
            if (line[j] == '\\') {
                if (j + 1 < line.size()) {
                    j += 2;
                } else {
                    ++j;
                }
                continue;
            }
            if (line[j] == quote) {
                ++j;
                break;
            }
            ++j;
        }

        applyRange(
            styles,
            priorities,
            lineStartOffset + i,
            lineStartOffset + j,
            static_cast<char>(SCE_LOG_STRING),
            static_cast<unsigned char>(kPriorityString));

        i = j;
    }
}

void applyFunctionAndSymbolRanges(
    const std::string &line,
    const size_t lineStartOffset,
    std::vector<char> &styles,
    std::vector<unsigned char> &priorities)
{
    size_t i = 0;
    while (i < line.size()) {
        if (!isWordStart(line[i]) || !isBoundaryBefore(line, i)) {
            ++i;
            continue;
        }
        const size_t tokenStart = i;
        while (i < line.size() && isWordChar(line[i])) {
            ++i;
        }
        while (i < line.size()) {
            if (line[i] == '.') {
                const size_t next = i + 1;
                if (next < line.size() && isWordStart(line[next])) {
                    i = next + 1;
                    while (i < line.size() && isWordChar(line[i])) {
                        ++i;
                    }
                    continue;
                }
            } else if (line[i] == ':' && i + 1 < line.size() && line[i + 1] == ':') {
                const size_t next = i + 2;
                if (next < line.size() && isWordStart(line[next])) {
                    i = next + 1;
                    while (i < line.size() && isWordChar(line[i])) {
                        ++i;
                    }
                    continue;
                }
            }
            break;
        }

        if (i >= line.size() || line[i] != '(') {
            continue;
        }

        const size_t nameStart = lineStartOffset + tokenStart;
        const size_t nameEnd = lineStartOffset + i;
        const size_t openParen = lineStartOffset + i;
        applyRange(
            styles,
            priorities,
            nameStart,
            nameEnd,
            static_cast<char>(SCE_LOG_FUNCTION),
            static_cast<unsigned char>(kPriorityFunction));
        applyRange(
            styles,
            priorities,
            openParen,
            openParen + 1,
            static_cast<char>(SCE_LOG_SYMBOL),
            static_cast<unsigned char>(kPriorityFunction));

        size_t depth = 1;
        size_t j = i + 1;
        while (j < line.size()) {
            const char ch = line[j];
            if (ch == '"') {
                ++j;
                while (j < line.size()) {
                    if (line[j] == '\\') {
                        if (j + 1 < line.size()) {
                            j += 2;
                        } else {
                            ++j;
                        }
                        continue;
                    }
                    if (line[j] == '"') {
                        ++j;
                        break;
                    }
                    ++j;
                }
                continue;
            }
            if (ch == '(') {
                applyRange(
                    styles,
                    priorities,
                    lineStartOffset + j,
                    lineStartOffset + j + 1,
                    static_cast<char>(SCE_LOG_SYMBOL),
                    static_cast<unsigned char>(kPriorityFunction));
                ++depth;
            } else if (ch == ')') {
                applyRange(
                    styles,
                    priorities,
                    lineStartOffset + j,
                    lineStartOffset + j + 1,
                    static_cast<char>(SCE_LOG_SYMBOL),
                    static_cast<unsigned char>(kPriorityFunction));
                if (depth == 0) {
                    break;
                }
                --depth;
                if (depth == 0) {
                    ++j;
                    break;
                }
            } else if (ch == ',' && depth > 0) {
                applyRange(
                    styles,
                    priorities,
                    lineStartOffset + j,
                    lineStartOffset + j + 1,
                    static_cast<char>(SCE_LOG_SYMBOL),
                    static_cast<unsigned char>(kPriorityFunction));
            }
            ++j;
        }
        i = j;
    }
}

void applyNumberRanges(
    const std::string &line,
    const size_t lineStartOffset,
    std::vector<char> &styles,
    std::vector<unsigned char> &priorities)
{
    size_t i = 0;
    while (i < line.size()) {
        if (!isAsciiDigit(line[i])) {
            ++i;
            continue;
        }
        if (!isBoundaryBefore(line, i)) {
            ++i;
            continue;
        }

        size_t end = i;
        if (line[i] == '0' && i + 2 <= line.size() && (line[i + 1] == 'x' || line[i + 1] == 'X')) {
            size_t j = i + 2;
            while (j < line.size() && isHexDigitChar(line[j])) {
                ++j;
            }
            if (j > i + 2 && isBoundaryAfter(line, j)) {
                end = j;
            }
        }
        if (end == i) {
            size_t j = i;
            while (j < line.size() && isAsciiDigit(line[j])) {
                ++j;
            }
            if (j < line.size() && line[j] == '.' && j + 1 < line.size() && isAsciiDigit(line[j + 1])) {
                ++j;
                while (j < line.size() && isAsciiDigit(line[j])) {
                    ++j;
                }
            }
            if (isBoundaryAfter(line, j)) {
                end = j;
            }
        }
        if (end > i) {
            applyRange(
                styles,
                priorities,
                lineStartOffset + i,
                lineStartOffset + end,
                static_cast<char>(SCE_LOG_NUMBER),
                static_cast<unsigned char>(kPriorityNumber));
            i = end;
        } else {
            ++i;
        }
    }
}

void applyDateTimeRanges(
    const std::string &line,
    const size_t lineStartOffset,
    std::vector<char> &styles,
    std::vector<unsigned char> &priorities)
{
    auto parseIso8601TimestampLength = [&](const size_t start) -> size_t {
        if (start + 19 > line.size()) {
            return 0;
        }
        if (!(isAsciiDigit(line[start])
              && isAsciiDigit(line[start + 1])
              && isAsciiDigit(line[start + 2])
              && isAsciiDigit(line[start + 3])
              && line[start + 4] == '-'
              && isAsciiDigit(line[start + 5])
              && isAsciiDigit(line[start + 6])
              && line[start + 7] == '-'
              && isAsciiDigit(line[start + 8])
              && isAsciiDigit(line[start + 9])
              && (line[start + 10] == 'T' || line[start + 10] == 't')
              && isAsciiDigit(line[start + 11])
              && isAsciiDigit(line[start + 12])
              && line[start + 13] == ':'
              && isAsciiDigit(line[start + 14])
              && isAsciiDigit(line[start + 15])
              && line[start + 16] == ':'
              && isAsciiDigit(line[start + 17])
              && isAsciiDigit(line[start + 18]))) {
            return 0;
        }

        size_t end = start + 19;
        if (end < line.size() && line[end] == '.') {
            size_t j = end + 1;
            size_t fractionDigits = 0;
            while (j < line.size() && isAsciiDigit(line[j]) && fractionDigits < 9) {
                ++j;
                ++fractionDigits;
            }
            if (fractionDigits > 0) {
                end = j;
            }
        }

        if (end < line.size() && (line[end] == 'Z' || line[end] == 'z')) {
            ++end;
        } else if (end + 6 <= line.size()
                   && (line[end] == '+' || line[end] == '-')
                   && isAsciiDigit(line[end + 1])
                   && isAsciiDigit(line[end + 2])
                   && line[end + 3] == ':'
                   && isAsciiDigit(line[end + 4])
                   && isAsciiDigit(line[end + 5])) {
            end += 6;
        }

        return end - start;
    };

    size_t i = 0;
    while (i < line.size()) {
        const size_t isoLength = parseIso8601TimestampLength(i);
        if (isoLength > 0
            && isBoundaryBefore(line, i)
            && isBoundaryAfter(line, i + isoLength)) {
            applyRange(
                styles,
                priorities,
                lineStartOffset + i,
                lineStartOffset + i + isoLength,
                static_cast<char>(SCE_LOG_DATETIME),
                static_cast<unsigned char>(kPriorityDateTime));
            i += isoLength;
            continue;
        }

        if (i + 10 <= line.size()
            && isAsciiDigit(line[i])
            && isAsciiDigit(line[i + 1])
            && isAsciiDigit(line[i + 2])
            && isAsciiDigit(line[i + 3])
            && line[i + 4] == '-'
            && isAsciiDigit(line[i + 5])
            && isAsciiDigit(line[i + 6])
            && line[i + 7] == '-'
            && isAsciiDigit(line[i + 8])
            && isAsciiDigit(line[i + 9])
            && isBoundaryBefore(line, i)
            && isBoundaryAfter(line, i + 10)) {
            applyRange(
                styles,
                priorities,
                lineStartOffset + i,
                lineStartOffset + i + 10,
                static_cast<char>(SCE_LOG_DATETIME),
                static_cast<unsigned char>(kPriorityDateTime));
        }

        if (i + 8 <= line.size()
            && isAsciiDigit(line[i])
            && isAsciiDigit(line[i + 1])
            && line[i + 2] == ':'
            && isAsciiDigit(line[i + 3])
            && isAsciiDigit(line[i + 4])
            && line[i + 5] == ':'
            && isAsciiDigit(line[i + 6])
            && isAsciiDigit(line[i + 7])
            && isBoundaryBefore(line, i)) {
            size_t end = i + 8;
            if (end < line.size() && line[end] == '.') {
                size_t j = end + 1;
                size_t digits = 0;
                while (j < line.size() && isAsciiDigit(line[j]) && digits < 6) {
                    ++j;
                    ++digits;
                }
                if (digits > 0) {
                    end = j;
                }
            }
            if (isBoundaryAfter(line, end)) {
                applyRange(
                    styles,
                    priorities,
                    lineStartOffset + i,
                    lineStartOffset + end,
                    static_cast<char>(SCE_LOG_DATETIME),
                    static_cast<unsigned char>(kPriorityDateTime));
            }
        }
        ++i;
    }
}

void applyNamespaceRanges(
    const std::string &line,
    const size_t lineStartOffset,
    std::vector<char> &styles,
    std::vector<unsigned char> &priorities)
{
    size_t i = 0;
    while (i < line.size()) {
        if (!isWordStart(line[i]) || !isBoundaryBefore(line, i)) {
            ++i;
            continue;
        }
        const size_t length = parseIdentifierChain(line, i);
        if (length > 0 && isBoundaryAfter(line, i + length)) {
            applyRange(
                styles,
                priorities,
                lineStartOffset + i,
                lineStartOffset + i + length,
                static_cast<char>(SCE_LOG_NAMESPACE),
                static_cast<unsigned char>(kPriorityNamespace));
            i += length;
            continue;
        }
        ++i;
    }
}

void applyLevelRanges(
    const std::string &line,
    const size_t lineStartOffset,
    std::vector<char> &styles,
    std::vector<unsigned char> &priorities)
{
    static constexpr std::array<const char *, 9> levels = {
        "TRACE",
        "DEBUG",
        "INFO",
        "INFORMATION",
        "WARN",
        "WARNING",
        "ERROR",
        "FAIL",
        "FAILURE"
    };

    size_t i = 0;
    while (i < line.size()) {
        if (!isWordStart(line[i])) {
            ++i;
            continue;
        }
        const size_t start = i;
        ++i;
        while (i < line.size() && isWordChar(line[i])) {
            ++i;
        }
        if (!isBoundaryBefore(line, start) || !isBoundaryAfter(line, i)) {
            continue;
        }
        const size_t len = i - start;
        for (const char *level : levels) {
            size_t levelLen = 0;
            while (level[levelLen] != '\0') {
                ++levelLen;
            }
            if (levelLen != len) {
                continue;
            }
            if (equalsIgnoreCaseAscii(line, start, level)) {
                applyRange(
                    styles,
                    priorities,
                    lineStartOffset + start,
                    lineStartOffset + i,
                    static_cast<char>(SCE_LOG_LEVEL),
                    static_cast<unsigned char>(kPriorityLevel));
                break;
            }
        }
    }
}

void applySpecialRanges(
    const std::string &line,
    const size_t lineStartOffset,
    std::vector<char> &styles,
    std::vector<unsigned char> &priorities)
{
    static constexpr char exceptionSuffix[] = "Exception";
    constexpr size_t exceptionSuffixLen = sizeof(exceptionSuffix) - 1;

    size_t i = 0;
    while (i < line.size()) {
        if (!isWordStart(line[i])) {
            ++i;
            continue;
        }
        const size_t start = i;
        ++i;
        while (i < line.size() && isWordChar(line[i])) {
            ++i;
        }
        if (i - start > exceptionSuffixLen
            && isBoundaryBefore(line, start)
            && isBoundaryAfter(line, i)) {
            const size_t suffixStart = i - exceptionSuffixLen;
            bool suffixOk = true;
            for (size_t k = 0; k < exceptionSuffixLen; ++k) {
                if (line[suffixStart + k] != exceptionSuffix[k]) {
                    suffixOk = false;
                    break;
                }
            }
            if (suffixOk) {
                applyRange(
                    styles,
                    priorities,
                    lineStartOffset + start,
                    lineStartOffset + i,
                    static_cast<char>(SCE_LOG_SPECIAL),
                    static_cast<unsigned char>(kPrioritySpecial));
            }
        }
    }

    for (size_t p = 0; p < line.size(); ++p) {
        if (!equalsIgnoreCaseAscii(line, p, "Segmentation")) {
            continue;
        }
        const size_t segEnd = p + 12;
        size_t j = segEnd;
        while (j < line.size() && std::isspace(static_cast<unsigned char>(line[j])) != 0) {
            ++j;
        }
        if (j == segEnd || !equalsIgnoreCaseAscii(line, j, "fault")) {
            continue;
        }
        const size_t end = j + 5;
        if (isBoundaryBefore(line, p) && isBoundaryAfter(line, end)) {
            applyRange(
                styles,
                priorities,
                lineStartOffset + p,
                lineStartOffset + end,
                static_cast<char>(SCE_LOG_SPECIAL),
                static_cast<unsigned char>(kPrioritySpecial));
        }
    }

    for (size_t p = 0; p < line.size(); ++p) {
        if (!equalsIgnoreCaseAscii(line, p, "core")) {
            continue;
        }
        const size_t coreEnd = p + 4;
        size_t j = coreEnd;
        while (j < line.size() && std::isspace(static_cast<unsigned char>(line[j])) != 0) {
            ++j;
        }
        if (j == coreEnd || !equalsIgnoreCaseAscii(line, j, "dumped")) {
            continue;
        }
        const size_t end = j + 6;
        if (isBoundaryBefore(line, p) && isBoundaryAfter(line, end)) {
            applyRange(
                styles,
                priorities,
                lineStartOffset + p,
                lineStartOffset + end,
                static_cast<char>(SCE_LOG_SPECIAL),
                static_cast<unsigned char>(kPrioritySpecial));
        }
    }
}

void applyPathRanges(
    const std::string &line,
    const size_t lineStartOffset,
    std::vector<char> &styles,
    std::vector<unsigned char> &priorities)
{
    size_t i = 0;
    while (i < line.size()) {
        size_t length = 0;
        if ((i == 0 || !isWordChar(line[i - 1])) && isAsciiAlpha(line[i])) {
            length = parseWindowsPathLength(line, i);
        }
        if (length == 0 && isPathLeadBoundary(line, i) && line[i] == '~') {
            length = parseHomePathLength(line, i);
        }
        if (length == 0 && isPathLeadBoundary(line, i) && line[i] == '/') {
            length = parseUnixAbsolutePathLength(line, i);
        }

        if (length > 0) {
            applyRange(
                styles,
                priorities,
                lineStartOffset + i,
                lineStartOffset + i + length,
                static_cast<char>(SCE_LOG_PATH),
                static_cast<unsigned char>(kPriorityPath));
            i += length;
        } else {
            ++i;
        }
    }
}

void styleLine(
    const std::string &line,
    const size_t lineStartOffset,
    std::vector<char> &styles,
    std::vector<unsigned char> &priorities)
{
    applyNumberRanges(line, lineStartOffset, styles, priorities);
    applyNamespaceRanges(line, lineStartOffset, styles, priorities);
    applyFunctionAndSymbolRanges(line, lineStartOffset, styles, priorities);
    applyLevelRanges(line, lineStartOffset, styles, priorities);
    applySpecialRanges(line, lineStartOffset, styles, priorities);
    applyDateTimeRanges(line, lineStartOffset, styles, priorities);
    applyPathRanges(line, lineStartOffset, styles, priorities);
    applyStringRanges(line, lineStartOffset, styles, priorities);
}

class LogLexer final : public Scintilla::ILexer5 {
private:
    int refCount_ = 1;
    std::string propertyValueStorage_;

public:
    int SCI_METHOD Version() const override {
        return Scintilla::lvRelease5;
    }

    void SCI_METHOD Release() override {
        --refCount_;
        if (refCount_ <= 0) {
            delete this;
        }
    }

    const char * SCI_METHOD PropertyNames() override {
        return "";
    }

    int SCI_METHOD PropertyType(const char *) override {
        return 0;
    }

    const char * SCI_METHOD DescribeProperty(const char *) override {
        return "";
    }

    Sci_Position SCI_METHOD PropertySet(const char *, const char *) override {
        return -1;
    }

    const char * SCI_METHOD DescribeWordListSets() override {
        return "";
    }

    Sci_Position SCI_METHOD WordListSet(int, const char *) override {
        return -1;
    }

    void SCI_METHOD Lex(
        Sci_PositionU startPos,
        Sci_Position lengthDoc,
        int,
        Scintilla::IDocument *pAccess) override
    {
        if (!pAccess) {
            return;
        }

        const Sci_Position docLength = pAccess->Length();
        if (docLength <= 0) {
            return;
        }

        Sci_Position start = static_cast<Sci_Position>(startPos);
        if (start < 0) {
            start = 0;
        }
        if (start >= docLength) {
            start = docLength - 1;
        }
        start = pAccess->LineStart(pAccess->LineFromPosition(start));

        Sci_Position end = start + lengthDoc;
        if (end < start) {
            end = start;
        }
        if (end >= docLength) {
            end = docLength;
        } else {
            const Sci_Position endLine = pAccess->LineFromPosition(end);
            end = pAccess->LineEnd(endLine);
        }

        if (end <= start) {
            return;
        }

        const size_t bufferLength = static_cast<size_t>(end - start);
        std::string text(bufferLength, '\0');
        pAccess->GetCharRange(text.data(), start, end - start);

        std::vector<char> styleBytes(bufferLength, static_cast<char>(SCE_LOG_DEFAULT));
        std::vector<unsigned char> priorities(bufferLength, static_cast<unsigned char>(kPriorityDefault));

        size_t lineStart = 0;
        while (lineStart < text.size()) {
            size_t lineEnd = lineStart;
            while (lineEnd < text.size() && text[lineEnd] != '\r' && text[lineEnd] != '\n') {
                ++lineEnd;
            }

            const std::string line = text.substr(lineStart, lineEnd - lineStart);
            styleLine(line, lineStart, styleBytes, priorities);

            if (lineEnd < text.size()) {
                if (text[lineEnd] == '\r' && lineEnd + 1 < text.size() && text[lineEnd + 1] == '\n') {
                    lineStart = lineEnd + 2;
                } else {
                    lineStart = lineEnd + 1;
                }
            } else {
                lineStart = lineEnd;
            }
        }

        pAccess->StartStyling(start);
        pAccess->SetStyles(end - start, styleBytes.data());
    }

    void SCI_METHOD Fold(Sci_PositionU, Sci_Position, int, Scintilla::IDocument *) override {
    }

    void * SCI_METHOD PrivateCall(int, void *) override {
        return nullptr;
    }

    int SCI_METHOD LineEndTypesSupported() override {
        return 0;
    }

    int SCI_METHOD AllocateSubStyles(int, int) override {
        return -1;
    }

    int SCI_METHOD SubStylesStart(int) override {
        return -1;
    }

    int SCI_METHOD SubStylesLength(int) override {
        return 0;
    }

    int SCI_METHOD StyleFromSubStyle(int subStyle) override {
        return subStyle;
    }

    int SCI_METHOD PrimaryStyleFromStyle(int style) override {
        return style;
    }

    void SCI_METHOD FreeSubStyles() override {
    }

    void SCI_METHOD SetIdentifiers(int, const char *) override {
    }

    int SCI_METHOD DistanceToSecondaryStyles() override {
        return 0;
    }

    const char * SCI_METHOD GetSubStyleBases() override {
        return "";
    }

    int SCI_METHOD NamedStyles() override {
        return 10;
    }

    const char * SCI_METHOD NameOfStyle(int style) override {
        static constexpr std::array<const char *, 10> names = {
            "Default",
            "DateTime",
            "LogLevel",
            "String",
            "Number",
            "Namespace",
            "Function",
            "Symbol",
            "Special",
            "Path"
        };
        if (style < 0 || style >= static_cast<int>(names.size())) {
            return "";
        }
        return names[static_cast<size_t>(style)];
    }

    const char * SCI_METHOD TagsOfStyle(int) override {
        return "";
    }

    const char * SCI_METHOD DescriptionOfStyle(int style) override {
        return NameOfStyle(style);
    }

    const char * SCI_METHOD GetName() override {
        return "logplus";
    }

    int SCI_METHOD GetIdentifier() override {
        return 0;
    }

    const char * SCI_METHOD PropertyGet(const char *) override {
        propertyValueStorage_.clear();
        return propertyValueStorage_.c_str();
    }
};

}

Scintilla::ILexer5 *createLogLexer()
{
    return new LogLexer();
}
