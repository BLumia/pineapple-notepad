// SPDX-FileCopyrightText: 2026 Gary Wang <git@blumia.net>
//
// Mapping of lexilla style ids to
// KSyntaxHighlighting::Theme::TextStyle, per lexer. This is the source of truth; the
// former language-config.json "baseStyles" block and per-language canonical "styles"
// mappings now live here as compile-time data.

#include "basestyles.h"

#include <KSyntaxHighlighting/Theme>
#include <QString>
#include <span>
#include <string_view>

using KSyntaxHighlighting::Theme;

namespace {

struct BaseStyleEntry {
    int styleId;
    Theme::TextStyle textStyle;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool eolFilled = false;
    // Static spell-check declaration for non-KSH lexers (comments, strings,
    // ...); the KSH lexer determines spell-check dynamically and ignores this
    // field.
    bool spellCheck = false;
};

static constexpr BaseStyleEntry cppStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 3, .textStyle = Theme::Documentation, .spellCheck = true },
    { 4, Theme::DecVal },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 6, .textStyle = Theme::String, .spellCheck = true },
    { 7, Theme::Char },
    { 8, Theme::Others },
    { 9, Theme::Preprocessor },
    { 10, Theme::Operator },
    { 11, Theme::Normal },
    { .styleId = 12, .textStyle = Theme::SpecialString, .spellCheck = true },
    { .styleId = 13, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 14, .textStyle = Theme::SpecialString, .spellCheck = true },
    { .styleId = 15, .textStyle = Theme::Documentation, .spellCheck = true },
    { 16, Theme::BuiltIn },
    { 17, Theme::Annotation },
    { 18, Theme::Error },
    { 19, Theme::BuiltIn },
    { .styleId = 20, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 21, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 22, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 23, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 24, .textStyle = Theme::Documentation, .spellCheck = true },
    { 25, Theme::Others },
    { 26, Theme::Information },
    { 27, Theme::SpecialChar },
};

static constexpr BaseStyleEntry hypertextStyles[] = {
    // styleId 0 is SCE_H_DEFAULT: the text content between tags (HTML prose).
    { .styleId = 0, .textStyle = Theme::Normal, .spellCheck = true },
    { 1, Theme::Keyword },
    { 2, Theme::Keyword },
    { 3, Theme::Attribute },
    { 4, Theme::Attribute },
    { 5, Theme::DecVal },
    { .styleId = 6, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 7, .textStyle = Theme::String, .spellCheck = true },
    { 8, Theme::Others },
    { .styleId = 9, .textStyle = Theme::Comment, .spellCheck = true },
    { 10, Theme::SpecialChar },
    { 11, Theme::Keyword },
    { 12, Theme::Preprocessor },
    { 13, Theme::Preprocessor },
    { 14, Theme::Others },
    { 17, Theme::Others },
    { 18, Theme::Preprocessor },
    { 19, Theme::String },
    { .styleId = 20, .textStyle = Theme::Comment, .spellCheck = true },
    { 22, Theme::Keyword },
    { 23, Theme::Attribute },
    { 24, Theme::String },
    { 25, Theme::String },
    { 26, Theme::Error },
    { 27, Theme::SpecialChar },
    { 28, Theme::SpecialChar },
    { .styleId = 29, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 30, .textStyle = Theme::Documentation, .spellCheck = true },
    { 31, Theme::Others },
};

static constexpr BaseStyleEntry xmlStyles[] = {
    // styleId 0 is SCE_H_DEFAULT: XML character data (text node content).
    { .styleId = 0, .textStyle = Theme::Normal, .spellCheck = true },
    { 1, Theme::Keyword },
    { 2, Theme::Keyword },
    { 3, Theme::Attribute },
    { 4, Theme::Attribute },
    { 5, Theme::DecVal },
    { .styleId = 6, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 7, .textStyle = Theme::String, .spellCheck = true },
    { 8, Theme::Others },
    { .styleId = 9, .textStyle = Theme::Comment, .spellCheck = true },
    { 10, Theme::SpecialChar },
    { 11, Theme::Keyword },
    { 12, Theme::Preprocessor },
    { 13, Theme::Preprocessor },
    { 17, Theme::Others },
    { 18, Theme::Preprocessor },
    { 19, Theme::String },
    { .styleId = 20, .textStyle = Theme::Comment, .spellCheck = true },
};

static constexpr BaseStyleEntry pythonStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { 2, Theme::DecVal },
    { .styleId = 3, .textStyle = Theme::String, .spellCheck = true },
    { 4, Theme::Char },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 6, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 7, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 8, .textStyle = Theme::DataType, .bold = true },
    { .styleId = 9, .textStyle = Theme::Function, .bold = true },
    { 10, Theme::Operator },
    { 11, Theme::Normal },
    { .styleId = 12, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 13, .textStyle = Theme::SpecialString, .spellCheck = true },
    { .styleId = 14, .textStyle = Theme::BuiltIn, .bold = true },
    { 15, Theme::Attribute },
    { .styleId = 16, .textStyle = Theme::String, .spellCheck = true },
    { 17, Theme::Char },
    { .styleId = 18, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 19, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { 20, Theme::Attribute },
};

static constexpr BaseStyleEntry bashStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Error },
    { .styleId = 2, .textStyle = Theme::Comment, .spellCheck = true },
    { 3, Theme::DecVal },
    { .styleId = 4, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 5, .textStyle = Theme::String, .spellCheck = true },
    { 6, Theme::Char },
    { 7, Theme::Operator },
    { 8, Theme::Normal },
    { .styleId = 9, .textStyle = Theme::Variable, .bold = true },
    { 10, Theme::Normal },
    { .styleId = 11, .textStyle = Theme::String, .bold = true, .spellCheck = true },
    { 12, Theme::Others },
    { 13, Theme::Others },
};

