#pragma once

#include <ScintillaEdit.h>

struct IndentDetectionResult {
    // Number of spaces that make up one indent level. Only meaningful for
    // space indentation; equals the passed-in default when tabs are used.
    int indentWidth = 4;
    // True when indentation should be made of spaces.
    bool indentUsingSpaces = true;
};

namespace IndentGuesser {
// Guesses the indentation style (tabs vs. spaces, and the width of one indent
// level in spaces) from the document content.
//
// Direct port of VS Code's guessIndentation()
// (src/vs/editor/common/model/indentationGuesser.ts, MIT licensed); the two
// small deviations from the original are marked in the implementation.
//
// Detection is a single bounded pass over the leading whitespace of at most
// the first 10,000 lines (oversized lines are only sampled at their start),
// so its cost is negligible next to lexing.
//
// Returns the passed-in defaults whenever the content yields no usable hints.
IndentDetectionResult guessIndentation(ScintillaEdit *editor, int defaultIndentWidth, bool defaultInsertSpaces);
}
