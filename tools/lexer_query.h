// SPDX-License-Identifier: MIT
// Shared lexer metadata query logic for CLI and GUI tools.

#ifndef LEXER_QUERY_H
#define LEXER_QUERY_H

#include <string>
#include <vector>

namespace LexerQuery {

struct Property {
    std::string name;
    int type;               // SC_TYPE_BOOLEAN=0, SC_TYPE_INTEGER=1, SC_TYPE_STRING=2
    std::string typeName;
    std::string description;
    std::string defaultValue;
};

struct KeywordList {
    int index;
    std::string description;
};

struct Style {
    int id;
    std::string name;
    std::string tags;
    std::string description;
};

struct LexerMetadata {
    std::string name;
    int identifier;
    std::vector<Property> properties;
    std::vector<KeywordList> keywordLists;
    std::vector<Style> styles;
    int totalStyleSlots;
    bool usesFallbackStyles;  // true = old-style lexer (styles from SciLexer.h)
};

const char* propertyTypeName(int type);
std::vector<std::string> splitByNewline(const char* text);
std::vector<std::string> enumerateLexerNames();
LexerMetadata queryLexerMetadata(const std::string& lexerName);

} // namespace LexerQuery

#endif // LEXER_QUERY_H