static constexpr BaseStyleEntry ksyntaxhighlightingStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Keyword, .bold = true },
    { 2, Theme::Function },
    { 3, Theme::Variable },
    { 4, Theme::ControlFlow },
    { 5, Theme::Operator },
    { 6, Theme::BuiltIn },
    { 7, Theme::Extension },
    { 8, Theme::Preprocessor },
    { 9, Theme::Attribute },
    { 10, Theme::Char },
    { 11, Theme::SpecialChar },
    { 12, Theme::String },
    { 13, Theme::VerbatimString },
    { 14, Theme::SpecialString },
    { 15, Theme::Import },
    { 16, Theme::DataType },
    { 17, Theme::DecVal },
    { 18, Theme::BaseN },
    { 19, Theme::Float },
    { 20, Theme::Constant },
    { 21, Theme::Comment },
    { 22, Theme::Documentation },
    { 23, Theme::Annotation },
    { 24, Theme::CommentVar },
    { 25, Theme::RegionMarker },
    { 26, Theme::Information },
    { 27, Theme::Warning },
    { 28, Theme::Alert },
    { 29, Theme::Error },
    { 30, Theme::Others },
};

static constexpr BaseStyleEntry avsStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::Comment },
    { 3, Theme::Comment },
    { 4, Theme::DecVal },
    { 5, Theme::Operator },
    { 6, Theme::Normal },
    { 7, Theme::String },
    { 8, Theme::String },
    { .styleId = 9, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 10, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 11, .textStyle = Theme::BuiltIn, .bold = true },
    { 12, Theme::Function },
    { 13, Theme::Keyword },
    { 14, Theme::SpecialString },
};

static constexpr BaseStyleEntry yamlStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { 2, Theme::Normal },
    { 3, Theme::Keyword },
    { 4, Theme::DecVal },
    { 5, Theme::Variable },
    { 6, Theme::Preprocessor },
    { .styleId = 7, .textStyle = Theme::String, .spellCheck = true },
    { 8, Theme::Error },
    { 9, Theme::Operator },
};

static constexpr BaseStyleEntry jsonStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::DecVal },
    { .styleId = 2, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 3, .textStyle = Theme::SpecialString, .spellCheck = true },
    // styleId 4 is SCE_JSON_PROPERTYNAME (object keys): identifiers, not prose.
    { 4, Theme::Attribute },
    { 5, Theme::SpecialChar },
    { .styleId = 6, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 7, .textStyle = Theme::Comment, .spellCheck = true },
    { 8, Theme::Operator },
    { 9, Theme::Others },
    { 10, Theme::Others },
    { 11, Theme::Keyword },
    { 12, Theme::Keyword },
    { 13, Theme::Error },
};

static constexpr BaseStyleEntry cssStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Keyword },
    { 2, Theme::Attribute },
    { 3, Theme::Attribute },
    { 4, Theme::Error },
    { 5, Theme::Operator },
    { 6, Theme::Normal },
    { 7, Theme::Error },
    { 8, Theme::Normal },
    { .styleId = 9, .textStyle = Theme::Comment, .spellCheck = true },
    { 10, Theme::Attribute },
    { .styleId = 11, .textStyle = Theme::Keyword, .bold = true },
    { 12, Theme::Preprocessor },
    { .styleId = 13, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 14, .textStyle = Theme::String, .spellCheck = true },
    { 15, Theme::Normal },
    { 16, Theme::Attribute },
    { 17, Theme::Normal },
    { 18, Theme::Attribute },
    { 19, Theme::Normal },
    { 20, Theme::Attribute },
    { 21, Theme::Attribute },
    { 22, Theme::Preprocessor },
    { 23, Theme::Variable },
};

static constexpr BaseStyleEntry cmakeStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 3, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 4, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 5, .textStyle = Theme::Function, .bold = true },
    { 6, Theme::Normal },
    { 7, Theme::Variable },
    { .styleId = 8, .textStyle = Theme::Function, .bold = true },
    { .styleId = 9, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 10, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 11, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 12, .textStyle = Theme::Function, .bold = true },
    { 13, Theme::Variable },
    { 14, Theme::DecVal },
};

static constexpr BaseStyleEntry propsStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { 2, Theme::Function },
    { 3, Theme::Others },
    { 4, Theme::Others },
    { 5, Theme::Keyword },
};

static constexpr BaseStyleEntry batchStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::Keyword, .bold = true },
    { 3, Theme::Others },
    { 4, Theme::Others },
    { 5, Theme::Function },
    { .styleId = 6, .textStyle = Theme::Normal, .bold = true },
    { 7, Theme::Operator },
    { 8, Theme::Others },
};

static constexpr BaseStyleEntry luaStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 3, .textStyle = Theme::Documentation, .spellCheck = true },
    { 4, Theme::DecVal },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 6, .textStyle = Theme::String, .spellCheck = true },
    { 7, Theme::Char },
    { .styleId = 8, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { 9, Theme::Preprocessor },
    { 10, Theme::Operator },
    { 11, Theme::Normal },
    { .styleId = 12, .textStyle = Theme::SpecialString, .spellCheck = true },
    { .styleId = 13, .textStyle = Theme::BuiltIn, .bold = true },
    { .styleId = 14, .textStyle = Theme::BuiltIn, .bold = true },
    { .styleId = 15, .textStyle = Theme::BuiltIn, .bold = true },
    { 16, Theme::BuiltIn },
    { 17, Theme::BuiltIn },
    { 18, Theme::BuiltIn },
    { 19, Theme::BuiltIn },
    { 20, Theme::Others },
};

static constexpr BaseStyleEntry makefileStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::Preprocessor, .bold = true },
    { 3, Theme::Normal },
    { 4, Theme::Operator },
    { .styleId = 5, .textStyle = Theme::Function, .bold = true },
    { .styleId = 9, .textStyle = Theme::Preprocessor, .italic = true },
};

static constexpr BaseStyleEntry logplusStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Preprocessor },
    { .styleId = 2, .textStyle = Theme::Keyword, .bold = true },
    { 3, Theme::String },
    { 11, Theme::Preprocessor },
    { .styleId = 12, .textStyle = Theme::Keyword, .bold = true },
    { 13, Theme::DataType },
};

