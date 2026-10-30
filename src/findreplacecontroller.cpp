#include "findreplacecontroller.h"

#include "sciedit.h"
#include "findreplacedialog.h"
#include "findinfilessearcher.h"
#include "searchresultsdock.h"
#include "documentmanager.h"
#include "editorviewmanager.h"
#include "appsettings.h"

#include <utility>

#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProgressDialog>

#include <algorithm>

#include <Scintilla.h>
#include <ScintillaTypes.h>

static bool checkAndClearRegexError(SciEdit *editor)
{
    if (editor->status() == static_cast<sptr_t>(Scintilla::Status::RegEx)) {
        editor->setStatus(0);
        return true;
    }
    return false;
}

// Unconditionally clear any lingering RegEx error status from a previous
// operation, so that a subsequent checkAndClearRegexError() only reflects
// errors produced by the current operation.
static void clearRegexError(SciEdit *editor)
{
    if (editor->status() == static_cast<sptr_t>(Scintilla::Status::RegEx))
        editor->setStatus(0);
}

static sptr_t buildSearchFlags(const FindReplaceDialog *dialog)
{
    sptr_t flags = 0;
    if (dialog->matchCase()) {
        flags |= SCFIND_MATCHCASE;
    }
    if (dialog->wholeWord()) {
        flags |= SCFIND_WHOLEWORD;
    }
    if (dialog->searchMode() == SearchMode::Regex) {
        flags |= SCFIND_REGEXP;
    }
    return flags;
}

static QString buildFindPattern(const FindReplaceDialog *dialog)
{
    return dialog->findTextForSearch();
}

static QString buildReplacementText(const FindReplaceDialog *dialog)
{
    return dialog->replaceTextForReplace();
}

static QString truncateLine(const QString &lineStr, int relStart, int relEnd, int maxLen, int &displayStart, int &displayEnd)
{
    if (lineStr.length() <= maxLen) {
        displayStart = relStart;
        displayEnd = relEnd;
        return lineStr;
    }
    const int center = (relStart + relEnd) / 2;
    const int halfMax = maxLen / 2;
    int showStart = qMax(0, center - halfMax);
    int showEnd = qMin(lineStr.length(), showStart + maxLen);
    if (showEnd - showStart < maxLen) {
        showStart = qMax(0, showEnd - maxLen);
    }
    displayStart = relStart - showStart;
    displayEnd = relEnd - showStart;
    return lineStr.mid(showStart, showEnd - showStart);
}

FindReplaceController::FindReplaceController(EditorProvider editorProvider, QObject *parent)
    : QObject(parent)
    , m_editorProvider(std::move(editorProvider))
{
}

FindReplaceController::~FindReplaceController() = default;

void FindReplaceController::showDialog(bool showReplace)
{
    ensureDialog();
    prepareDialogCommon();

    if (showReplace) {
        m_dialog->openReplace();
    } else {
        m_dialog->openFind();
    }
}

void FindReplaceController::showFindInFilesDialog()
{
    ensureDialog();
    prepareDialogCommon();
    m_dialog->openFindInFiles();
}

void FindReplaceController::showMarkDialog()
{
    ensureDialog();
    prepareDialogCommon();
    m_dialog->openMark();
}

void FindReplaceController::prepareDialogCommon()
{
    // Fill the find field with the selection: when nothing is selected (and
    // the sub-option allows it), fall back to the word under the caret; either
    // way the text must stay within the configured character limit.
    if (AppSettings::fillFindFieldWithSelection()) {
        if (SciEdit *editor = m_editorProvider()) {
            const sptr_t selStart = editor->selectionStart();
            const sptr_t selEnd = editor->selectionEnd();

            QString text;
            if (selStart != selEnd) {
                const sptr_t start = qMin(selStart, selEnd);
                const sptr_t end = qMax(selStart, selEnd);
                text = QString::fromUtf8(editor->textRange(start, end));
            } else if (AppSettings::fillFindFieldWithWordAtCaret()) {
                const sptr_t pos = editor->currentPos();
                const sptr_t wordStart = editor->wordStartPosition(pos, true);
                const sptr_t wordEnd = editor->wordEndPosition(pos, true);
                if (wordEnd > wordStart) {
                    text = QString::fromUtf8(editor->textRange(wordStart, wordEnd));
                }
            }

            if (!text.isEmpty() && text.size() <= AppSettings::fillFindFieldMaxChars()) {
                m_dialog->setFindTextIfNotEmpty(text);
            }
        }
    }

    if (m_viewManager && m_documentManager) {
        const int docId = m_viewManager->currentDocumentId();
        if (docId >= 0) {
            const QString filePath = m_documentManager->getDocumentFilePath(docId);
            if (!filePath.isEmpty()) {
                m_dialog->setCurrentDocumentDirectory(QFileInfo(filePath).absolutePath());
            }
        }
    }
}

