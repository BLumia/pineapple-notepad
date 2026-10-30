// SPDX-License-Identifier: MIT
// Shared lexer metadata query logic for CLI and GUI tools.

#include "lexer_query.h"

#include <cctype>

#include "ILexer.h"
#include "Lexilla.h"

// Auto-generated fallback style data for old-style lexers
#include "fallback_styles.cpp"

namespace LexerQuery {

const char* propertyTypeName(int type) {
    switch (type) {
    case 0: return "boolean";
    case 1: return "integer";
    case 2: return "string";
    default: return "unknown";
    }
}

std::vector<std::string> splitByNewline(const char* text) {
    std::vector<std::string> result;
    if (!text || !*text)
        return result;
    const char* p = text;
    while (*p) {
        const char* start = p;
        while (*p && *p != '\n')
            ++p;
        result.emplace_back(start, p - start);
        if (*p == '\n')
            ++p;
    }
    return result;
}

std::vector<std::string> enumerateLexerNames() {
    std::vector<std::string> names;
    int count = GetLexerCount();
    names.reserve(count);
    for (int i = 0; i < count; ++i) {
        char name[200]{};
        GetLexerName(i, name, sizeof(name));
        names.emplace_back(name);
    }
    return names;
}

static const FallbackLexerData* findFallbackData(const std::string& lexerName) {
    auto it = g_fallbackData.find(lexerName);
    if (it != g_fallbackData.end())
        return &it->second;
    for (auto& kv : g_fallbackData) {
        if (kv.first.size() == lexerName.size()) {
            bool match = true;
            for (size_t ci = 0; ci < kv.first.size(); ++ci) {
                if (std::tolower(static_cast<unsigned char>(kv.first[ci])) !=
                    std::tolower(static_cast<unsigned char>(lexerName[ci]))) {
                    match = false;
                    break;
                }
            }
            if (match)
                return &kv.second;
        }
    }
    return nullptr;
}

LexerMetadata queryLexerMetadata(const std::string& lexerName) {
    LexerMetadata meta;
    meta.name = lexerName;
    meta.identifier = 0;
    meta.totalStyleSlots = 0;
    meta.usesFallbackStyles = false;

    Scintilla::ILexer5* lexer = CreateLexer(lexerName.c_str());
    if (!lexer)
        return meta;

    meta.name = lexer->GetName();
    meta.identifier = lexer->GetIdentifier();

    // Properties
    for (auto& pn : splitByNewline(lexer->PropertyNames())) {
        Property prop;
        prop.name = std::move(pn);
        prop.type = lexer->PropertyType(prop.name.c_str());
        prop.typeName = propertyTypeName(prop.type);
        const char* d = lexer->DescribeProperty(prop.name.c_str());
        prop.description = d ? d : "";
        const char* v = lexer->PropertyGet(prop.name.c_str());
        prop.defaultValue = (v && *v) ? v : "";
        meta.properties.push_back(std::move(prop));
    }

    // Keyword lists
    auto wld = splitByNewline(lexer->DescribeWordListSets());
    for (size_t i = 0; i < wld.size(); ++i) {
        meta.keywordLists.push_back({static_cast<int>(i), std::move(wld[i])});
    }

    // Styles (from ILexer5)
    int numStyles = lexer->NamedStyles();
    meta.totalStyleSlots = numStyles;
    for (int i = 0; i < numStyles; ++i) {
        const char* n = lexer->NameOfStyle(i);
        const char* t = lexer->TagsOfStyle(i);
        const char* d = lexer->DescriptionOfStyle(i);
        bool hasName = n && *n;
        bool hasTags = t && *t;
        bool hasDesc = d && *d;
        if (!hasName && !hasTags && !hasDesc)
            continue;
        Style s;
        s.id = i;
        s.name = hasName ? n : "";
        s.tags = hasTags ? t : "";
        s.description = hasDesc ? d : "";
        meta.styles.push_back(std::move(s));
    }

    // Fallback for old-style lexers
    if (meta.styles.empty() || meta.properties.empty()) {
        const FallbackLexerData* fallback = findFallbackData(lexerName);
        if (fallback) {
            if (meta.styles.empty()) {
                meta.usesFallbackStyles = true;
                meta.totalStyleSlots = 0;
                for (auto& fs : fallback->styles) {
                    if (fs.id >= meta.totalStyleSlots)
                        meta.totalStyleSlots = fs.id + 1;
                    Style s;
                    s.id = fs.id;
                    s.name = fs.name;
                    meta.styles.push_back(std::move(s));
                }
            }
            if (meta.properties.empty()) {
                for (auto& fp : g_commonFoldProps) {
                    Property prop;
                    prop.name = fp.name;
                    prop.typeName = fp.typeName;
                    prop.description = fp.description;
                    prop.type = 0;
                    meta.properties.push_back(std::move(prop));
                }
            }
        }
    }

    lexer->Release();
    return meta;
}

} // namespace LexerQuery