// See lib/searchresultlexer.h.
static constexpr BaseStyleEntry searchResultStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 2, .textStyle = Theme::String, .bold = true },
    { 3, Theme::Comment },
    { .styleId = 4, .textStyle = Theme::Alert, .bold = true },
};

static constexpr BaseStyleEntry diffStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { .styleId = 2, .textStyle = Theme::Keyword, .bold = true },
    { 3, Theme::Preprocessor },
    { 4, Theme::DecVal },
    { 5, Theme::Error },
    { 6, Theme::String },
    { 7, Theme::Others },
    { 8, Theme::Others },
    { 9, Theme::Others },
    { 10, Theme::Others },
    { 11, Theme::Others },
};

static constexpr BaseStyleEntry rubyStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Error },
    { .styleId = 2, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 3, .textStyle = Theme::Documentation, .spellCheck = true },
    { 4, Theme::DecVal },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 6, .textStyle = Theme::String, .spellCheck = true },
    { 7, Theme::Char },
    { .styleId = 8, .textStyle = Theme::DataType, .bold = true },
    { .styleId = 9, .textStyle = Theme::Function, .bold = true },
    { 10, Theme::Operator },
    { 11, Theme::Normal },
    { .styleId = 12, .textStyle = Theme::SpecialString, .spellCheck = true },
    { 13, Theme::Variable },
    { 14, Theme::Others },
    { .styleId = 15, .textStyle = Theme::DataType, .bold = true },
    { 16, Theme::Variable },
    { 17, Theme::Variable },
    { .styleId = 18, .textStyle = Theme::String, .spellCheck = true },
    { 19, Theme::Others },
    { 20, Theme::Others },
    { 21, Theme::Others },
    { 22, Theme::Others },
    { 23, Theme::Others },
    { .styleId = 24, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 25, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 26, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 27, .textStyle = Theme::SpecialString, .spellCheck = true },
    { .styleId = 28, .textStyle = Theme::String, .spellCheck = true },
    { 29, Theme::Others },
    { 30, Theme::Others },
    { 31, Theme::Others },
    { 40, Theme::Error },
    { .styleId = 41, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 42, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 43, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 44, .textStyle = Theme::String, .spellCheck = true },
};

static constexpr BaseStyleEntry rustStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 3, .textStyle = Theme::Documentation, .spellCheck = true },
    { .styleId = 4, .textStyle = Theme::Documentation, .spellCheck = true },
    { 5, Theme::DecVal },
    { .styleId = 6, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 7, .textStyle = Theme::DataType, .bold = true },
    { 8, Theme::DataType },
    { 9, Theme::Keyword },
    { 10, Theme::Keyword },
    { 11, Theme::Keyword },
    { 12, Theme::Keyword },
    { .styleId = 13, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 14, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { 15, Theme::Char },
    { 16, Theme::Operator },
    { 17, Theme::Normal },
    { 18, Theme::Others },
    { 19, Theme::Others },
    { 20, Theme::Error },
    { .styleId = 21, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 22, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { 23, Theme::Char },
    { .styleId = 24, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 25, .textStyle = Theme::VerbatimString, .spellCheck = true },
};

static constexpr BaseStyleEntry tclStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::Comment },
    { 3, Theme::DecVal },
    { .styleId = 4, .textStyle = Theme::String, .bold = true },
    { .styleId = 5, .textStyle = Theme::String, .bold = true },
    { 6, Theme::Operator },
    { 7, Theme::Normal },
    { 8, Theme::Variable },
    { 9, Theme::String },
    { 10, Theme::Preprocessor },
    { 11, Theme::Preprocessor },
    { .styleId = 12, .textStyle = Theme::Keyword, .bold = true },
    { 13, Theme::BuiltIn },
    { 14, Theme::BuiltIn },
    { 15, Theme::BuiltIn },
    { 16, Theme::BuiltIn },
    { .styleId = 17, .textStyle = Theme::Documentation, .bold = true },
    { 18, Theme::Documentation },
    { 19, Theme::BuiltIn },
    { 20, Theme::Comment },
    { 21, Theme::Comment },
};

static constexpr BaseStyleEntry pascalStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Normal },
    { 2, Theme::Comment },
    { 3, Theme::Comment },
    { 4, Theme::Documentation },
    { 5, Theme::Preprocessor },
    { 6, Theme::Preprocessor },
    { 7, Theme::DecVal },
    { 8, Theme::BaseN },
    { .styleId = 9, .textStyle = Theme::Keyword, .bold = true },
    { 10, Theme::String },
    { 11, Theme::SpecialString },
    { 12, Theme::Char },
    { 13, Theme::Operator },
    { .styleId = 14, .textStyle = Theme::Keyword, .bold = true },
    { 15, Theme::VerbatimString },
};

static constexpr BaseStyleEntry powershellStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::String, .spellCheck = true },
    { 3, Theme::Char },
    { 4, Theme::DecVal },
    { .styleId = 5, .textStyle = Theme::Variable, .bold = true },
    { .styleId = 6, .textStyle = Theme::Normal, .bold = true },
    { 7, Theme::Normal },
    { .styleId = 8, .textStyle = Theme::Keyword, .bold = true },
    { 9, Theme::BuiltIn },
    { 10, Theme::Normal },
    { 11, Theme::Function },
    { 12, Theme::Others },
    { .styleId = 13, .textStyle = Theme::Documentation, .spellCheck = true },
    { .styleId = 14, .textStyle = Theme::String, .spellCheck = true },
    { 15, Theme::Char },
    { 16, Theme::Annotation },
};

static constexpr BaseStyleEntry asmStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::DecVal },
    { 3, Theme::String },
    { 4, Theme::Operator },
    { 5, Theme::Normal },
    { .styleId = 6, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 7, .textStyle = Theme::Keyword, .bold = true },
    { 8, Theme::BuiltIn },
    { 9, Theme::Preprocessor },
    { 10, Theme::Normal },
    { 11, Theme::Comment },
    { 12, Theme::Char },
    { 13, Theme::SpecialString },
    { 14, Theme::BuiltIn },
    { 15, Theme::Others },
    { 16, Theme::String },
};