void FindReplaceController::ensureDialog()
{
    if (m_dialog) {
        return;
    }

    m_dialog = new FindReplaceDialog(qobject_cast<QWidget *>(parent()));
    connect(m_dialog, &FindReplaceDialog::findRequested, this, [this](bool forward) {
        findNext(forward, true);
    });
    connect(m_dialog, &FindReplaceDialog::replaceRequested, this, [this]() {
        replaceOne(true);
    });
    connect(m_dialog, &FindReplaceDialog::replaceAllRequested, this, [this]() {
        replaceAll(true);
    });
    connect(m_dialog, &FindReplaceDialog::dialogClosed, this, [this]() {
        if (SciEdit *editor = m_editorProvider()) {
            editor->clearFindResultHighlight();
        }
    });
    connect(m_dialog, &FindReplaceDialog::countRequested, this, [this]() {
        countMatches();
    });
    connect(m_dialog, &FindReplaceDialog::findAllInCurrentDocumentRequested, this, [this]() {
        findAllInCurrentDocument();
    });
    connect(m_dialog, &FindReplaceDialog::findAllInAllDocumentsRequested, this, [this]() {
        findAllInAllDocuments();
    });
    connect(m_dialog, &FindReplaceDialog::replaceAllInAllDocumentsRequested, this, [this]() {
        replaceAllInAllDocuments();
    });
    connect(m_dialog, &FindReplaceDialog::findInFilesRequested, this, [this]() {
        findInFiles();
    });
    connect(m_dialog, &FindReplaceDialog::markAllRequested, this, [this]() {
        markAll();
    });
    connect(m_dialog, &FindReplaceDialog::clearMarksRequested, this, [this]() {
        clearMarks();
    });
    connect(m_dialog, &FindReplaceDialog::copyMarkedTextRequested, this, [this]() {
        copyMarkedText();
    });
}

QPair<int, int> FindReplaceController::resolveSearchRange(SciEdit *editor)
{
    const int docLen = static_cast<int>(editor->textLength());

    if (m_dialog->inSelection()) {
        if (!m_findInSelectionEnabled) {
            const int selStart = static_cast<int>(editor->selectionStart());
            const int selEnd = static_cast<int>(editor->selectionEnd());
            if (selStart != selEnd) {
                m_findSelectionStart = qMin(selStart, selEnd);
                m_findSelectionEnd = qMax(selStart, selEnd);
                m_findInSelectionEnabled = true;
            }
        }
    } else {
        m_findInSelectionEnabled = false;
    }

    if (m_findInSelectionEnabled) {
        int rangeStart = qBound(0, m_findSelectionStart, docLen);
        int rangeEnd = qBound(0, m_findSelectionEnd, docLen);
        if (rangeStart >= rangeEnd) {
            m_findInSelectionEnabled = false;
            return {0, docLen};
        }
        return {rangeStart, rangeEnd};
    }

    return {0, docLen};
}

int FindReplaceController::findMatchesInEditor(SciEdit *editor, int flags, const QByteArray &needle, int maxLen)
{
    clearRegexError(editor);

    const int docLen = static_cast<int>(editor->textLength());
    int startPos = 0;
    int count = 0;

    while (startPos < docLen) {
        const QPair<int, int> found = editor->findText(flags, needle.constData(), startPos, docLen);
        if (found.first < 0) {
            break;
        }

        const int line = editor->lineFromPosition(found.first);
        const int lineStart = editor->positionFromLine(line);
        const int lineEnd = editor->lineEndPosition(line);
        const QByteArray lineBytes = editor->textRange(lineStart, lineEnd);
        const QString lineStr = QString::fromUtf8(lineBytes);

        const QByteArray beforeMatch = editor->textRange(lineStart, found.first);
        const QByteArray beforeEnd = editor->textRange(lineStart, found.second);
        const int relStart = QString::fromUtf8(beforeMatch).length();
        const int relEnd = QString::fromUtf8(beforeEnd).length();

        int displayStart, displayEnd;
        const QString displayLine = truncateLine(lineStr, relStart, relEnd, maxLen, displayStart, displayEnd);

        m_searchResultsDock->newResultsEntry(displayLine, line, relStart, relEnd,
                                              displayStart, displayEnd,
                                              found.first, found.second);
        ++count;

        startPos = found.second;
        if (startPos <= found.first) {
            startPos = found.first + 1;
        }
    }

    return count;
}

