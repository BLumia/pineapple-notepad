#pragma once

#include <QObject>
#include <functional>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

#include "findinfilessearcher.h"

class SciEdit;
class FindReplaceDialog;
class SearchResultsDock;
class DocumentManager;
class EditorViewManager;

class FindReplaceController : public QObject
{
    Q_OBJECT

public:
    using EditorProvider = std::function<SciEdit *()>;

    explicit FindReplaceController(EditorProvider editorProvider, QObject *parent = nullptr);
    ~FindReplaceController() override;

    void showDialog(bool showReplace);
    void showFindInFilesDialog();
    void showMarkDialog();
    bool findNext(bool forward, bool showErrorMessage);
    bool quickFindNext(bool forward);
    bool replaceOne(bool showErrorMessage);
    int replaceAll(bool showErrorMessage);

    void markAll();
    void clearMarks();
    void copyMarkedText();

    void setSearchResultsDock(SearchResultsDock *dock);
    void setDocumentManager(DocumentManager *docManager);
    void setEditorViewManager(EditorViewManager *viewManager);
    void setEnsureDockCallback(std::function<void()> cb);

    void findAllInCurrentDocument();
    void findAllInAllDocuments();
    void replaceAllInAllDocuments();
    void findInFiles();
    void countMatches();

    // "Find in these search results": re-runs a search
    // on an explicit file list and streams the results into `destDock`.
    // When `lineFilter` is given, a match is kept only if the file's line is
    // listed in it (the "Search only in found lines" option). The directory/filter
    // members of `params` are ignored.
    void findInFilesList(const FindInFilesSearcher::Params &params,
                         const QStringList &filePaths,
                         const QHash<QString, QSet<int>> *lineFilter,
                         SearchResultsDock *destDock);

private:
    void ensureDialog();
    void prepareDialogCommon();
    QPair<int, int> resolveSearchRange(SciEdit *editor);
    void clearMarksInternal(SciEdit *editor, int styleIndex, bool bookmarkLine);
    void copyMarkedTextForMask(int styleMask);

    int findMatchesInEditor(SciEdit *editor, int flags, const QByteArray &needle, int maxLen);
    int performReplaceInEditor(SciEdit *editor, int flags, const QByteArray &needle,
                               const QByteArray &replacement, int rangeStart, int rangeEnd);

    // Returns a FindInFilesSearcher::ContentProvider that serves the live
    // text of open documents, so unsaved-modified and untitled documents are
    // searched with their editor content instead of the disk state.
    FindInFilesSearcher::ContentProvider openDocumentContentProvider() const;

    void ensureSearchResultsDock();

    EditorProvider m_editorProvider;
    FindReplaceDialog *m_dialog = nullptr;
    SearchResultsDock *m_searchResultsDock = nullptr;
    DocumentManager *m_documentManager = nullptr;
    EditorViewManager *m_viewManager = nullptr;
    std::function<void()> m_ensureDockCallback;
    bool m_findInSelectionEnabled = false;
    int m_findSelectionStart = 0;
    int m_findSelectionEnd = 0;
    int m_lastZeroLengthMatchPos = -1;
};