static constexpr BaseStyleEntry fortranStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::DecVal },
    { 3, Theme::String },
    { .styleId = 4, .textStyle = Theme::String, .bold = true },
    { 5, Theme::SpecialString },
    { 6, Theme::Operator },
    { 7, Theme::Normal },
    { .styleId = 8, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 9, .textStyle = Theme::Function, .bold = true },
    { .styleId = 10, .textStyle = Theme::Function, .bold = true },
    { 11, Theme::Preprocessor },
    { 12, Theme::Operator },
    { 13, Theme::Others },
    { 14, Theme::Others },
};

static constexpr BaseStyleEntry camlStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Normal },
    { 2, Theme::DataType },
    { .styleId = 3, .textStyle = Theme::Keyword, .bold = true },
    { 4, Theme::DataType },
    { 5, Theme::DataType },
    { 6, Theme::Others },
    { .styleId = 7, .textStyle = Theme::Operator, .bold = true },
    { 8, Theme::DecVal },
    { 9, Theme::Char },
    { 10, Theme::Others },
    { 11, Theme::String },
    { 12, Theme::Comment },
    { 13, Theme::Comment },
    { 14, Theme::Documentation },
    { 15, Theme::Documentation },
};

static constexpr BaseStyleEntry haskellStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Normal },
    { .styleId = 2, .textStyle = Theme::Keyword, .bold = true },
    { 3, Theme::DecVal },
    { 4, Theme::String },
    { 5, Theme::Char },
    { 6, Theme::DataType },
    { 7, Theme::DataType },
    { 8, Theme::Normal },
    { 9, Theme::BuiltIn },
    { 10, Theme::Import },
    { 11, Theme::Operator },
    { 12, Theme::BuiltIn },
    { 13, Theme::Comment },
    { 14, Theme::Comment },
    { 15, Theme::Comment },
    { 16, Theme::Comment },
    { 17, Theme::Preprocessor },
    { 18, Theme::Preprocessor },
    { 19, Theme::SpecialString },
    { 20, Theme::Operator },
    { 21, Theme::Others },
    { 22, Theme::Others },
};

static constexpr BaseStyleEntry asciidocStyles[] = {
    // Prose markup: default/bold/italic/heading/blockquote/comment styles are
    // all natural-language text and get spell checked. Code-ish styles
    // (CODEBK, PASSBK, LITERAL*, ATTRIB*, LINK, MACRO, list markers) do not.
    { .styleId = 0, .textStyle = Theme::Normal, .spellCheck = true },
    { .styleId = 1, .textStyle = Theme::Normal, .bold = true, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::Normal, .bold = true, .spellCheck = true },
    { .styleId = 3, .textStyle = Theme::Normal, .italic = true, .spellCheck = true },
    { .styleId = 4, .textStyle = Theme::Normal, .italic = true, .spellCheck = true },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true, .spellCheck = true },
    { .styleId = 6, .textStyle = Theme::Keyword, .bold = true, .spellCheck = true },
    { .styleId = 7, .textStyle = Theme::Keyword, .bold = true, .spellCheck = true },
    { .styleId = 8, .textStyle = Theme::Keyword, .bold = true, .spellCheck = true },
    { .styleId = 9, .textStyle = Theme::Keyword, .bold = true, .spellCheck = true },
    { .styleId = 10, .textStyle = Theme::Keyword, .bold = true, .spellCheck = true },
    { 11, Theme::Operator },
    { 12, Theme::DecVal },
    { .styleId = 13, .textStyle = Theme::Comment, .spellCheck = true },
    { 14, Theme::Attribute },
    { 15, Theme::String },
    { 16, Theme::String },
    { .styleId = 17, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 18, .textStyle = Theme::Comment, .spellCheck = true },
    { 19, Theme::String },
    { 20, Theme::String },
    { 21, Theme::Attribute },
    { 22, Theme::String },
    { 23, Theme::Preprocessor },
};

static constexpr BaseStyleEntry dartStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::Documentation, .spellCheck = true },
    { .styleId = 3, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 4, .textStyle = Theme::Documentation, .spellCheck = true },
    { .styleId = 5, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 6, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 7, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 8, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 9, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 10, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 11, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 12, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { 13, Theme::SpecialChar },
    { 14, Theme::Normal },
    { 15, Theme::Normal },
    { 16, Theme::Operator },
    { 17, Theme::Operator },
    { 18, Theme::Others },
    { 19, Theme::Others },
    { 20, Theme::DecVal },
    { 21, Theme::Attribute },
    { 22, Theme::Preprocessor },
    { .styleId = 23, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 24, .textStyle = Theme::Keyword, .bold = true },
    { 25, Theme::Keyword },
    { .styleId = 26, .textStyle = Theme::DataType, .bold = true },
    { 27, Theme::Error },
};

static constexpr BaseStyleEntry juliaStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::DecVal },
    { .styleId = 3, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 4, .textStyle = Theme::DataType, .bold = true },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true },
    { 6, Theme::Char },
    { 7, Theme::Operator },
    { 8, Theme::Others },
    { 9, Theme::Normal },
    { 10, Theme::String },
    { 11, Theme::Others },
    { 12, Theme::Preprocessor },
    { 13, Theme::Variable },
    { 14, Theme::Documentation },
    { 15, Theme::String },
    { 16, Theme::String },
    { 17, Theme::String },
    { 18, Theme::DataType },
    { 19, Theme::Error },
    { .styleId = 20, .textStyle = Theme::Function, .bold = true },
    { 21, Theme::Operator },
};

static constexpr BaseStyleEntry lispStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::DecVal },
    { .styleId = 3, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 4, .textStyle = Theme::Keyword, .bold = true },
    { 5, Theme::Others },
    { 6, Theme::String },
    { 7, Theme::SpecialString },
    { 8, Theme::Error },
    { 9, Theme::Normal },
    { 10, Theme::Operator },
    { 11, Theme::Preprocessor },
    { 12, Theme::Comment },
};

