#include "searchresultsdock.h"
#include "ui_searchresultsdock.h"

#include "appsettings.h"
#include "documentmanager.h"
#include "editorviewmanager.h"
#include "searchresultlexer.h"
#include "searchresultview.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QShortcut>

#include <Scintilla.h>
#include <ScintillaEdit.h>

using namespace Qt::StringLiterals;

namespace {

// Converts a UTF-16 code unit index into a UTF-8 byte offset. Match positions
// coming from FindReplaceController are QString indices while Scintilla style
// bytes are addressed in UTF-8.
int utf8ByteOffset(const QString &text, int charIndex)
{
    charIndex = qBound(0, charIndex, text.size());
    return text.left(charIndex).toUtf8().size();
}

// Number of decimal digits needed for a 1-based line number of a file with
// totalLines lines.
int digitsForLineCount(int totalLines)
{
    int digits = 1;
    while (totalLines >= 10) {
        totalLines /= 10;
        ++digits;
    }
    return digits;
}

} // namespace

SearchResultsDock::SearchResultsDock(QWidget *parent)
    : QDockWidget(parent)
    , ui(new Ui::SearchResultsDock)
{
    ui->setupUi(this);

    new QShortcut(QKeySequence::Cancel, this, this, &SearchResultsDock::close, Qt::WidgetWithChildrenShortcut);

    // The lexer instance is handed over to the view's Scintilla document by
    // setLexer(); the dock keeps calling setMarkings()/marking access on it
    // and never replaces it, so the raw pointer stays valid.
    m_lexer = pineapple::createSearchResultLexer();
    m_lexer->setMarkings(&m_markings);
    ui->resultView->setLexer(m_lexer);

    connect(ui->resultView, &ScintillaEditBase::doubleClick,
            this, &SearchResultsDock::handleDoubleClick);
    connect(ui->resultView, &SearchResultView::contextMenuRequested,
            this, &SearchResultsDock::showContextMenu);
}

SearchResultsDock::~SearchResultsDock()
{
    delete ui;
}

void SearchResultsDock::setDocumentManager(DocumentManager *docManager)
{
    m_documentManager = docManager;
}

void SearchResultsDock::setEditorViewManager(EditorViewManager *viewManager)
{
    m_viewManager = viewManager;
}

void SearchResultsDock::setVolatile(bool volatileDock)
{
    m_volatile = volatileDock;
}

bool SearchResultsDock::isVolatile() const
{
    return m_volatile;
}

QStringList SearchResultsDock::allResultFilePaths() const
{
    return filePathsInRange(0, m_foundInfos.count() - 1);
}

QHash<QString, QSet<int>> SearchResultsDock::resultLinesByFile() const
{
    // Result lines only (header placeholders carry lineNumber < 0); both
    // docId-based and file-based entries know their source file and line.
    QHash<QString, QSet<int>> linesByFile;
    for (const FoundInfo &info : m_foundInfos) {
        if (info.lineNumber >= 0 && !info.filePath.isEmpty()) {
            linesByFile[info.filePath].insert(info.lineNumber);
        }
    }
    return linesByFile;
}

void SearchResultsDock::applyEditorSettings()
{
    ui->resultView->setEditorFont(AppSettings::editorFont());
    ui->resultView->setThemeId(AppSettings::editorTheme());
    ui->resultView->setTabWidth(AppSettings::tabWidth());
    ui->resultView->setWrapIndentMode(SC_WRAPINDENT_SAME);
    ui->resultView->setWrapMode(AppSettings::searchResultWordWrap() ? SC_WRAP_WORD : SC_WRAP_NONE);
}

void SearchResultsDock::newSearch(const QString &searchTerm)
{
    show();
    raise();

    // A multiline search term must stay on one buffer line; line indices of
    // m_foundInfos/m_markings depend on it.
    QString oneLine = searchTerm;
    oneLine.remove(u'\r');
    oneLine.remove(u'\n');
    m_searchTerm = oneLine;

    contractPreviousSearches();

    m_currentSearchLine = appendResultLine(tr("Search \"%1\"").arg(oneLine), FoundInfo());

    m_currentFileLine = -1;
    m_currentFileCount = 0;
    m_totalFileHitCount = 0;
    m_totalHitCount = 0;
    m_scopeNote.clear();
    m_lastSourceLine = -1;
    m_lastDisplayLine.clear();

    ui->resultView->setFirstVisibleLine(m_currentSearchLine);
}

