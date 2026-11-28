/*
 * SPDX-FileCopyrightText: Microsoft Corporation
 * SPDX-License-Identifier: MIT
 *
 * Indentation detection for Scintilla documents. Direct port of VS Code's
 * guessIndentation():
 * https://github.com/microsoft/vscode/blob/main/src/vs/editor/common/model/indentationGuesser.ts
 *
 * Two small deviations from the original were added by the pineapple-notepad
 * project (both follow fixes made in KDE's KTextEditor port of the same
 * algorithm and are marked below):
 *   - odd indent widths can only win when no even width has any votes
 *   - a lone 1-space diff pattern is recognized as single-space indentation
 */

#include "indentguesser.h"

#include <algorithm>

namespace {

// Look at most at the first 10k lines.
constexpr sptr_t kMaxScannedLines = 10000;

// Scintilla adaptation: lines longer than this are not fetched as a whole.
// Only the leading whitespace and a few bytes beyond it ever matter for the
// guess, so a single multi-megabyte line (minified file) cannot stall the
// document load.
constexpr sptr_t kMaxFetchedLineLength = 65536;
constexpr int kIndentPrefixLength = 64;

struct SpacesDiffResult {
    int spacesDiff = 0;
    bool looksLikeAlignment = false;
};

// Compute the diff in spaces between two lines' indentation.
// aLength/bLength are the indentation lengths; a/b additionally give access
// to the full line text (needed by the alignment heuristic below).
SpacesDiffResult spacesDiff(const QByteArray &a, int aLength, const QByteArray &b, int bLength)
{
    SpacesDiffResult result;

    // This can go both ways (e.g.):
    //  - a: "\t"
    //  - b: "\t    "
    //  => This should count 1 tab and 4 spaces

    int i = 0;
    for (i = 0; i < aLength && i < bLength; i++) {
        if (a.at(i) != b.at(i)) {
            break;
        }
    }

    int aSpacesCnt = 0;
    int aTabsCount = 0;
    for (int j = i; j < aLength; j++) {
        const char aCharCode = a.at(j);
        if (aCharCode == ' ') {
            aSpacesCnt++;
        } else {
            aTabsCount++;
        }
    }

    int bSpacesCnt = 0;
    int bTabsCount = 0;
    for (int j = i; j < bLength; j++) {
        const char bCharCode = b.at(j);
        if (bCharCode == ' ') {
            bSpacesCnt++;
        } else {
            bTabsCount++;
        }
    }

    if (aSpacesCnt > 0 && aTabsCount > 0) {
        return result;
    }
    if (bSpacesCnt > 0 && bTabsCount > 0) {
        return result;
    }

    const int tabsDiff = std::abs(aTabsCount - bTabsCount);
    const int spacesDiff = std::abs(aSpacesCnt - bSpacesCnt);

    if (tabsDiff == 0) {
        // check if the indentation difference might be caused by alignment reasons
        // sometime folks like to align their code, but this should not be used as a hint
        result.spacesDiff = spacesDiff;

        if (spacesDiff > 0 && 0 <= bSpacesCnt - 1 && bSpacesCnt - 1 < a.size() && bSpacesCnt < b.size()) {
            if (b.at(bSpacesCnt) != ' ' && a.at(bSpacesCnt - 1) == ' ') {
                if (a.at(a.size() - 1) == ',') {
                    // This looks like an alignment desire: e.g.
                    // const a = b + c,
                    //       d = b - c;
                    result.looksLikeAlignment = true;
                }
            }
        }
        return result;
    }
    if (spacesDiff % tabsDiff == 0) {
        result.spacesDiff = spacesDiff / tabsDiff;
        return result;
    }
    return result;
}

} // namespace