static constexpr BaseStyleEntry latexStyles[] = {
    // Document format: SCE_L_DEFAULT (0) is the document prose; math and
    // verbatim environments are code and stay unchecked.
    { .styleId = 0, .textStyle = Theme::Normal, .spellCheck = true },
    { .styleId = 1, .textStyle = Theme::Keyword, .bold = true },
    { 2, Theme::Others },
    { 3, Theme::String },
    { .styleId = 4, .textStyle = Theme::Comment, .spellCheck = true },
    { 5, Theme::Others },
    { 6, Theme::String },
    { .styleId = 7, .textStyle = Theme::Comment, .spellCheck = true },
    { 8, Theme::VerbatimString },
    { 9, Theme::Others },
    { 10, Theme::Others },
    { 11, Theme::Attribute },
    { 12, Theme::Error },
};

static constexpr BaseStyleEntry vbStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::DecVal },
    { .styleId = 3, .textStyle = Theme::Keyword, .bold = true },
    { 4, Theme::String },
    { 5, Theme::Preprocessor },
    { 6, Theme::Operator },
    { 7, Theme::Normal },
    { 8, Theme::Others },
    { 9, Theme::SpecialString },
    { 10, Theme::Keyword },
    { 11, Theme::Keyword },
    { 12, Theme::Keyword },
    { 13, Theme::Constant },
    { 14, Theme::Others },
    { 15, Theme::Others },
    { 16, Theme::Error },
    { 17, Theme::BaseN },
    { 18, Theme::BaseN },
    { 19, Theme::Comment },
    { 20, Theme::Documentation },
    { 21, Theme::Documentation },
    { 22, Theme::Annotation },
};

static constexpr BaseStyleEntry perlStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Error },
    { .styleId = 2, .textStyle = Theme::Comment, .spellCheck = true },
    // 3 is SCE_PL_POD: plain old documentation, natural-language prose.
    // SCE_PL_POD_VERB (31, indented verbatim blocks) is code and unchecked.
    { .styleId = 3, .textStyle = Theme::Documentation, .spellCheck = true },
    { 4, Theme::DecVal },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 6, .textStyle = Theme::String, .spellCheck = true },
    { 7, Theme::Char },
    { 8, Theme::Others },
    { 9, Theme::Preprocessor },
    { 10, Theme::Operator },
    { 11, Theme::Normal },
    { 12, Theme::Variable },
    { 13, Theme::Variable },
    { 14, Theme::Variable },
    { 15, Theme::Variable },
    { 16, Theme::Others },
    // Regexes (17, 18, 29), v-strings (19), backticks/commands (20, 28),
    // here-docs (22-25), qw word lists (30) and data sections stay unchecked.
    { 17, Theme::SpecialString },
    { 18, Theme::SpecialString },
    { 19, Theme::Others },
    { 20, Theme::String },
    { 21, Theme::Others },
    { 22, Theme::Others },
    { 23, Theme::Others },
    { 24, Theme::Others },
    { 25, Theme::Others },
    { .styleId = 26, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 27, .textStyle = Theme::String, .spellCheck = true },
    { 28, Theme::String },
    { 29, Theme::SpecialString },
    { 30, Theme::String },
    { 31, Theme::Documentation },
    { 40, Theme::Others },
    { 41, Theme::Others },
    { 42, Theme::Others },
    { 43, Theme::Variable },
    { 44, Theme::Others },
    { 54, Theme::Variable },
    { 55, Theme::Variable },
    { 57, Theme::Variable },
    { 61, Theme::Variable },
    { 62, Theme::Variable },
    { 64, Theme::Variable },
    { 65, Theme::Variable },
    { 66, Theme::Variable },
};

static constexpr BaseStyleEntry sqlStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::Comment },
    { 3, Theme::Documentation },
    { 4, Theme::DecVal },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true },
    { 6, Theme::String },
    { 7, Theme::Char },
    { 8, Theme::Others },
    { 9, Theme::Others },
    { 10, Theme::Operator },
    { 11, Theme::Normal },
    { 13, Theme::Comment },
    { 15, Theme::Documentation },
    { .styleId = 16, .textStyle = Theme::BuiltIn, .bold = true },
    { 17, Theme::Annotation },
    { 18, Theme::Error },
    { 19, Theme::DataType },
    { 20, Theme::Others },
    { 21, Theme::Others },
    { 22, Theme::Others },
    { 23, Theme::Normal },
    { 24, Theme::Operator },
};

static constexpr BaseStyleEntry nixStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::Documentation, .spellCheck = true },
    { .styleId = 3, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 4, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { 5, Theme::SpecialChar },
    { 6, Theme::Normal },
    { 7, Theme::Operator },
    { 8, Theme::Others },
    { 9, Theme::DecVal },
    { 10, Theme::Attribute },
    { 11, Theme::Others },
    { .styleId = 12, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 13, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 14, .textStyle = Theme::BuiltIn, .bold = true },
    { .styleId = 15, .textStyle = Theme::Preprocessor, .bold = true },
    { .styleId = 16, .textStyle = Theme::SpecialString, .spellCheck = true },
};

static constexpr BaseStyleEntry nimrodStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::Documentation },
    { 3, Theme::Comment },
    { 4, Theme::Documentation },
    { 5, Theme::DecVal },
    { 6, Theme::String },
    { 7, Theme::Char },
    { .styleId = 8, .textStyle = Theme::Keyword, .bold = true },
    { 9, Theme::VerbatimString },
    { 10, Theme::VerbatimString },
    { 11, Theme::Others },
    { 12, Theme::Function },
    { 13, Theme::SpecialString },
    { 14, Theme::Error },
    { 15, Theme::Operator },
    { 16, Theme::Normal },
};