int FindReplaceController::performReplaceInEditor(SciEdit *editor, int flags,
                                                   const QByteArray &needle, const QByteArray &replacement,
                                                   int rangeStart, int rangeEnd)
{
    clearRegexError(editor);

    const bool useRegex = (m_dialog->searchMode() == SearchMode::Regex);

    editor->setSearchFlags(flags);

    int count = 0;
    int start = rangeStart;
    int end = rangeEnd;

    editor->beginUndoAction();
    while (start <= end) {
        editor->setTargetRange(start, end);
        const sptr_t pos = editor->searchInTarget(needle.size(), needle.constData());
        if (pos < 0) {
            break;
        }

        const int matchStart = static_cast<int>(editor->targetStart());
        const int matchEnd = static_cast<int>(editor->targetEnd());
        const int matchLen = matchEnd - matchStart;

        sptr_t replacedLen = 0;
        if (useRegex) {
            replacedLen = editor->replaceTargetRE(replacement.size(), replacement.constData());
        } else {
            replacedLen = editor->replaceTarget(replacement.size(), replacement.constData());
        }

        ++count;
        const int delta = static_cast<int>(replacedLen) - matchLen;
        end += delta;
        // Zero-length match protection: advance by one character to avoid infinite loop
        // when regex produces zero-length matches (e.g. $, ^, \b)
        if (matchLen == 0) {
            start = matchStart + 1;
        } else {
            start = matchStart + static_cast<int>(replacedLen);
        }
    }
    editor->endUndoAction();

    return count;
}

bool FindReplaceController::findNext(bool forward, bool showErrorMessage)
{
    SciEdit *editor = m_editorProvider();
    if (!editor) {
        return false;
    }

    ensureDialog();

    const QString needle = buildFindPattern(m_dialog);
    if (needle.isEmpty()) {
        return false;
    }

    const QByteArray needleBytes = needle.toUtf8();
    const sptr_t flags = buildSearchFlags(m_dialog);
    const bool isRegex = (m_dialog->searchMode() == SearchMode::Regex);

    const auto [rangeStart, rangeEnd] = resolveSearchRange(editor);

    const int selStartNow = static_cast<int>(editor->selectionStart());
    const int selEndNow = static_cast<int>(editor->selectionEnd());
    int caretBase = static_cast<int>(editor->currentPos());
    if (selStartNow != selEndNow) {
        if (forward) {
            caretBase = qMax(selStartNow, selEndNow);
        } else {
            caretBase = qMin(selStartNow, selEndNow);
        }
    }
    caretBase = qBound(rangeStart, caretBase, rangeEnd);

    // If the previous find resulted in a zero-length match at this position,
    // advance past it to avoid finding the same match again.
    if (forward && isRegex && caretBase == m_lastZeroLengthMatchPos) {
        caretBase = qMin(caretBase + 1, rangeEnd);
    }
    if (!forward && isRegex && caretBase == m_lastZeroLengthMatchPos) {
        caretBase = qMax(caretBase - 1, rangeStart);
    }

    auto selectMatch = [&](int start, int end) {
        editor->setSel(start, end);
        editor->scrollCaret();
    };

    auto showNotFound = [&]() {
        const bool hadRegexError = isRegex && checkAndClearRegexError(editor);
        if (!showErrorMessage) {
            return;
        }
        if (hadRegexError) {
            m_dialog->showStatusMessage(tr("Invalid regular expression."));
        } else {
            m_dialog->showStatusMessage(tr("Text not found."));
        }
    };

    auto handleFound = [&](int start, int end) {
        selectMatch(start, end);
        editor->highlightFindResults(needleBytes, static_cast<int>(flags));
        m_dialog->clearStatusMessage();
        if (start == end && isRegex) {
            const QByteArray tip = tr("^ zero length match").toUtf8();
            editor->callTipShow(start, tip.constData());
            m_lastZeroLengthMatchPos = start;
        } else {
            editor->callTipCancel();
            m_lastZeroLengthMatchPos = -1;
        }
    };

    if (forward) {
        const QPair<int, int> found = editor->findText(static_cast<int>(flags), needleBytes.constData(), caretBase, rangeEnd);
        if (found.first >= 0) {
            handleFound(found.first, found.second);
            return true;
        }
        if (m_dialog->wrapAround()) {
            const QPair<int, int> wrapped = editor->findText(static_cast<int>(flags), needleBytes.constData(), rangeStart, rangeEnd);
            if (wrapped.first >= 0) {
                handleFound(wrapped.first, wrapped.second);
                return true;
            }
        }
        showNotFound();
        return false;
    }

    QPair<int, int> found = editor->findLast(static_cast<int>(flags), needleBytes.constData(), rangeStart, caretBase);
    if (found.first >= 0) {
        handleFound(found.first, found.second);
        return true;
    }

    if (m_dialog->wrapAround()) {
        found = editor->findLast(static_cast<int>(flags), needleBytes.constData(), rangeStart, rangeEnd);
        if (found.first >= 0) {
            handleFound(found.first, found.second);
            return true;
        }
    }

    showNotFound();
    m_lastZeroLengthMatchPos = -1;
    return false;
}

