#pragma once

#include <QDockWidget>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>
#include <ScintillaTypes.h>

#include <vector>

namespace Ui {
class SearchResultsDock;
}

namespace pineapple {
class SearchResultLexer;
struct SearchResultMarkingLine;
}

class DocumentManager;
class EditorViewManager;
class QPoint;

// Search results dock hosting a Scintilla view (SearchResultView) styled by
// the custom "searchResult" lexer. Results are streamed in by FindReplaceController
// through newSearch()/newFileEntry()/newResultsEntry()/completeSearch();
// the view's buffer lines stay in 1:1 correspondence with m_foundInfos/m_markings
// for navigation and highlighting.
class SearchResultsDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit SearchResultsDock(QWidget *parent = nullptr);
    ~SearchResultsDock() override;

    void setDocumentManager(DocumentManager *docManager);
    void setEditorViewManager(EditorViewManager *viewManager);

    // A volatile dock hosts the results of "Find in these search results":
    // it offers "Close these search results"
    // instead of "Clear all" and is destroyed on close.
    void setVolatile(bool volatileDock);
    bool isVolatile() const;

    // Unique file paths referenced by the whole panel (any line range).
    QStringList allResultFilePaths() const;
    // Per-file set of source line numbers (0-based) currently listed in the
    // panel; used to implement "Search only in found lines" option.
    QHash<QString, QSet<int>> resultLinesByFile() const;

    // Applies the editor font/theme/tab width settings; called on creation
    // and whenever the corresponding settings change.
    void applyEditorSettings();

    void newSearch(const QString &searchTerm);
    void newFileEntry(int docId, const QString &filePath, int totalLines);
    void newFileEntryForPath(const QString &filePath, int totalLines);
    // relStart/relEnd locate the match inside the source line (UTF-16 code
    // units); displayStart/displayEnd locate it inside displayLine. When
    // several matches hit the same source line, they are merged into a single
    // result entry, unless the extra match falls outside the displayed
    // (possibly truncated) part.
    void newResultsEntry(const QString &displayLine, int lineNumber,
                         int relStart, int relEnd,
                         int displayStart, int displayEnd,
                         int docStartPos, int docEndPos, int hitCount = 1);
    void newResultsEntryForFile(const QString &displayLine, int lineNumber,
                                int relStart, int relEnd,
                                int displayStart, int displayEnd);
    void completeSearch();

    void setSearchInfo(int searchedFiles, int filesWithHits);
    // Extra text appended to the search header line by completeSearch()
    // (e.g. the line-filter note of a Find-in-Finder search).
    void setSearchScopeNote(const QString &note);

signals:
    void searchResultActivated(int docId, int lineNumber, int startPos, int endPos);
    void searchResultActivatedForFile(const QString &filePath, int lineNumber, int startCol, int endCol);
    // Emitted for the "Find in these search results..." context menu item;
    // the sender dock is the search source.
    void findInResultsRequested();
    // Emitted by a volatile dock's "Close these search results" item.
    void closeRequested();

public slots:
    void collapseAll();
    void expandAll();
    void selectAll();
    void copySelection();
    void copyLines();
    void copySelectedPaths();
    void setWordWrap(bool enabled);
    void deleteEntry(int line);
    void deleteAll();

private slots:
    void handleDoubleClick(Scintilla::Position position, Scintilla::Position line);
    void showContextMenu(const QPoint &globalPos, int line);

private:
    // Navigation metadata for one buffer line. Header lines carry default
    // entries (lineNumber < 0); result lines carry either a docId-based hit
    // or a file-path-based hit (Find in Files). A merged entry (several
    // matches on the same source line) keeps one range per match, parallel to
    // the marking segments of the buffer line: docId-based entries store
    // document positions, file-based entries store columns within the line.
    struct FoundInfo {
        int docId = -1;
        QString filePath;
        int lineNumber = -1;
        bool isFileBased = false;
        QVector<QPair<int, int>> ranges;
    };

    void finishPendingFileHeader();
    void insertTextAtLineEnd(int line, const QString &text);
    int appendResultLine(const QString &lineText, const FoundInfo &info);
    // Composes "\t<Line> <right-justified line number>: " for a result line;
    // the label is translatable and the number is padded to the width of the
    // current file's largest line number.
    QString resultLinePrefix(int lineNumber) const;
    // Tries to merge a match into the previously appended result entry (same
    // source line, match visible within the entry's display window) instead
    // of creating another buffer line. Returns true when merged.
    bool tryMergeIntoLastEntry(int lineNumber, int relStart, int relEnd,
                               const QPair<int, int> &range);
    void markMatchOnLastLine(int prefixBytes, const QString &displayLine,
                             int displayStart, int displayEnd);
    void contractPreviousSearches();
    // First and last buffer line covered by the current selection (both are
    // the caret line for an empty selection).
    QPair<int, int> selectionLineRange() const;
    // True when the buffer line is a file header line (leading space).
    bool lineIsFileHeader(int line) const;
    // File paths referenced by the given line range (deduplicated), for the
    // "Copy Selected Pathname(s)" action.
    QStringList filePathsInRange(int fromLine, int toLine) const;
    static void adjustPendingLine(int *pendingLine, int removedFrom, int removedCount);

    Ui::SearchResultsDock *ui = nullptr;
    DocumentManager *m_documentManager = nullptr;
    EditorViewManager *m_viewManager = nullptr;
    // Owned by the view's Scintilla document after setLexer(); never replaced.
    pineapple::SearchResultLexer *m_lexer = nullptr;
    // True for docks hosting "Find in these search results" results.
    bool m_volatile = false;

    QString m_searchTerm;
    QString m_currentFilePath;
    int m_currentDocId = -1;

    int m_currentFileCount = 0;
    int m_totalFileHitCount = 0;
    int m_totalHitCount = 0;
    int m_searchedFiles = 0;
    int m_filesWithHits = 0;

    int m_currentSearchLine = -1;
    int m_currentFileLine = -1;
    QString m_scopeNote;

    // Digit width used to right-justify result line numbers; derived from the
    // current file's total line count.
    int m_lineNumberDigits = 1;

    // Streaming state for merging several matches of the same source line into one result entry:
    // source line number of the last appended entry, plus the displayed window
    // (UTF-16 offsets within the source line) that entry covers.
    int m_lastSourceLine = -1;
    int m_lastWindowStart = 0;
    int m_lastPrefixBytes = 0;
    QString m_lastDisplayLine;

    // index == buffer line number, kept in sync with the view's document.
    QVector<FoundInfo> m_foundInfos;
    std::vector<pineapple::SearchResultMarkingLine> m_markings;
};