IndentDetectionResult IndentGuesser::guessIndentation(ScintillaEdit *editor, int defaultIndentWidth, bool defaultInsertSpaces)
{
    IndentDetectionResult result;
    result.indentWidth = defaultIndentWidth;
    result.indentUsingSpaces = defaultInsertSpaces;
    if (!editor || editor->length() == 0) {
        return result;
    }

    // Look at most at the first 10k lines
    const int linesCount = int(std::min<sptr_t>(editor->lineCount(), kMaxScannedLines));

    int linesIndentedWithTabsCount = 0; // number of lines that contain at least one tab in indentation
    int linesIndentedWithSpacesCount = 0; // number of lines that contain only spaces in indentation

    QByteArray previousLineText; // content of latest line that contained non-whitespace chars
    int previousLineIndentation = 0; // index at which latest line contained the first non-whitespace char

    constexpr int ALLOWED_TAB_SIZE_GUESSES[] = {2, 4, 6, 8, 3, 5, 7}; // prefer even guesses for `tabSize`, limit to [2, 8]
    constexpr int MAX_ALLOWED_TAB_SIZE_GUESS = 8; // max(ALLOWED_TAB_SIZE_GUESSES)

    int spacesDiffCount[MAX_ALLOWED_TAB_SIZE_GUESS + 1] = {0, 0, 0, 0, 0, 0, 0, 0, 0}; // `tabSize` scores
    SpacesDiffResult tmp;

    for (int lineNumber = 0; lineNumber < linesCount; lineNumber++) {
        const sptr_t lineStart = editor->positionFromLine(lineNumber);
        const sptr_t lineLength = editor->lineLength(lineNumber); // includes the line ending

        // Scintilla adaptation: fetch the line text without the line ending
        // so the alignment heuristic below sees the real last character of
        // the line. Oversized lines are truncated to a small prefix.
        QByteArray currentLineText;
        if (lineLength <= kMaxFetchedLineLength) {
            currentLineText = editor->textRange(int(lineStart), int(lineStart + lineLength));
            while (currentLineText.endsWith('\n') || currentLineText.endsWith('\r')) {
                currentLineText.chop(1);
            }
        } else {
            currentLineText = editor->textRange(int(lineStart), int(lineStart) + kIndentPrefixLength);
        }

        bool currentLineHasContent = false; // does the line contain non-whitespace chars
        int currentLineIndentation = 0; // index at which the line contains the first non-whitespace char
        int currentLineSpacesCount = 0; // count of spaces found in the line's indentation
        int currentLineTabsCount = 0; // count of tabs found in the line's indentation
        for (int j = 0, lenJ = currentLineText.size(); j < lenJ; j++) {
            const char charCode = currentLineText.at(j);

            if (charCode == '\t') {
                currentLineTabsCount++;
            } else if (charCode == ' ') {
                currentLineSpacesCount++;
            } else {
                // Hit non whitespace character on this line
                currentLineHasContent = true;
                currentLineIndentation = j;
                break;
            }
        }

        // Ignore empty or only whitespace lines
        if (!currentLineHasContent) {
            continue;
        }

        if (currentLineTabsCount > 0) {
            linesIndentedWithTabsCount++;
        } else if (currentLineSpacesCount > 1) {
            linesIndentedWithSpacesCount++;
        }

        tmp = spacesDiff(previousLineText, previousLineIndentation, currentLineText, currentLineIndentation);

        if (tmp.looksLikeAlignment) {
            // if defaultInsertSpaces === true && the spaces count == tabSize, we may want to count it as valid indentation
            //
            // - item1
            //   - item2
            //
            // otherwise skip this line entirely
            //
            // const a = 1,
            //       b = 2;
            if (!(defaultInsertSpaces && defaultIndentWidth == tmp.spacesDiff)) {
                continue;
            }
        }

        const int currentSpacesDiff = tmp.spacesDiff;
        if (currentSpacesDiff <= MAX_ALLOWED_TAB_SIZE_GUESS) {
            spacesDiffCount[currentSpacesDiff]++;
        }

        previousLineText = currentLineText;
        previousLineIndentation = currentLineIndentation;
    }

    bool insertSpaces = defaultInsertSpaces;
    if (linesIndentedWithTabsCount != linesIndentedWithSpacesCount) {
        insertSpaces = (linesIndentedWithTabsCount < linesIndentedWithSpacesCount);
    }

    int tabSize = defaultIndentWidth;

    // Guess tabSize only if inserting spaces...
    if (insertSpaces) {
        int tabSizeScore = 0;
        for (const int possibleTabSize : ALLOWED_TAB_SIZE_GUESSES) {
            const int possibleTabSizeScore = spacesDiffCount[possibleTabSize];
            // pineapple-notepad deviation (follows KTextEditor, Kate bug
            // 474505): prefer even guesses; an odd width only wins when no
            // even width has any votes, so stray odd indents cannot take the
            // guess away from a clearly predominant even width.
            if (possibleTabSizeScore > tabSizeScore && (possibleTabSize % 2 == 0 || tabSizeScore == 0)) {
                tabSizeScore = possibleTabSizeScore;
                tabSize = possibleTabSize;
            }
        }

        // Let a tabSize of 2 win over 4 only if it has at least 2/3 of the occurrences of 4
        // This helps detect 2-space indentation in cases like YAML files where there might be
        // some 4-space diffs from deeper nesting, while still preferring 4 when it's clearly predominant
        if (tabSize == 4 && spacesDiffCount[4] > 0 && spacesDiffCount[2] > 0
            && spacesDiffCount[2] >= spacesDiffCount[4] * 2 / 3) {
            tabSize = 2;
        }

        // pineapple-notepad deviation (follows KTextEditor): when no width
        // received a single vote, a lone 1-space diff pattern means the file
        // really uses single-space indentation (common in XML).
        if (tabSizeScore == 0) {
            const auto it = std::max_element(spacesDiffCount, spacesDiffCount + MAX_ALLOWED_TAB_SIZE_GUESS + 1);
            const auto maxIdx = std::distance(spacesDiffCount, it);
            if (maxIdx == 1) {
                tabSize = 1;
            }
        }
    }

    result.indentWidth = tabSize;
    result.indentUsingSpaces = insertSpaces;
    return result;
}