bool FindReplaceController::quickFindNext(bool forward)
{
    SciEdit *editor = m_editorProvider();
    if (!editor) {
        return false;
    }

    const int selStart = static_cast<int>(editor->selectionStart());
    const int selEnd = static_cast<int>(editor->selectionEnd());

    QByteArray needleBytes;
    if (selStart != selEnd) {
        needleBytes = editor->get_text_range(selStart, selEnd);
    } else {
        const int pos = static_cast<int>(editor->currentPos());
        const int wordS = static_cast<int>(editor->wordStartPosition(pos, true));
        const int wordE = static_cast<int>(editor->wordEndPosition(pos, true));
        if (wordS < wordE) {
            needleBytes = editor->get_text_range(wordS, wordE);
        }
    }

    if (needleBytes.isEmpty()) {
        return false;
    }

    const int flags = 0;
    const int docLen = static_cast<int>(editor->textLength());
    const int caret = qBound(0, static_cast<int>(editor->currentPos()), docLen);

    auto selectAndHighlight = [&](const QPair<int, int> &match) {
        if (match.first < 0) {
            return false;
        }
        editor->setSel(match.first, match.second);
        editor->scrollCaret();
        editor->highlightFindResults(needleBytes, flags);
        return true;
    };

    auto doSearch = [&](int start, int end) -> QPair<int, int> {
        return forward ? editor->findText(flags, needleBytes.constData(), start, end)
                       : editor->findLast(flags, needleBytes.constData(), start, end);
    };

    if (selectAndHighlight(doSearch(forward ? caret : 0, forward ? docLen : caret))) {
        return true;
    }

    if (selectAndHighlight(doSearch(0, docLen))) {
        return true;
    }

    return false;
}

bool FindReplaceController::replaceOne(bool showErrorMessage)
{
    SciEdit *editor = m_editorProvider();
    if (!editor) {
        return false;
    }

    ensureDialog();

    const QString needle = buildFindPattern(m_dialog);
    if (needle.isEmpty()) {
        return false;
    }

    const int selStart = static_cast<int>(editor->selectionStart());
    const int selEnd = static_cast<int>(editor->selectionEnd());
    if (selStart == selEnd) {
        if (!findNext(true, showErrorMessage)) {
            return false;
        }
    }

    const QString replacement = buildReplacementText(m_dialog);
    const QByteArray replacementBytes = replacement.toUtf8();

    editor->targetFromSelection();
    if (m_dialog->searchMode() == SearchMode::Regex) {
        editor->replaceTargetRE(replacementBytes.size(), replacementBytes.constData());
    } else {
        editor->replaceTarget(replacementBytes.size(), replacementBytes.constData());
    }

    findNext(true, false);
    return true;
}

int FindReplaceController::replaceAll(bool showErrorMessage)
{
    SciEdit *editor = m_editorProvider();
    if (!editor) {
        return 0;
    }

    ensureDialog();

    const QString needle = buildFindPattern(m_dialog);
    if (needle.isEmpty()) {
        return 0;
    }

    const QByteArray needleBytes = needle.toUtf8();
    const QString replacement = buildReplacementText(m_dialog);
    const QByteArray replacementBytes = replacement.toUtf8();
    const sptr_t flags = buildSearchFlags(m_dialog);

    const auto [rangeStart, rangeEnd] = resolveSearchRange(editor);

    const int replacedCount = performReplaceInEditor(editor, static_cast<int>(flags), needleBytes, replacementBytes, rangeStart, rangeEnd);

    if (replacedCount == 0 && showErrorMessage) {
        if (m_dialog->searchMode() == SearchMode::Regex && checkAndClearRegexError(editor)) {
            m_dialog->showStatusMessage(tr("Invalid regular expression."));
        } else {
            m_dialog->showStatusMessage(tr("Text not found."));
        }
    } else if (replacedCount > 0) {
        m_dialog->clearStatusMessage();
    }

    return replacedCount;
}