void SearchResultsDock::newFileEntry(int docId, const QString &filePath, int totalLines)
{
    finishPendingFileHeader();

    m_currentFilePath = filePath;
    m_currentDocId = docId;
    m_totalFileHitCount = 0;
    m_lineNumberDigits = digitsForLineCount(qMax(1, totalLines));
    m_lastSourceLine = -1;
    m_lastDisplayLine.clear();

    m_currentFileLine = appendResultLine(u"  %1"_s.arg(filePath), FoundInfo());

    m_currentFileCount++;
}

void SearchResultsDock::newFileEntryForPath(const QString &filePath, int totalLines)
{
    finishPendingFileHeader();

    m_currentFilePath = filePath;
    m_currentDocId = -1;
    m_totalFileHitCount = 0;
    m_lineNumberDigits = digitsForLineCount(qMax(1, totalLines));
    m_lastSourceLine = -1;
    m_lastDisplayLine.clear();

    m_currentFileLine = appendResultLine(u"  %1"_s.arg(filePath), FoundInfo());

    m_currentFileCount++;
}

void SearchResultsDock::newResultsEntry(const QString &displayLine, int lineNumber,
                                        int relStart, int relEnd,
                                        int displayStart, int displayEnd,
                                        int docStartPos, int docEndPos, int hitCount)
{
    const QPair<int, int> range(docStartPos, docEndPos);

    if (!tryMergeIntoLastEntry(lineNumber, relStart, relEnd, range)) {
        FoundInfo info;
        info.docId = m_currentDocId;
        // Kept for "Copy Selected Pathname(s)"; navigation is docId-based.
        info.filePath = m_currentFilePath;
        info.lineNumber = lineNumber;
        info.ranges.append(range);

        const QString prefix = resultLinePrefix(lineNumber);
        appendResultLine(prefix + displayLine, info);
        const int prefixBytes = prefix.toUtf8().size();
        markMatchOnLastLine(prefixBytes, displayLine, displayStart, displayEnd);

        m_lastSourceLine = lineNumber;
        // truncateLine() reports the match relative to the displayed part, so
        // the window start inside the source line is relStart - displayStart.
        m_lastWindowStart = relStart - displayStart;
        m_lastPrefixBytes = prefixBytes;
        m_lastDisplayLine = displayLine;
    }

    // Every occurrence counts towards the hit totals, merged or not.
    m_totalFileHitCount += hitCount;
    m_totalHitCount += hitCount;
}

void SearchResultsDock::newResultsEntryForFile(const QString &displayLine, int lineNumber,
                                               int relStart, int relEnd,
                                               int displayStart, int displayEnd)
{
    const QPair<int, int> range(relStart, relEnd);

    if (!tryMergeIntoLastEntry(lineNumber, relStart, relEnd, range)) {
        FoundInfo info;
        info.filePath = m_currentFilePath;
        info.lineNumber = lineNumber;
        info.isFileBased = true;
        info.ranges.append(range);

        const QString prefix = resultLinePrefix(lineNumber);
        appendResultLine(prefix + displayLine, info);
        const int prefixBytes = prefix.toUtf8().size();
        markMatchOnLastLine(prefixBytes, displayLine, displayStart, displayEnd);

        m_lastSourceLine = lineNumber;
        m_lastWindowStart = relStart - displayStart;
        m_lastPrefixBytes = prefixBytes;
        m_lastDisplayLine = displayLine;
    }

    m_totalFileHitCount += 1;
    m_totalHitCount += 1;
}

void SearchResultsDock::setSearchInfo(int searchedFiles, int filesWithHits)
{
    m_searchedFiles = searchedFiles;
    m_filesWithHits = filesWithHits;
}

void SearchResultsDock::setSearchScopeNote(const QString &note)
{
    m_scopeNote = note;
}