static constexpr BaseStyleEntry coffeescriptStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::Comment },
    { 3, Theme::Documentation },
    { 4, Theme::DecVal },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true },
    { 6, Theme::String },
    { 7, Theme::Char },
    { 8, Theme::Others },
    { 9, Theme::Preprocessor },
    { 10, Theme::Operator },
    { 11, Theme::Normal },
    { 12, Theme::SpecialString },
    { 13, Theme::VerbatimString },
    { 14, Theme::SpecialString },
    { 15, Theme::Documentation },
    { .styleId = 16, .textStyle = Theme::BuiltIn, .bold = true },
    { 17, Theme::Annotation },
    { 18, Theme::Error },
    { 19, Theme::BuiltIn },
    { 20, Theme::VerbatimString },
    { 21, Theme::VerbatimString },
    { 22, Theme::Comment },
    { 23, Theme::SpecialString },
    { 24, Theme::Comment },
    { 25, Theme::Others },
};

static constexpr BaseStyleEntry dStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::Comment },
    { 3, Theme::Documentation },
    { 4, Theme::Documentation },
    { 5, Theme::DecVal },
    { .styleId = 6, .textStyle = Theme::Keyword, .bold = true },
    { 7, Theme::BuiltIn },
    { 8, Theme::BuiltIn },
    { 9, Theme::DataType },
    { 10, Theme::String },
    { 11, Theme::SpecialString },
    { 12, Theme::Char },
    { 13, Theme::Operator },
    { 14, Theme::Normal },
    { 15, Theme::Documentation },
    { .styleId = 16, .textStyle = Theme::Annotation, .bold = true },
    { 17, Theme::Annotation },
    { 18, Theme::VerbatimString },
    { 19, Theme::VerbatimString },
    { 20, Theme::BuiltIn },
    { 21, Theme::BuiltIn },
    { 22, Theme::BuiltIn },
};

static constexpr BaseStyleEntry erlangStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::Variable },
    { 3, Theme::DecVal },
    { .styleId = 4, .textStyle = Theme::Keyword, .bold = true },
    { 5, Theme::String },
    { 6, Theme::Operator },
    { 7, Theme::Normal },
    { 8, Theme::Function },
    { 9, Theme::Char },
    { 10, Theme::Preprocessor },
    { .styleId = 11, .textStyle = Theme::DataType, .bold = true },
    { 12, Theme::Preprocessor },
    { 13, Theme::Normal },
    { 14, Theme::Documentation },
    { 15, Theme::Documentation },
    { .styleId = 16, .textStyle = Theme::Documentation, .bold = true },
    { .styleId = 17, .textStyle = Theme::Documentation, .bold = true },
    { 18, Theme::Normal },
    { 19, Theme::Preprocessor },
    { 20, Theme::Normal },
    { 21, Theme::Normal },
    { .styleId = 22, .textStyle = Theme::BuiltIn, .bold = true },
    { 23, Theme::BuiltIn },
    { 24, Theme::Preprocessor },
    { 31, Theme::Error },
};

static constexpr BaseStyleEntry nsisStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 3, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 4, .textStyle = Theme::String, .spellCheck = true },
    { 5, Theme::Function },
    { 6, Theme::Variable },
    { 7, Theme::Others },
    { 8, Theme::Others },
    { .styleId = 9, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 10, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 11, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 12, .textStyle = Theme::Keyword, .bold = true },
    { 13, Theme::Variable },
    { 14, Theme::DecVal },
    { .styleId = 15, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 16, .textStyle = Theme::Keyword, .bold = true },
    { 17, Theme::Function },
    { .styleId = 18, .textStyle = Theme::Comment, .spellCheck = true },
};

static constexpr BaseStyleEntry objcStyles[] = {
    { 1, Theme::Comment },
    { 2, Theme::Comment },
    { 3, Theme::Documentation },
    { 4, Theme::DecVal },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true },
    { 6, Theme::String },
    { 7, Theme::Char },
    { 9, Theme::Preprocessor },
    { 10, Theme::Operator },
    { 11, Theme::Normal },
    { 15, Theme::Documentation },
    { .styleId = 16, .textStyle = Theme::DataType, .bold = true },
    { .styleId = 17, .textStyle = Theme::Annotation, .bold = true },
    { 18, Theme::Annotation },
    { .styleId = 19, .textStyle = Theme::Preprocessor, .bold = true },
    { 20, Theme::DataType },
};

static constexpr BaseStyleEntry rStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { .styleId = 2, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 3, .textStyle = Theme::BuiltIn, .bold = true },
    { 4, Theme::BuiltIn },
    { 5, Theme::DecVal },
    { 6, Theme::String },
    { 7, Theme::String },
    { 8, Theme::Operator },
    { 9, Theme::Normal },
    { 10, Theme::Function },
    { 11, Theme::Others },
    { 12, Theme::Others },
    { 13, Theme::VerbatimString },
    { 14, Theme::VerbatimString },
    { 15, Theme::SpecialChar },
};

static constexpr BaseStyleEntry rebolStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::Comment },
    { 3, Theme::Others },
    { 4, Theme::Operator },
    { 5, Theme::Char },
    { 6, Theme::String },
    { 7, Theme::VerbatimString },
    { 8, Theme::DecVal },
    { 9, Theme::Float },
    { 10, Theme::Others },
    { 11, Theme::Others },
    { 12, Theme::Others },
    { 13, Theme::Others },
    { 14, Theme::Keyword },
    { 15, Theme::Others },
    { 16, Theme::Others },
    { 17, Theme::Others },
    { 18, Theme::Others },
    { 19, Theme::Others },
    { 20, Theme::Normal },
    { .styleId = 21, .textStyle = Theme::Keyword, .bold = true },
    { 22, Theme::BuiltIn },
    { .styleId = 23, .textStyle = Theme::DataType, .bold = true },
    { 24, Theme::BuiltIn },
    { 25, Theme::BuiltIn },
    { 26, Theme::BuiltIn },
    { 27, Theme::BuiltIn },
    { 28, Theme::BuiltIn },
};