void FindReplaceController::markAll()
{
    SciEdit *editor = m_editorProvider();
    if (!editor) {
        return;
    }

    ensureDialog();

    const QString needle = buildFindPattern(m_dialog);
    if (needle.isEmpty()) {
        m_dialog->showStatusMessage(tr("Find what is empty."));
        return;
    }

    const QByteArray needleBytes = needle.toUtf8();
    const sptr_t flags = buildSearchFlags(m_dialog);
    const int styleIndex = m_dialog->markStyleIndex();
    const bool bookmarkLine = m_dialog->bookmarkLine();

    if (m_dialog->purgeMarks()) {
        clearMarksInternal(editor, styleIndex, bookmarkLine);
    }

    const int docLen = static_cast<int>(editor->textLength());
    int start = 0;
    int end = docLen;
    QString scope;

    const int selStart = static_cast<int>(editor->selectionStart());
    const int selEnd = static_cast<int>(editor->selectionEnd());
    const int cpMin = qMin(selStart, selEnd);
    const int cpMax = qMax(selStart, selEnd);
    const bool backward = m_dialog->backwardDirection();

    if (m_dialog->inSelection()) {
        // Without a real selection the range is empty.
        start = qBound(0, cpMin, docLen);
        end = qBound(0, cpMax, docLen);
        scope = tr("in selection");
    } else if (m_dialog->wrapAround()) {
        start = 0;
        end = docLen;
        scope = tr("in entire file");
    } else if (backward) {
        start = 0;
        end = qBound(0, cpMax, docLen);
        scope = tr("from start of file to caret");
    } else {
        start = qBound(0, cpMin, docLen);
        end = docLen;
        scope = tr("from caret to end of file");
    }

    const int count = editor->markAllInRange(static_cast<int>(flags), needleBytes, start, end,
                                             styleIndex, bookmarkLine);

    if (count == 0) {
        if (m_dialog->searchMode() == SearchMode::Regex && checkAndClearRegexError(editor)) {
            m_dialog->showStatusMessage(tr("Invalid regular expression."));
            return;
        }
    }
    m_dialog->showStatusMessage(tr("Mark: %n match(es) %1.", nullptr, count).arg(scope));
}

void FindReplaceController::clearMarks()
{
    SciEdit *editor = m_editorProvider();
    if (!editor) {
        return;
    }

    ensureDialog();
    clearMarksInternal(editor, m_dialog->markStyleIndex(), m_dialog->bookmarkLine());
    m_dialog->clearStatusMessage();
}

void FindReplaceController::clearMarksInternal(SciEdit *editor, int styleIndex, bool bookmarkLine)
{
    const int selStart = static_cast<int>(editor->selectionStart());
    const int selEnd = static_cast<int>(editor->selectionEnd());

    if (m_dialog->inSelection() && selStart != selEnd) {
        const int start = qMin(selStart, selEnd);
        const int end = qMax(selStart, selEnd);
        editor->clearMarks(styleIndex, start, end);
        if (bookmarkLine) {
            editor->clearBookmarksInRange(start, end);
        }
        return;
    }

    editor->clearMarks(styleIndex);
    if (bookmarkLine) {
        editor->clearAllBookmarks();
    }
}

void FindReplaceController::copyMarkedText()
{
    ensureDialog();
    copyMarkedTextForMask(1 << m_dialog->markStyleIndex());
}

void FindReplaceController::copyMarkedTextForMask(int styleMask)
{
    SciEdit *editor = m_editorProvider();
    if (!editor) {
        return;
    }

    const QList<QPair<int, QString>> marked = editor->markedTexts(styleMask);
    if (marked.isEmpty()) {
        m_dialog->showStatusMessage(tr("No marked text."));
        return;
    }

    QGuiApplication::clipboard()->setText(SciEdit::joinMarkedTexts(marked));
    m_dialog->showStatusMessage(tr("%n fragment(s) copied.", nullptr, marked.size()));
}

void FindReplaceController::setSearchResultsDock(SearchResultsDock *dock)
{
    m_searchResultsDock = dock;
}

void FindReplaceController::setDocumentManager(DocumentManager *docManager)
{
    m_documentManager = docManager;
}

void FindReplaceController::setEditorViewManager(EditorViewManager *viewManager)
{
    m_viewManager = viewManager;
}

void FindReplaceController::setEnsureDockCallback(std::function<void()> cb)
{
    m_ensureDockCallback = std::move(cb);
}

void FindReplaceController::ensureSearchResultsDock()
{
    if (m_ensureDockCallback) {
        m_ensureDockCallback();
    }
}

