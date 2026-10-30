#include "QRegularExpressionSearch.h"

#include <QRegularExpression>
#include <QByteArray>

#include <string>

using namespace Scintilla;

#ifdef SCI_OWNREGEX
RegexSearchBase *Scintilla::Internal::CreateRegexSearch(CharClassify *charClassTable)
{
    Q_UNUSED(charClassTable);
    return new QRegularExpressionSearch();
}
#endif

QRegularExpressionSearch::QRegularExpressionSearch()
{
}

QRegularExpressionSearch::~QRegularExpressionSearch()
{
}

void QRegularExpressionSearch::compileRegex(const char *pattern, QRegularExpression::PatternOptions options)
{
    const std::string patternStr(pattern);
    if (patternStr != m_lastPattern || options != m_lastOptions) {
        m_regex = QRegularExpression(QString::fromUtf8(pattern), options);
        m_lastPattern = patternStr;
        m_lastOptions = options;
        if (m_regex.isValid())
            m_regex.optimize();
    }
}

Sci::Position QRegularExpressionSearch::FindText(Document *doc, Sci::Position minPos, Sci::Position maxPos,
    const char *s, bool caseSensitive, bool word, bool wordStart,
    Scintilla::FindOption flags, Sci::Position *length)
{
    Q_UNUSED(caseSensitive);
    Q_UNUSED(word);
    Q_UNUSED(wordStart);

    // Ensure positions are not in the middle of a multi-byte character
    minPos = doc->MovePositionOutsideChar(minPos, 1, false);
    maxPos = doc->MovePositionOutsideChar(maxPos, -1, false);

    if (minPos == maxPos)
        return -1;

    auto options = QRegularExpression::MultilineOption
        | QRegularExpression::UseUnicodePropertiesOption;

    if (!FlagSet(flags, FindOption::MatchCase))
        options |= QRegularExpression::CaseInsensitiveOption;

    compileRegex(s, options);
    if (!m_regex.isValid())
        throw RegexError();

    const Sci::Position rangeLength = maxPos - minPos;
    const QString subject = QString::fromUtf8(doc->RangePointer(minPos, rangeLength), static_cast<int>(rangeLength));

    QRegularExpressionMatch m = m_regex.match(subject, 0, QRegularExpression::NormalMatch, QRegularExpression::NoMatchOption);

    if (!m.hasMatch())
        return -1;

    m_match = m;

    // Convert UTF-16 match positions back to Scintilla byte positions
    const int positionStart = doc->GetRelativePositionUTF16(minPos, m_match.capturedStart(0));
    const int positionEnd = doc->GetRelativePositionUTF16(positionStart, m_match.capturedLength(0));

    *length = positionEnd - positionStart;

    return positionStart;
}

const char *QRegularExpressionSearch::SubstituteByPosition(Document *doc, const char *text, Sci::Position *length)
{
    Q_UNUSED(doc);

    Q_ASSERT(m_match.isValid());
    Q_ASSERT(m_match.hasMatch());

    QString newString = m_match.captured();
    newString.replace(m_match.regularExpression(), QByteArray(text, static_cast<int>(*length)));

    m_substituted = newString.toUtf8();
    *length = m_substituted.length();
    return m_substituted.data();
}
