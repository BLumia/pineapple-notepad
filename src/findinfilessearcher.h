#pragma once

#include <QString>
#include <QVector>

enum class SearchMode;

class FindInFilesSearcher
{
public:
    struct Match
    {
        int line = 0;            // 0-based line number
        QString lineText;        // full line text (without EOL)
        int matchStartCol = 0;   // character offset of match start within the line
        int matchEndCol = 0;     // character offset of match end within the line
    };

    struct Params
    {
        QString directory;
        QString filters;
        bool recursive = true;
        bool includeHidden = false;
        QString needle;          // already decoded (extended-mode escapes applied)
        bool matchCase = false;
        bool wholeWord = false;
        SearchMode mode;
    };

    struct Stats
    {
        int totalHits = 0;
        int filesWithHits = 0;
        int filesSearched = 0;
        bool cancelled = false;
    };

    using CancelChecker = std::function<bool()>;
    using ProgressReporter = std::function<void(const QString &currentFile, int searched, int matched, int total)>;
    // totalLines is the 1-based line count of the scanned file, used by the
    // results dock to right-justify line numbers.
    using FileResultCallback = std::function<void(const QString &filePath, int totalLines, const QVector<Match> &matches)>;
    // Supplies the text of a file instead of reading it from disk. Used so
    // currently open documents (including untitled and unsaved-modified
    // ones) are searched with their live editor content.
    // Return false to fall back to disk.
    using ContentProvider = std::function<bool(const QString &filePath, QString &content)>;

    static Stats search(const Params &params,
                        const CancelChecker &shouldCancel,
                        const ProgressReporter &progress,
                        const FileResultCallback &fileResult,
                        const ContentProvider &contentProvider = {});

    // Searches an explicit file list instead of a directory tree; used by
    // "Find in these search results", which re-runs a search on the files
    // listed in an existing results panel.
    static Stats searchFiles(const QStringList &filePaths,
                             const Params &params,
                             const CancelChecker &shouldCancel,
                             const ProgressReporter &progress,
                             const FileResultCallback &fileResult,
                             const ContentProvider &contentProvider = {});
};