FindInFilesSearcher::ContentProvider FindReplaceController::openDocumentContentProvider() const
{
    return [this](const QString &fileRef, QString &content) {
        if (!m_viewManager || !m_documentManager) {
            return false;
        }

        // Match open documents by full file path first.
        int docId = m_documentManager->findDocumentByFilePath(fileRef);
        if (docId < 0) {
            // Fall back to the display title, which covers untitled
            // documents ("Untitled-N") listed by "Find All" searches.
            docId = m_documentManager->findUntitledDocumentByTitle(fileRef);
        }
        if (docId < 0) {
            return false;
        }

        SciEdit *editor = m_viewManager->editorForDocId(docId);
        if (!editor) {
            return false;
        }
        content = QString::fromUtf8(editor->textRange(0, int(editor->textLength())));
        return true;
    };
}

void FindReplaceController::findAllInCurrentDocument()
{
    SciEdit *editor = m_editorProvider();
    if (!editor || !m_documentManager) {
        return;
    }

    ensureSearchResultsDock();
    Q_CHECK_PTR(m_searchResultsDock);

    ensureDialog();

    const QString needle = buildFindPattern(m_dialog);
    if (needle.isEmpty()) {
        return;
    }

    const QByteArray needleBytes = needle.toUtf8();
    const sptr_t flags = buildSearchFlags(m_dialog);

    const int docId = m_viewManager ? m_viewManager->currentDocumentId() : -1;
    // The full file path goes into the results panel (shown in the file
    // header, used by "Copy Selected Pathname(s)" and by Find-in-Finder to
    // re-open the file); untitled documents fall back to their display title.
    QString filePath = m_documentManager->getDocumentFilePath(docId);
    if (filePath.isEmpty()) {
        filePath = m_documentManager->getDocumentTitle(docId);
    }

    m_searchResultsDock->newSearch(needle);
    m_searchResultsDock->newFileEntry(docId, filePath, int(editor->lineCount()));

    const int maxLen = AppSettings::searchResultMaxLineLength();
    const int matchCount = findMatchesInEditor(editor, static_cast<int>(flags), needleBytes, maxLen);

    m_searchResultsDock->completeSearch();

    if (matchCount == 0) {
        if (m_dialog->searchMode() == SearchMode::Regex && checkAndClearRegexError(editor)) {
            m_dialog->showStatusMessage(tr("Invalid regular expression."));
        } else {
            m_dialog->showStatusMessage(tr("Text not found."));
        }
    } else {
        m_dialog->showStatusMessage(tr("%1 occurrence(s) found.").arg(matchCount));
    }
}

void FindReplaceController::findAllInAllDocuments()
{
    if (!m_documentManager || !m_viewManager) {
        return;
    }

    ensureSearchResultsDock();
    Q_CHECK_PTR(m_searchResultsDock);

    ensureDialog();

    const QString needle = buildFindPattern(m_dialog);
    if (needle.isEmpty()) {
        return;
    }

    const QByteArray needleBytes = needle.toUtf8();
    const sptr_t flags = buildSearchFlags(m_dialog);

    m_searchResultsDock->newSearch(needle);

    const int maxLen = AppSettings::searchResultMaxLineLength();
    int totalMatchCount = 0;
    bool regexError = false;
    const QVector<int> docIds = m_viewManager->documentIds();

    for (int docId : docIds) {
        SciEdit *editor = m_viewManager->editorForDocId(docId);
        if (!editor) {
            continue;
        }

        // Full file path so the results panel can re-open the file (see
        // findAllInCurrentDocument); untitled documents use their title.
        QString filePath = m_documentManager->getDocumentFilePath(docId);
        if (filePath.isEmpty()) {
            filePath = m_documentManager->getDocumentTitle(docId);
        }
        m_searchResultsDock->newFileEntry(docId, filePath, int(editor->lineCount()));

        totalMatchCount += findMatchesInEditor(editor, static_cast<int>(flags), needleBytes, maxLen);
        if (!regexError && m_dialog->searchMode() == SearchMode::Regex)
            regexError = checkAndClearRegexError(editor);
    }

    m_searchResultsDock->completeSearch();

    if (totalMatchCount == 0) {
        if (regexError) {
            m_dialog->showStatusMessage(tr("Invalid regular expression."));
        } else {
            m_dialog->showStatusMessage(tr("Text not found."));
        }
    } else {
        m_dialog->showStatusMessage(tr("%1 occurrence(s) found in %2 file(s).").arg(totalMatchCount).arg(docIds.size()));
    }
}

