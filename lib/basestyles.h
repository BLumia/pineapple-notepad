// SPDX-FileCopyrightText: 2026 Gary Wang <git@blumia.net>
//
// Compile-time lexilla style id -> KSyntaxHighlighting::Theme::TextStyle mapping
// tables. The implementation (data + lookup) lives in basestyles.cpp.

#pragma once

#include "languageconfigstore.h"

#include <QStringList>
#include <QStringView>

namespace BaseStyles
{
// Returns the cached style mapping for the given lexer name, or nullptr if the
// lexer has no entry in the static tables. The returned pointer points into a
// function-local static cache and remains valid for the program lifetime.
const QHash<int, StyleSpec> *stylesForLexer(QStringView lexerName);

// Names of all lexers with a style mapping in the static tables (sorted,
// excluding the KSH compat lexer), i.e. the lexers usable as a base for
// user defined languages.
QStringList knownLexers();
}
