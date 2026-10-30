// SPDX-License-Identifier: MIT
// Lexer Info Tool - CLI version. Displays properties, keywords, and styles for all Lexilla lexers.
//
// Usage:
//   lexer_info                      Interactive mode (default)
//   lexer_info <name> [<name>...]   Show info for named lexer(s)
//   lexer_info --all                Show info for all lexers
//   lexer_info --list               List all lexer names

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "lexer_query.h"

static void printLexerInfo(const LexerQuery::LexerMetadata& meta) {
    std::printf("\n");
    std::printf("========================================\n");
    std::printf(" Lexer: %s\n", meta.name.c_str());
    std::printf(" Identifier: %d\n", meta.identifier);
    std::printf(" Type: %s\n", meta.usesFallbackStyles ? "old-style (styles from SciLexer.h)" : "new-style (ILexer5 metadata)");
    std::printf("========================================\n");

    // --- Properties ---
    std::printf("\n--- Properties (%zu) ---\n", meta.properties.size());
    if (meta.properties.empty()) {
        std::printf("  (none)\n");
    } else {
        for (const auto& prop : meta.properties) {
            std::printf("  %s\n", prop.name.c_str());
            std::printf("    Type:        %s\n", prop.typeName.c_str());
            std::printf("    Description: %s\n", prop.description.empty() ? "(none)" : prop.description.c_str());
            std::printf("    Default:     %s\n", prop.defaultValue.empty() ? "(empty)" : prop.defaultValue.c_str());
        }
    }

    // --- Keyword Lists ---
    std::printf("\n--- Keyword Lists (%zu) ---\n", meta.keywordLists.size());
    if (meta.keywordLists.empty()) {
        std::printf("  (none)\n");
    } else {
        for (const auto& kw : meta.keywordLists) {
            std::printf("  [%d] %s\n", kw.index, kw.description.c_str());
        }
    }

    // --- Styles ---
    int effective = static_cast<int>(meta.styles.size());
    std::printf("\n--- Styles (%d", meta.totalStyleSlots);
    if (effective != meta.totalStyleSlots)
        std::printf(", %d defined", effective);
    std::printf(") ---\n");
    for (const auto& s : meta.styles) {
        std::printf("  [%3d] %-30s", s.id, s.name.empty() ? "(unnamed)" : s.name.c_str());
        if (!s.tags.empty())
            std::printf("  <%s>", s.tags.c_str());
        std::printf("\n");
        if (!s.description.empty())
            std::printf("        %s\n", s.description.c_str());
    }
}

static void printUsage(const char* argv0) {
    std::fprintf(stderr, "Usage:\n");
    std::fprintf(stderr, "  %s                     Interactive mode (default)\n", argv0);
    std::fprintf(stderr, "  %s <name> [<name>...]  Show info for named lexer(s)\n", argv0);
    std::fprintf(stderr, "  %s --all               Show info for all lexers\n", argv0);
    std::fprintf(stderr, "  %s --list              List all lexer names\n", argv0);
}

static bool findLexer(const std::vector<std::string>& names, const std::string& target) {
    for (const auto& n : names) {
        if (n == target)
            return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    auto lexerNames = LexerQuery::enumerateLexerNames();
    int count = static_cast<int>(lexerNames.size());

    // --- Non-interactive modes ---
    if (argc > 1) {
        if (std::strcmp(argv[1], "--list") == 0) {
            for (int i = 0; i < count; ++i) {
                std::printf("%s\n", lexerNames[i].c_str());
            }
            return 0;
        }

        if (std::strcmp(argv[1], "--all") == 0) {
            for (int i = 0; i < count; ++i) {
                auto meta = LexerQuery::queryLexerMetadata(lexerNames[i]);
                printLexerInfo(meta);
            }
            return 0;
        }

        // Treat each argument as a lexer name
        int failed = 0;
        for (int a = 1; a < argc; ++a) {
            if (!findLexer(lexerNames, argv[a])) {
                std::fprintf(stderr, "Unknown lexer: %s\n", argv[a]);
                ++failed;
                continue;
            }
            auto meta = LexerQuery::queryLexerMetadata(argv[a]);
            printLexerInfo(meta);
        }
        return failed > 0 ? 1 : 0;
    }

    // --- Interactive mode ---
    std::printf("=== Lexilla Lexer Info Tool ===\n");
    std::printf("Total lexers: %d\n\n", count);

    for (int i = 0; i < count; ++i) {
        std::printf("  [%3d] %s\n", i, lexerNames[i].c_str());
    }

    std::printf("\nEnter lexer number (0-%d) to view details, or 'q' to quit.\n", count - 1);
    std::printf("Enter 'a' to show all lexers.\n\n");

    char input[64]{};
    while (true) {
        std::printf("> ");
        if (!std::fgets(input, sizeof(input), stdin))
            break;

        size_t len = std::strlen(input);
        while (len > 0 && (input[len - 1] == '\n' || input[len - 1] == '\r'))
            input[--len] = '\0';

        if (len == 0)
            continue;
        if (input[0] == 'q' || input[0] == 'Q')
            break;

        if (input[0] == 'a' || input[0] == 'A') {
            for (int i = 0; i < count; ++i) {
                auto meta = LexerQuery::queryLexerMetadata(lexerNames[i]);
                printLexerInfo(meta);
            }
            continue;
        }

        char* end = nullptr;
        long idx = std::strtol(input, &end, 10);
        if (end == input || idx < 0 || idx >= count) {
            std::printf("Invalid input. Enter a number 0-%d, 'a' for all, or 'q' to quit.\n", count - 1);
            continue;
        }

        auto meta = LexerQuery::queryLexerMetadata(lexerNames[idx]);
        printLexerInfo(meta);
    }

    return 0;
}