void FindReplaceController::replaceAllInAllDocuments()
{
    if (!m_dialog || !m_viewManager) {
        return;
    }

    const QString needle = buildFindPattern(m_dialog);
    if (needle.isEmpty()) {
        return;
    }

    const QByteArray needleBytes = needle.toUtf8();
    const QString replacement = buildReplacementText(m_dialog);
    const QByteArray replacementBytes = replacement.toUtf8();
    const sptr_t flags = buildSearchFlags(m_dialog);

    int totalReplacedCount = 0;
    bool regexError = false;

    const QVector<int> docIds = m_viewManager->documentIds();
    for (int docId : docIds) {
        SciEdit *editor = m_viewManager->editorForDocId(docId);
        if (!editor) {
            continue;
        }

        const int docLen = static_cast<int>(editor->textLength());
        totalReplacedCount += performReplaceInEditor(editor, static_cast<int>(flags), needleBytes, replacementBytes, 0, docLen);
        if (!regexError && m_dialog->searchMode() == SearchMode::Regex)
            regexError = checkAndClearRegexError(editor);
    }

    if (totalReplacedCount == 0) {
        if (regexError) {
            m_dialog->showStatusMessage(tr("Invalid regular expression."));
        } else {
            m_dialog->showStatusMessage(tr("Text not found."));
        }
    } else {
        m_dialog->showStatusMessage(tr("%1 occurrence(s) replaced in %2 file(s).").arg(totalReplacedCount).arg(docIds.size()));
    }
}

void FindReplaceController::findInFiles()
{
    ensureDialog();

    const QString needle = m_dialog->findTextForSearch();
    if (needle.isEmpty()) {
        m_dialog->showStatusMessage(tr("Find what is empty."));
        return;
    }

    const QString directory = m_dialog->directory();
    if (directory.isEmpty() || !QFileInfo(directory).isDir()) {
        m_dialog->showStatusMessage(tr("Invalid directory."));
        return;
    }

    ensureSearchResultsDock();
    Q_CHECK_PTR(m_searchResultsDock);

    m_searchResultsDock->newSearch(needle);

    FindInFilesSearcher::Params params;
    params.directory = directory;
    params.filters = m_dialog->filters();
    params.recursive = m_dialog->isRecursive();
    params.includeHidden = m_dialog->isInHiddenFolders();
    params.needle = needle;
    params.matchCase = m_dialog->matchCase();
    params.wholeWord = m_dialog->wholeWord();
    params.mode = m_dialog->searchMode();

    const int maxLen = AppSettings::searchResultMaxLineLength();

    QProgressDialog progress(qobject_cast<QWidget *>(parent()));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setWindowTitle(tr("Find in Files"));
    progress.setLabelText(tr("Searching..."));
    progress.setRange(0, 0);
    progress.setValue(0);
    progress.show();

    auto shouldCancel = [&progress]() -> bool {
        QApplication::processEvents();
        return progress.wasCanceled();
    };

    auto progressReporter = [&progress](const QString &currentFile, int searched, int matched, int total) {
        progress.setMaximum(total);
        progress.setValue(searched);
        progress.setLabelText(FindReplaceController::tr("Searching %1...  (%L2 files searched, %L3 with matches)")
            .arg(QFileInfo(currentFile).fileName())
            .arg(searched)
            .arg(matched));
    };

    auto fileResult = [this, maxLen](const QString &filePath, int totalLines, const QVector<FindInFilesSearcher::Match> &matches) {
        m_searchResultsDock->newFileEntryForPath(filePath, totalLines);
        for (const FindInFilesSearcher::Match &m : matches) {
            int displayStart = 0;
            int displayEnd = 0;
            const QString displayLine = truncateLine(m.lineText, m.matchStartCol, m.matchEndCol, maxLen, displayStart, displayEnd);
            m_searchResultsDock->newResultsEntryForFile(displayLine, m.line,
                                                         m.matchStartCol, m.matchEndCol,
                                                         displayStart, displayEnd);
        }
    };

    const FindInFilesSearcher::Stats stats =
        FindInFilesSearcher::search(params, shouldCancel, progressReporter, fileResult,
                                    openDocumentContentProvider());

    progress.reset();

    m_searchResultsDock->setSearchInfo(stats.filesSearched, stats.filesWithHits);
    m_searchResultsDock->completeSearch();

    if (stats.cancelled) {
        m_dialog->showStatusMessage(tr("Search cancelled. %1 occurrence(s) found in %2 file(s).")
            .arg(stats.totalHits).arg(stats.filesWithHits));
    } else if (stats.totalHits == 0) {
        if (params.mode == SearchMode::Regex) {
            m_dialog->showStatusMessage(tr("No matches or invalid regular expression."));
        } else {
            m_dialog->showStatusMessage(tr("No matches found."));
        }
    } else {
        m_dialog->showStatusMessage(tr("%1 occurrence(s) found in %2 file(s).")
            .arg(stats.totalHits).arg(stats.filesWithHits));
    }
}

