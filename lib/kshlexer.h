#pragma once

#include <ILexer.h>

namespace KSyntaxHighlighting
{
class Repository;
}

Scintilla::ILexer5 *createKshLexer();

namespace kshutils {
KSyntaxHighlighting::Repository &repository();
}