void SearchResultsDock::completeSearch()
{
    finishPendingFileHeader();

    const int searchLine = m_currentSearchLine;
    if (searchLine >= 0) {
        QString suffix = tr(" (%L1 hits in %L2 files)").arg(m_totalHitCount).arg(m_currentFileCount);
        if (m_searchedFiles > 0) {
            suffix += tr("  [searched %L1 files]").arg(m_searchedFiles);
        }
        if (!m_scopeNote.isEmpty()) {
            suffix += m_scopeNote;
        }
        insertTextAtLineEnd(searchLine, suffix);
    }

    m_currentSearchLine = -1;
    m_currentFileLine = -1;
    m_scopeNote.clear();
    m_currentFileCount = 0;
    m_totalFileHitCount = 0;
    m_totalHitCount = 0;
    m_searchedFiles = 0;
    m_filesWithHits = 0;

    // Styling/fold levels are computed lazily while results stream in; run a
    // final pass over everything and keep the finished search in view.
    ui->resultView->colourise(0, -1);
    if (searchLine >= 0) {
        ui->resultView->setFirstVisibleLine(searchLine);
    }
}

void SearchResultsDock::collapseAll()
{
    ui->resultView->colourise(0, -1);
    ui->resultView->foldAll(SC_FOLDACTION_CONTRACT);
}

void SearchResultsDock::expandAll()
{
    SearchResultView *view = ui->resultView;
    view->colourise(0, -1);
    const sptr_t totalLines = view->lineCount();
    for (sptr_t line = 0; line < totalLines; ++line) {
        if (view->foldLevel(line) & SC_FOLDLEVELHEADERFLAG) {
            view->foldLine(line, SC_FOLDACTION_EXPAND);
        }
    }
}

void SearchResultsDock::selectAll()
{
    ui->resultView->selectAll();
}

void SearchResultsDock::copySelection()
{
    ui->resultView->copy();
}

