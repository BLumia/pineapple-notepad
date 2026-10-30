#pragma once

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QByteArray>

#include <cassert>
#include <forward_list>
#include <set>
#include <string>

#include "ScintillaTypes.h"
#include "ScintillaMessages.h"
#include "ScintillaStructures.h"
#include "ILoader.h"
#include "ILexer.h"

#include "Debugging.h"
#include "Geometry.h"
#include "Platform.h"

#include "CharacterType.h"
#include "CharacterCategoryMap.h"
#include "Position.h"
#include "UniqueString.h"
#include "SplitVector.h"
#include "Partitioning.h"
#include "RunStyles.h"
#include "ContractionState.h"
#include "CellBuffer.h"
#include "PerLine.h"
#include "KeyMap.h"
#include "Indicator.h"
#include "LineMarker.h"
#include "Style.h"
#include "ViewStyle.h"
#include "CharClassify.h"
#include "Decoration.h"
#include "CaseFolder.h"
#include "Document.h"

using namespace Scintilla::Internal;

class QRegularExpressionSearch : public RegexSearchBase
{
public:
    QRegularExpressionSearch();
    ~QRegularExpressionSearch() override;

    Sci::Position FindText(Document *doc, Sci::Position minPos, Sci::Position maxPos,
        const char *s, bool caseSensitive, bool word, bool wordStart,
        Scintilla::FindOption flags, Sci::Position *length) override;

    const char *SubstituteByPosition(Document *doc, const char *text,
        Sci::Position *length) override;

private:
    void compileRegex(const char *pattern, QRegularExpression::PatternOptions options);

    QRegularExpression m_regex;
    std::string m_lastPattern;
    QRegularExpression::PatternOptions m_lastOptions{};

    QRegularExpressionMatch m_match;
    QByteArray m_substituted;
};