static constexpr BaseStyleEntry registryStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { .styleId = 2, .textStyle = Theme::Normal, .bold = true },
    { 3, Theme::String },
    { 4, Theme::BaseN },
    { 5, Theme::DataType },
    { 6, Theme::Keyword },
    { 7, Theme::Error },
    { 8, Theme::Others },
    { 9, Theme::Others },
    { 10, Theme::Others },
    { 11, Theme::Others },
    { 12, Theme::Operator },
};

static constexpr BaseStyleEntry schemeStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::DecVal },
    { .styleId = 3, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 4, .textStyle = Theme::Keyword, .bold = true },
    { 5, Theme::Others },
    { 6, Theme::String },
    { 7, Theme::SpecialString },
    { 8, Theme::Error },
    { 9, Theme::Normal },
    { 10, Theme::Operator },
    { 11, Theme::Preprocessor },
    { 12, Theme::Comment },
};

static constexpr BaseStyleEntry vhdlStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::Comment },
    { 3, Theme::DecVal },
    { 4, Theme::String },
    { 5, Theme::Operator },
    { 6, Theme::Normal },
    { 7, Theme::SpecialString },
    { .styleId = 8, .textStyle = Theme::Keyword, .bold = true },
    { 9, Theme::Operator },
    { .styleId = 10, .textStyle = Theme::Attribute, .bold = true },
    { .styleId = 11, .textStyle = Theme::Function, .bold = true },
    { 12, Theme::BuiltIn },
    { .styleId = 13, .textStyle = Theme::DataType, .bold = true },
    { 14, Theme::Normal },
    { 15, Theme::Documentation },
};

static constexpr BaseStyleEntry zigStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::Documentation, .spellCheck = true },
    { .styleId = 3, .textStyle = Theme::Comment, .spellCheck = true },
    { 4, Theme::DecVal },
    { 5, Theme::Operator },
    { 6, Theme::Char },
    { .styleId = 7, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 8, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { 9, Theme::SpecialChar },
    { 10, Theme::Normal },
    { .styleId = 11, .textStyle = Theme::Function, .bold = true },
    { .styleId = 12, .textStyle = Theme::BuiltIn, .bold = true },
    { .styleId = 13, .textStyle = Theme::Keyword, .bold = true },
    { 14, Theme::Keyword },
    { 15, Theme::Keyword },
    { .styleId = 16, .textStyle = Theme::DataType, .bold = true },
    { 17, Theme::Normal },
    { 18, Theme::SpecialString },
};

static constexpr BaseStyleEntry tomlStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { 2, Theme::Normal },
    { .styleId = 3, .textStyle = Theme::Keyword, .bold = true },
    { 4, Theme::DecVal },
    { 5, Theme::DataType },
    { 6, Theme::Attribute },
    { 7, Theme::Error },
    { 8, Theme::Operator },
    { .styleId = 9, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 10, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 11, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 12, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { 13, Theme::SpecialChar },
    { 14, Theme::Others },
    { .styleId = 15, .textStyle = Theme::SpecialString, .spellCheck = true },
};

static constexpr BaseStyleEntry gdscriptStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { 2, Theme::DecVal },
    { .styleId = 3, .textStyle = Theme::String, .spellCheck = true },
    { 4, Theme::Char },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 6, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 7, .textStyle = Theme::VerbatimString, .spellCheck = true },
    { .styleId = 8, .textStyle = Theme::DataType, .bold = true },
    { .styleId = 9, .textStyle = Theme::Function, .bold = true },
    { 10, Theme::Operator },
    { 11, Theme::Normal },
    { .styleId = 12, .textStyle = Theme::Comment, .spellCheck = true },
    { 13, Theme::Error },
    { .styleId = 14, .textStyle = Theme::BuiltIn, .bold = true },
    { 15, Theme::Annotation },
    // 16 is SCE_GD_NODEPATH ($Path/To/Node), not natural-language text.
    { 16, Theme::String },
};

static constexpr BaseStyleEntry poStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::Keyword, .bold = true },
    // The *_TEXT styles carry the actual msgid/msgstr payloads: the primary
    // spell check targets in gettext catalogs.
    { .styleId = 3, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 4, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 5, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 6, .textStyle = Theme::Keyword, .bold = true },
    // 7 is SCE_PO_MSGCTXT_TEXT (context strings, identifier-like): unchecked.
    { 7, Theme::String },
    { 8, Theme::Annotation },
    { .styleId = 9, .textStyle = Theme::Documentation, .spellCheck = true },
    { 10, Theme::Others },
    { 11, Theme::Others },
    { .styleId = 12, .textStyle = Theme::SpecialString, .spellCheck = true },
    { .styleId = 13, .textStyle = Theme::SpecialString, .spellCheck = true },
    { 14, Theme::SpecialString },
    { 15, Theme::Error },
};

static constexpr BaseStyleEntry innoStyles[] = {
    { 0, Theme::Normal },
    { .styleId = 1, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 2, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 3, .textStyle = Theme::Attribute, .bold = true },
    { .styleId = 4, .textStyle = Theme::Preprocessor, .bold = true },
    { .styleId = 5, .textStyle = Theme::Preprocessor, .bold = true },
    { 6, Theme::Others },
    { .styleId = 7, .textStyle = Theme::Comment, .spellCheck = true },
    { .styleId = 8, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 9, .textStyle = Theme::Function, .bold = true },
    { .styleId = 10, .textStyle = Theme::String, .spellCheck = true },
    { .styleId = 11, .textStyle = Theme::String, .spellCheck = true },
    { 12, Theme::Normal },
};

static constexpr BaseStyleEntry verilogStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::Comment },
    { 3, Theme::Comment },
    { 4, Theme::DecVal },
    { .styleId = 5, .textStyle = Theme::Keyword, .bold = true },
    { 6, Theme::String },
    { 7, Theme::DataType },
    { 8, Theme::BuiltIn },
    { 9, Theme::Preprocessor },
    { .styleId = 10, .textStyle = Theme::Operator, .bold = true },
    { 11, Theme::Normal },
    { 12, Theme::SpecialString },
    { 19, Theme::Others },
    { 20, Theme::Annotation },
    { 21, Theme::Others },
    { 22, Theme::Others },
    { 23, Theme::Others },
    { 24, Theme::Others },
};