void FindReplaceController::findInFilesList(const FindInFilesSearcher::Params &params,
                                            const QStringList &filePaths,
                                            const QHash<QString, QSet<int>> *lineFilter,
                                            SearchResultsDock *destDock)
{
    Q_CHECK_PTR(destDock);

    const int maxLen = AppSettings::searchResultMaxLineLength();

    QProgressDialog progress(qobject_cast<QWidget *>(parent()));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setWindowTitle(tr("Find in Search Results"));
    progress.setLabelText(tr("Searching..."));
    progress.setRange(0, 0);
    progress.setValue(0);
    progress.show();

    auto shouldCancel = [&progress]() -> bool {
        QApplication::processEvents();
        return progress.wasCanceled();
    };

    auto progressReporter = [&progress](const QString &currentFile, int searched, int matched, int total) {
        progress.setMaximum(total);
        progress.setValue(searched);
        progress.setLabelText(FindReplaceController::tr("Searching %1...  (%L2 files searched, %L3 with matches)")
            .arg(QFileInfo(currentFile).fileName())
            .arg(searched)
            .arg(matched));
    };

    auto fileResult = [destDock, maxLen, lineFilter](const QString &filePath, int totalLines,
                                                     const QVector<FindInFilesSearcher::Match> &matches) {
        QVector<FindInFilesSearcher::Match> kept = matches;
        if (lineFilter) {
            // "Search only in found lines": keep only matches on lines
            // that already appear in the source results.
            const QSet<int> lines = lineFilter->value(filePath);
            kept.erase(std::remove_if(kept.begin(), kept.end(),
                                      [&lines](const FindInFilesSearcher::Match &m) {
                                          return !lines.contains(m.line);
                                      }),
                       kept.end());
            if (kept.isEmpty()) {
                return;
            }
        }

        destDock->newFileEntryForPath(filePath, totalLines);
        for (const FindInFilesSearcher::Match &m : kept) {
            int displayStart = 0;
            int displayEnd = 0;
            const QString displayLine = truncateLine(m.lineText, m.matchStartCol, m.matchEndCol, maxLen, displayStart, displayEnd);
            destDock->newResultsEntryForFile(displayLine, m.line,
                                             m.matchStartCol, m.matchEndCol,
                                             displayStart, displayEnd);
        }
    };

    destDock->newSearch(params.needle);
    if (lineFilter) {
        // Marks this as line filter mode in the search header.
        destDock->setSearchScopeNote(tr(" - Line Filter Mode: only display the filtered results"));
    }

    const FindInFilesSearcher::Stats stats =
        FindInFilesSearcher::searchFiles(filePaths, params, shouldCancel, progressReporter, fileResult,
                                         openDocumentContentProvider());

    progress.reset();

    destDock->setSearchInfo(stats.filesSearched, stats.filesWithHits);
    destDock->completeSearch();
}

void FindReplaceController::countMatches()
{
    SciEdit *editor = m_editorProvider();
    if (!editor) {
        return;
    }

    ensureDialog();

    const QString needle = buildFindPattern(m_dialog);
    if (needle.isEmpty()) {
        return;
    }

    const QByteArray needleBytes = needle.toUtf8();
    const sptr_t flags = buildSearchFlags(m_dialog);

    const auto [rangeStart, rangeEnd] = resolveSearchRange(editor);

    clearRegexError(editor);

    int count = 0;
    int startPos = rangeStart;

    while (startPos < rangeEnd) {
        const QPair<int, int> found = editor->findText(static_cast<int>(flags), needleBytes.constData(), startPos, rangeEnd);
        if (found.first < 0) {
            break;
        }
        ++count;
        startPos = found.second;
        if (startPos <= found.first) {
            startPos = found.first + 1;
        }
    }

    if (count == 0) {
        if (m_dialog->searchMode() == SearchMode::Regex && checkAndClearRegexError(editor)) {
            m_dialog->showStatusMessage(tr("Invalid regular expression."));
        } else {
            m_dialog->showStatusMessage(tr("Text not found."));
        }
    } else {
        m_dialog->showStatusMessage(tr("%1 occurrence(s) found.").arg(count));
    }
}
