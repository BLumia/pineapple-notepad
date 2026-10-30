// SPDX-License-Identifier: MIT
#include "spellcheckcontroller.h"
#include "sciedit.h"
#include "textutil.h"

#include <QLoggingCategory>
#include <QTimer>

#include <algorithm>

Q_LOGGING_CATEGORY(lcSpellCheck, "pnotepad.spellcheck")

namespace {

// A word must start with a letter; letters plus internal apostrophes
// (straight ' and typographic U+2019) continue it.
bool isWordStartChar(QChar c)
{
    return c.isLetter();
}

bool isWordContinuationChar(QChar c)
{
    return c.isLetter() || c == QLatin1Char('\'') || c == QChar(0x2019);
}

// Skip single characters and ALL-CAPS words (acronyms such as NASA or TODO);
// anything else is worth a dictionary lookup.
bool worthChecking(const QString &word)
{
    if (word.size() < 2) {
        return false;
    }
    for (const QChar &c : word) {
        if (c.isLower()) {
            return true;
        }
    }
    return false;
}

} // namespace

SpellCheckController::SpellCheckController(SciEdit *editor, QObject *parent)
    : QObject(parent)
    , m_editor(editor)
{
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setInterval(300);
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this, &SpellCheckController::scanNow);
}

void SpellCheckController::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    if (!m_enabled) {
        m_debounceTimer->stop();
        m_forceOnResume = false;
        clearAll();
    } else {
        requestScan();
    }
}

bool SpellCheckController::enabled() const
{
    return m_enabled;
}

Sonnet::Speller &SpellCheckController::speller()
{
    return m_speller;
}

void SpellCheckController::refreshSpeller()
{
    m_speller = Sonnet::Speller();
    if (m_enabled) {
        clearAll();
        requestScan();
    }
}

void SpellCheckController::requestScan()
{
    if (!m_enabled || m_suspendCount > 0) {
        return;
    }
    m_debounceTimer->start();
}

void SpellCheckController::scanNow()
{
    if (!m_enabled || m_suspendCount > 0 || !m_editor || m_editor->isLargeFile()) {
        return;
    }

    m_editor->setIndicatorCurrent(SciEdit::kSpellCheckIndicatorId);

    // Whole lines covered by the viewport, same model as
    // SciEdit::scanVisibleUrls().
    const sptr_t firstLine = m_editor->firstVisibleLine();
    sptr_t visibleLines = m_editor->linesOnScreen();
    if (visibleLines <= 0) {
        visibleLines = 1;
    }
    const sptr_t lastLine = std::min<sptr_t>(firstLine + visibleLines - 1, m_editor->lineCount() - 1);
    if (lastLine < firstLine) {
        return;
    }
    const sptr_t startPos = m_editor->positionFromLine(firstLine);
    const sptr_t endPos = m_editor->lineEndPosition(lastLine);
    if (endPos <= startPos) {
        return;
    }

    m_editor->indicatorClearRange(startPos, endPos - startPos);

    if (!m_speller.isValid()) {
        return;
    }

    const QByteArray bytes = m_editor->get_text_range(int(startPos), int(endPos));
    const Utf8DecodedText decoded = decodeUtf8WithBytePositions(bytes);
    const int charCount = decoded.chars.size();
    if (charCount == 0) {
        return;
    }

    int i = 0;
    QString word;
    while (i < charCount) {
        if (!isWordStartChar(decoded.chars.at(i))) {
            ++i;
            continue;
        }
        const int wordStart = i;
        while (i < charCount && isWordContinuationChar(decoded.chars.at(i))) {
            ++i;
        }
        // Trailing apostrophes are not part of the word ("isn't'" -> "isn't").
        int wordEnd = i;
        while (wordEnd > wordStart
               && (decoded.chars.at(wordEnd - 1) == QLatin1Char('\'')
                   || decoded.chars.at(wordEnd - 1) == QChar(0x2019))) {
            --wordEnd;
        }
        if (wordEnd - wordStart < 2 || wordEnd >= decoded.bytePositions.size()) {
            continue;
        }

        word.clear();
        for (int k = wordStart; k < wordEnd; ++k) {
            word.append(decoded.chars.at(k));
        }
        if (!worthChecking(word)) {
            continue;
        }

        const sptr_t docPos = startPos + decoded.bytePositions.at(wordStart);
        // Skip words that are part of a detected URL.
        if (m_editor->indicatorValueAt(SciEdit::kUrlHoverIndicatorId, docPos)) {
            continue;
        }
        if (!m_editor->styleNeedsSpellCheck(int(m_editor->styleAt(docPos)))) {
            continue;
        }

        if (!m_speller.isCorrect(word)) {
            const sptr_t wordEndPos = startPos + decoded.bytePositions.at(wordEnd);
            m_editor->indicatorFillRange(docPos, wordEndPos - docPos);
        }
    }
}

void SpellCheckController::suspendBatch()
{
    ++m_suspendCount;
    m_forceOnResume = true;
}

void SpellCheckController::resumeBatch()
{
    if (m_suspendCount <= 0) {
        return;
    }
    --m_suspendCount;
    if (m_suspendCount == 0 && m_forceOnResume) {
        m_forceOnResume = false;
        if (m_enabled) {
            m_debounceTimer->start();
        }
    }
}

void SpellCheckController::textModified(sptr_t position, sptr_t length)
{
    if (!m_enabled || !m_editor) {
        return;
    }

    m_editor->setIndicatorCurrent(SciEdit::kSpellCheckIndicatorId);
    const sptr_t docLength = m_editor->length();
    const sptr_t clampedStart = std::clamp<sptr_t>(position, 0, docLength);
    const sptr_t clampedEnd = std::clamp<sptr_t>(position + std::max<sptr_t>(length, 1), 0, docLength);
    const sptr_t firstLine = m_editor->lineFromPosition(clampedStart);
    const sptr_t lastLine = m_editor->lineFromPosition(clampedEnd);
    const sptr_t startPos = m_editor->positionFromLine(firstLine);
    const sptr_t endPos = m_editor->lineEndPosition(lastLine);
    if (endPos > startPos) {
        m_editor->indicatorClearRange(startPos, endPos - startPos);
    }

    requestScan();
}

void SpellCheckController::clearAll()
{
    if (!m_editor) {
        return;
    }
    m_editor->setIndicatorCurrent(SciEdit::kSpellCheckIndicatorId);
    m_editor->indicatorClearRange(0, m_editor->length());
}