static constexpr BaseStyleEntry kixStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::String },
    { 3, Theme::String },
    { 4, Theme::DecVal },
    { 5, Theme::Variable },
    { 6, Theme::BuiltIn },
    { .styleId = 7, .textStyle = Theme::Keyword, .bold = true },
    { 8, Theme::Function },
    { .styleId = 9, .textStyle = Theme::Operator, .bold = true },
    { 10, Theme::Comment },
    { 31, Theme::Normal },
};

static constexpr BaseStyleEntry gui4cliStyles[] = {
    { 0, Theme::Normal },
    { 1, Theme::Comment },
    { 2, Theme::Comment },
    { .styleId = 3, .textStyle = Theme::Keyword, .bold = true },
    { .styleId = 4, .textStyle = Theme::ControlFlow, .bold = true },
    { 5, Theme::Attribute },
    { 6, Theme::ControlFlow },
    { 7, Theme::Function },
    { .styleId = 9, .textStyle = Theme::Operator, .bold = true },
};

static constexpr BaseStyleEntry csoundStyles[] = {
    { 0, Theme::Normal },                                            // DEFAULT
    { 1, Theme::Comment },                                           // COMMENT (;)
    { 2, Theme::DecVal },                                            // NUMBER
    { 3, Theme::Operator },                                          // OPERATOR
    { 4, Theme::Keyword },                                           // INSTR (instr/endin blocks)
    { 5, Theme::Normal },                                            // IDENTIFIER
    { .styleId = 6, .textStyle = Theme::Keyword, .bold = true },     // OPCODE
    { 7, Theme::Preprocessor },                                      // HEADERSTMT (sr/kr/ksmps...)
    { 8, Theme::Others },                                            // USERKEYWORD
    { 9, Theme::Comment },                                           // COMMENTBLOCK
    { .styleId = 10, .textStyle = Theme::Variable, .bold = true },   // PARAM (p-fields)
    { 11, Theme::Variable },                                         // ARATE_VAR
    { 12, Theme::Variable },                                         // KRATE_VAR
    { 13, Theme::Variable },                                         // IRATE_VAR
    { 14, Theme::Variable },                                         // GLOBAL_VAR
    { 15, Theme::Error },                                            // STRINGEOL
};

struct LexerTable { std::u16string_view name; std::span<const BaseStyleEntry> entries; };

static constexpr LexerTable kTables[] = {
    { u"cpp", cppStyles },
    { u"hypertext", hypertextStyles },
    { u"xml", xmlStyles },
    { u"python", pythonStyles },
    { u"bash", bashStyles },
    { u"ksyntaxhighlighting", ksyntaxhighlightingStyles },
    { u"avs", avsStyles },
    { u"yaml", yamlStyles },
    { u"json", jsonStyles },
    { u"css", cssStyles },
    { u"cmake", cmakeStyles },
    { u"props", propsStyles },
    { u"batch", batchStyles },
    { u"lua", luaStyles },
    { u"makefile", makefileStyles },
    { u"logplus", logplusStyles },
    { u"searchResult", searchResultStyles },
    { u"diff", diffStyles },
    { u"ruby", rubyStyles },
    { u"rust", rustStyles },
    { u"tcl", tclStyles },
    { u"pascal", pascalStyles },
    { u"powershell", powershellStyles },
    { u"asm", asmStyles },
    { u"fortran", fortranStyles },
    { u"caml", camlStyles },
    { u"haskell", haskellStyles },
    { u"asciidoc", asciidocStyles },
    { u"dart", dartStyles },
    { u"julia", juliaStyles },
    { u"lisp", lispStyles },
    { u"latex", latexStyles },
    { u"vb", vbStyles },
    { u"perl", perlStyles },
    { u"sql", sqlStyles },
    { u"nix", nixStyles },
    { u"nimrod", nimrodStyles },
    { u"coffeescript", coffeescriptStyles },
    { u"d", dStyles },
    { u"erlang", erlangStyles },
    { u"nsis", nsisStyles },
    { u"objc", objcStyles },
    { u"r", rStyles },
    { u"rebol", rebolStyles },
    { u"registry", registryStyles },
    { u"scheme", schemeStyles },
    { u"vhdl", vhdlStyles },
    { u"zig", zigStyles },
    { u"toml", tomlStyles },
    { u"gdscript", gdscriptStyles },
    { u"po", poStyles },
    { u"inno", innoStyles },
    { u"verilog", verilogStyles },
    { u"kix", kixStyles },
    { u"gui4cli", gui4cliStyles },
    { u"csound", csoundStyles },
};

} // namespace

namespace BaseStyles {
const QHash<int, StyleSpec> *stylesForLexer(QStringView lexerName)
{
    static const QHash<QString, QHash<int, StyleSpec>> cache = []() {
        QHash<QString, QHash<int, StyleSpec>> m;
        for (const LexerTable &t : kTables) {
            const QString name = QString::fromUtf16(t.name.data(), int(t.name.size()));
            QHash<int, StyleSpec> &h = m[name];
            for (const BaseStyleEntry &e : t.entries) {
                StyleSpec s;
                s.textStyle = e.textStyle;
                s.bold = e.bold;
                s.italic = e.italic;
                s.underline = e.underline;
                s.eolFilled = e.eolFilled;
                s.spellCheck = e.spellCheck;
                h.insert(e.styleId, s);
            }
        }
        return m;
    }();

    const auto it = cache.constFind(lexerName.toString());
    if (it == cache.constEnd())
        return nullptr;
    return &it.value();
}

QStringList knownLexers()
{
    static const QStringList names = []() {
        QStringList list;
        for (const LexerTable &t : kTables) {
            const QString name = QString::fromUtf16(t.name.data(), int(t.name.size()));
            if (name == u"ksyntaxhighlighting")
                continue;
            list.append(name);
        }
        list.sort(Qt::CaseInsensitive);
        return list;
    }();
    return names;
}
} // namespace BaseStyles