void SearchResultsDock::copyLines()
{
    // Copy the code text of the selected result lines without the
    // "\t<Line> N: " prefixes. With no (or a single-line)
    // selection sitting on a header line, the whole entry's result lines are
    // copied; consecutively repeated result lines are copied once.
    SearchResultView *view = ui->resultView;

    const QPair<int, int> range = selectionLineRange();
    int fromLine = range.first;
    int toLine = range.second;

    if (view->selectionEmpty() || fromLine == toLine) {
        const int level = int(view->foldLevel(fromLine) & SC_FOLDLEVELNUMBERMASK);
        if (level != SC_FOLDLEVELBASE + 2) {
            // Caret on a search or file header line: extend to its subtree.
            toLine = qMax<int>(fromLine, int(view->lastChild(fromLine, -1)));
            const int lastLevel = int(view->foldLevel(toLine) & SC_FOLDLEVELNUMBERMASK);
            if (lastLevel != SC_FOLDLEVELBASE + 2) {
                return; // the entry has no result lines (0 hits)
            }
        }
    }

    QStringList lines;
    QString previousLine;
    for (int line = fromLine; line <= toLine && line < m_foundInfos.count(); ++line) {
        const QString text = QString::fromUtf8(
            view->textRange(view->positionFromLine(line), view->lineEndPosition(line)));
        if (!text.startsWith(u'\t')) {
            continue; // only result lines carry text to copy
        }
        if (text == previousLine) {
            continue;
        }
        previousLine = text;
        const qsizetype colon = text.indexOf(u':');
        if (colon < 0) {
            continue;
        }
        lines.append(text.mid(colon + 2)); // skip ": " after the line number
    }
    if (lines.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(lines.join(u"\r\n"_s) + u"\r\n"_s);
}

void SearchResultsDock::copySelectedPaths()
{
    // The paths of the files listed in the selected lines, deduplicated, one per line.
    const QPair<int, int> range = selectionLineRange();
    const QStringList paths = filePathsInRange(range.first, range.second);
    if (paths.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(paths.join(u"\r\n"_s) + u"\r\n"_s);
}

void SearchResultsDock::setWordWrap(bool enabled)
{
    // "Word wrap long lines": applied immediately and persisted.
    ui->resultView->setWrapIndentMode(SC_WRAPINDENT_SAME);
    ui->resultView->setWrapMode(enabled ? SC_WRAP_WORD : SC_WRAP_NONE);
    AppSettings::setSearchResultWordWrap(enabled);
    AppSettings::self()->save();
}

void SearchResultsDock::deleteEntry(int line)
{
    SearchResultView *view = ui->resultView;
    if (line < 0 || line >= m_foundInfos.count()) {
        return;
    }

    // Header lines remove their whole subtree, result lines just themselves.
    int last = line;
    if (view->foldLevel(line) & SC_FOLDLEVELHEADERFLAG) {
        last = qMax<int>(line, int(view->lastChild(line, -1)));
    }

    const int removedCount = last - line + 1;
    const sptr_t startPos = view->positionFromLine(line);
    const sptr_t endPos = (last + 1 < int(view->lineCount()))
        ? view->positionFromLine(last + 1)
        : view->length();
    if (endPos <= startPos) {
        return;
    }

    view->setReadOnly(false);
    view->deleteRange(startPos, endPos - startPos);
    view->setReadOnly(true);

    m_foundInfos.remove(line, removedCount);
    m_markings.erase(m_markings.begin() + line, m_markings.begin() + line + removedCount);

    // Keep pending header bookkeeping valid; entries can be removed while a
    // Find in Files search is still streaming results.
    adjustPendingLine(&m_currentSearchLine, line, removedCount);
    adjustPendingLine(&m_currentFileLine, line, removedCount);

    view->colourise(0, -1);
}

void SearchResultsDock::deleteAll()
{
    SearchResultView *view = ui->resultView;

    view->setReadOnly(false);
    view->clearAll();
    view->setReadOnly(true);

    m_foundInfos.clear();
    m_markings.clear();
    m_currentSearchLine = -1;
    m_currentFileLine = -1;
    m_currentFileCount = 0;
    m_totalFileHitCount = 0;
    m_totalHitCount = 0;
    m_searchedFiles = 0;
    m_filesWithHits = 0;
}

void SearchResultsDock::handleDoubleClick(Scintilla::Position position, Scintilla::Position line)
{
    SearchResultView *view = ui->resultView;
    const int index = int(line);
    if (index < 0 || index >= m_foundInfos.count()) {
        return;
    }

    if (view->foldLevel(index) & SC_FOLDLEVELHEADERFLAG) {
        view->toggleFold(index);
        return;
    }

    const FoundInfo &info = m_foundInfos[index];
    if (info.lineNumber < 0 || info.ranges.isEmpty()) {
        return;
    }

    const sptr_t lineStart = view->positionFromLine(index);

    // Double-clicking a highlighted atch jumps to that occurrence,
    // double-clicking anywhere else on the line jumps to the first one.
    // The segment bounds are inclusive. We check
    // `range.first <= pos && pos <= range.second`, so a click exactly at
    // either boundary of the match word (e.g. right after its last character,
    // where the double-click position lands on the gap following the word)
    // still selects that match.
    int rangeIndex = 0;
    if (position >= 0) {
        const int posInLine = int(position - lineStart); // UTF-8 byte offset
        const std::vector<pineapple::SearchResultMarkingSegment> &segments =
            m_markings[static_cast<size_t>(index)].segments;
        for (size_t i = 0; i < segments.size(); ++i) {
            if (posInLine >= segments[i].start && posInLine <= segments[i].end) {
                rangeIndex = int(i);
                break;
            }
        }
    }
    rangeIndex = qBound(0, rangeIndex,
                        qMin(info.ranges.count(), int(m_markings[static_cast<size_t>(index)].segments.size())) - 1);

    // Give feedback in the results view by selecting the chosen occurrence.
    const pineapple::SearchResultMarkingSegment &segment =
        m_markings[static_cast<size_t>(index)].segments[static_cast<size_t>(rangeIndex)];
    view->setSel(lineStart + segment.start, lineStart + segment.end);

    const QPair<int, int> &range = info.ranges[rangeIndex];
    if (info.isFileBased) {
        emit searchResultActivatedForFile(info.filePath, info.lineNumber, range.first, range.second);
    } else {
        emit searchResultActivated(info.docId, info.lineNumber, range.first, range.second);
    }
}

void SearchResultsDock::showContextMenu(const QPoint &globalPos, int line)
{
    QMenu menu(this);
    // Find in search results: re-run a search on the files listed here.
    menu.addAction(QIcon::fromTheme(u"edit-find"_s), tr("Find in These Search Results..."), this, [this]() {
        emit findInResultsRequested();
    });
    if (m_volatile) {
        // Volatile docks (Find in search results) are closed instead of cleared.
        menu.addAction(tr("Close These Search Results"), this, [this]() {
            emit closeRequested();
        });
    }
    menu.addSeparator();
    menu.addAction(tr("Collapse All"), this, &SearchResultsDock::collapseAll);
    menu.addAction(tr("Expand All"), this, &SearchResultsDock::expandAll);
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(u"edit-select-all"_s), tr("Select All"), this, &SearchResultsDock::selectAll);
    menu.addAction(QIcon::fromTheme(u"edit-copy"_s), tr("Copy"), this, &SearchResultsDock::copySelection)
        ->setEnabled(!ui->resultView->selectionEmpty());
    menu.addAction(tr("Copy Selected Line(s)"), this, &SearchResultsDock::copyLines);
    menu.addAction(tr("Copy Selected Pathname(s)"), this, &SearchResultsDock::copySelectedPaths);
    if (line >= 0 && line < m_foundInfos.count()) {
        menu.addSeparator();
        menu.addAction(tr("Remove Entry"), this, [this, line]() {
            deleteEntry(line);
        });
    }
    if (!m_volatile) {
        menu.addSeparator();
        menu.addAction(tr("Clear All"), this, &SearchResultsDock::deleteAll);
    }
    menu.addSeparator();
    QAction *wrapAction = menu.addAction(tr("Word wrap long lines"),
                                         this, &SearchResultsDock::setWordWrap);
    wrapAction->setIcon(QIcon::fromTheme(u"text-wrap"_s));
    wrapAction->setCheckable(true);
    wrapAction->setChecked(AppSettings::searchResultWordWrap());

    menu.exec(globalPos);
}

void SearchResultsDock::finishPendingFileHeader()
{
    if (m_currentFileLine < 0) {
        return;
    }
    insertTextAtLineEnd(m_currentFileLine, tr(" (%L1 hits)").arg(m_totalFileHitCount));
    m_currentFileLine = -1;
}

void SearchResultsDock::insertTextAtLineEnd(int line, const QString &text)
{
    SearchResultView *view = ui->resultView;
    const QByteArray bytes = text.toUtf8();

    view->setReadOnly(false);
    view->insertText(view->lineEndPosition(line), bytes.constData());
    view->setReadOnly(true);
}

int SearchResultsDock::appendResultLine(const QString &lineText, const FoundInfo &info)
{
    // The lexer classifies lines by their first character: no prefix for the
    // search header, a space prefix for file headers, a tab for result lines.
    QByteArray bytes = lineText.toUtf8();
    bytes += "\r\n";

    SearchResultView *view = ui->resultView;
    view->setReadOnly(false);
    view->appendText(bytes.size(), bytes.constData());
    view->setReadOnly(true);

    // One entry per buffer line, including headers (as placeholder).
    m_foundInfos.push_back(info);
    m_markings.emplace_back();
    return m_foundInfos.count() - 1;
}

QString SearchResultsDock::resultLinePrefix(int lineNumber) const
{
    // The line number is right-justified to the width of the largest line number of the file so the colons of
    // consecutive result lines align ("行   8:" / "行 123:").
    const QString number = QString::number(lineNumber + 1).rightJustified(m_lineNumberDigits, u' ');
    return u"\t%1 %2: "_s.arg(tr("Line", "search result line-number label"), number);
}

void SearchResultsDock::markMatchOnLastLine(int prefixBytes, const QString &displayLine,
                                            int displayStart, int displayEnd)
{
    pineapple::SearchResultMarkingSegment segment;
    segment.start = prefixBytes + utf8ByteOffset(displayLine, displayStart);
    segment.end = prefixBytes + utf8ByteOffset(displayLine, displayEnd);
    m_markings.back().segments.push_back(segment);
}

bool SearchResultsDock::tryMergeIntoLastEntry(int lineNumber, int relStart, int relEnd,
                                              const QPair<int, int> &range)
{
    // Matches "show only one entry per found line if possible" preference (default on):
    // when disabled, every occurrence gets its own result entry.
    if (!AppSettings::searchResultOneEntryPerLine()) {
        return false;
    }

    if (lineNumber != m_lastSourceLine || m_lastDisplayLine.isEmpty() || m_foundInfos.isEmpty()) {
        return false;
    }

    // Only merge when the extra match is actually visible in the displayed
    // (possibly truncated) part of the last entry; otherwise it could not be
    // highlighted there and gets its own entry.
    const int windowStart = m_lastWindowStart;
    const int windowEnd = windowStart + m_lastDisplayLine.size();
    if (relStart < windowStart || relEnd > windowEnd) {
        return false;
    }

    pineapple::SearchResultMarkingSegment segment;
    segment.start = m_lastPrefixBytes + utf8ByteOffset(m_lastDisplayLine, relStart - windowStart);
    segment.end = m_lastPrefixBytes + utf8ByteOffset(m_lastDisplayLine, relEnd - windowStart);
    m_markings.back().segments.push_back(segment);
    m_foundInfos.back().ranges.append(range);
    return true;
}

void SearchResultsDock::contractPreviousSearches()
{
    SearchResultView *view = ui->resultView;

    // Make sure fold levels are computed for everything appended so far
    // (styling is lazy), then fold the top-level search headers.
    view->colourise(0, -1);
    const sptr_t totalLines = view->lineCount();
    for (sptr_t line = 0; line < totalLines; ++line) {
        const sptr_t level = view->foldLevel(line);
        if ((level & SC_FOLDLEVELHEADERFLAG)
            && (level & SC_FOLDLEVELNUMBERMASK) == SC_FOLDLEVELBASE) {
            view->foldLine(line, SC_FOLDACTION_CONTRACT);
        }
    }
}

QPair<int, int> SearchResultsDock::selectionLineRange() const
{
    SearchResultView *view = ui->resultView;
    const sptr_t selStart = view->selectionStart();
    const sptr_t selEnd = view->selectionEnd();
    return qMakePair(int(view->lineFromPosition(qMin(selStart, selEnd))),
                     int(view->lineFromPosition(qMax(selStart, selEnd))));
}

bool SearchResultsDock::lineIsFileHeader(int line) const
{
    SearchResultView *view = ui->resultView;
    if (line < 0 || line >= int(view->lineCount())) {
        return false;
    }
    // File header lines are the only ones starting with a space (result
    // lines start with a tab, search headers with the label).
    const sptr_t lineStart = view->positionFromLine(line);
    return view->textRange(lineStart, lineStart + 1) == QByteArrayLiteral(" ");
}

QStringList SearchResultsDock::filePathsInRange(int fromLine, int toLine) const
{
    fromLine = qMax(0, fromLine);
    toLine = qMin<int>(toLine, m_foundInfos.count() - 1);
    if (toLine < fromLine) {
        return {};
    }

    // When the range ends on a file header line, the first result line below it belongs to it as well.
    if (lineIsFileHeader(toLine) && toLine + 1 < m_foundInfos.count()) {
        ++toLine;
    }

    QStringList paths;
    for (int line = fromLine; line <= toLine; ++line) {
        const QString &path = m_foundInfos[line].filePath;
        if (!path.isEmpty() && !paths.contains(path)) {
            paths.append(path);
        }
    }
    return paths;
}

void SearchResultsDock::adjustPendingLine(int *pendingLine, int removedFrom, int removedCount)
{
    if (!pendingLine || *pendingLine < 0) {
        return;
    }
    if (*pendingLine >= removedFrom + removedCount) {
        *pendingLine -= removedCount;
    } else if (*pendingLine >= removedFrom) {
        *pendingLine = -1;
    }
}
