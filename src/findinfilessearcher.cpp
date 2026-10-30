#include "findinfilessearcher.h"

#include "findreplacedialog.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringConverter>
#include <QVector>

#include <KEncodingProber>

#include <functional>

using namespace Qt::Literals::StringLiterals;

namespace {

QRegularExpression wildcardToRegex(const QString &pattern)
{
    QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern));
    re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    return re;
}

struct FileFilter
{
    QVector<QRegularExpression> includes;
    QVector<QRegularExpression> excludes;
    QVector<QRegularExpression> excludeDirs;         // top-level only (level == 1)
    QVector<QRegularExpression> excludeDirsRecursive; // all levels
};

// Parse filter syntax:
//   *.cpp *.h        -> include
//   !*.exe !*.obj    -> exclude file
//   !\tests !\bin*   -> exclude top-level directory
//   !+\log*          -> exclude directory at all levels
FileFilter parseFilters(const QString &raw)
{
    FileFilter filter;
    const QStringList tokens = raw.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    for (const QString &token : tokens) {
        if (token.startsWith(u"!+\\"_s) || token.startsWith(u"!+/"_s)) {
            filter.excludeDirsRecursive.append(wildcardToRegex(token.mid(3)));
        } else if (token.startsWith(u"!\\"_s) || token.startsWith(u"!/"_s)) {
            filter.excludeDirs.append(wildcardToRegex(token.mid(2)));
        } else if (token.startsWith(u'!')) {
            filter.excludes.append(wildcardToRegex(token.mid(1)));
        } else {
            filter.includes.append(wildcardToRegex(token));
        }
    }

    // If no includes, or all patterns were exclusions, match everything.
    if (filter.includes.isEmpty()) {
        filter.includes.append(wildcardToRegex(u"*"_s));
    }

    return filter;
}

bool fileMatchesFilter(const QString &fileName, const FileFilter &filter)
{
    for (const QRegularExpression &ex : filter.excludes) {
        if (ex.match(fileName).hasMatch()) {
            return false;
        }
    }
    for (const QRegularExpression &in : filter.includes) {
        if (in.match(fileName).hasMatch()) {
            return true;
        }
    }
    return false;
}

bool dirExcluded(const QString &dirName, int level, const FileFilter &filter)
{
    for (const QRegularExpression &ex : filter.excludeDirsRecursive) {
        if (ex.match(dirName).hasMatch()) {
            return true;
        }
    }
    if (level == 1) {
        for (const QRegularExpression &ex : filter.excludeDirs) {
            if (ex.match(dirName).hasMatch()) {
                return true;
            }
        }
    }
    return false;
}

// Recursively collect files that match the filter.
// level is the depth relative to the search root (root itself is level 0).
void collectFiles(const QString &dir, int level, const FileFilter &filter,
                  bool recursive, bool includeHidden, QStringList &out)
{
    const QDir qdir(dir);
    const QFileInfoList entries = qdir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

    for (const QFileInfo &fi : entries) {
        if (fi.isDir()) {
            if (!includeHidden && fi.isHidden()) {
                continue;
            }
            if (!recursive) {
                continue;
            }
            if (dirExcluded(fi.fileName(), level + 1, filter)) {
                continue;
            }
            collectFiles(fi.absoluteFilePath(), level + 1, filter, recursive, includeHidden, out);
        } else if (fi.isFile()) {
            if (!includeHidden && fi.isHidden()) {
                continue;
            }
            if (fileMatchesFilter(fi.fileName(), filter)) {
                out.append(fi.absoluteFilePath());
            }
        }
    }
}

QString codecNameForBom(const QByteArray &raw)
{
    if (raw.startsWith("\xEF\xBB\xBF")) {
        return QStringLiteral("UTF-8");
    }
    if (raw.size() >= 2 && (unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE) {
        if (raw.size() >= 4 && (unsigned char)raw[2] == 0x00 && (unsigned char)raw[3] == 0x00) {
            return QStringLiteral("UTF-32LE");
        }
        return QStringLiteral("UTF-16LE");
    }
    if (raw.size() >= 2 && (unsigned char)raw[0] == 0xFE && (unsigned char)raw[1] == 0xFF) {
        return QStringLiteral("UTF-16BE");
    }
    if (raw.size() >= 4 && (unsigned char)raw[0] == 0x00 && (unsigned char)raw[1] == 0x00
        && (unsigned char)raw[2] == 0xFE && (unsigned char)raw[3] == 0xFF) {
        return QStringLiteral("UTF-32BE");
    }
    return {};
}

QString decodeContent(const QByteArray &raw)
{
    const QString bomCodec = codecNameForBom(raw);
    if (!bomCodec.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QStringDecoder decoder(bomCodec.toUtf8());
#else
        QStringDecoder decoder(bomCodec.toUtf8().constData());
#endif
        if (decoder.isValid()) {
            QString text = decoder.decode(raw);
            if (!decoder.hasError()) {
                return text;
            }
        }
    }

    // Try strict UTF-8 first.
    {
        QStringDecoder utf8(QStringConverter::Utf8);
        const QString text = utf8.decode(raw);
        if (!utf8.hasError()) {
            return text;
        }
    }

    // Fall back to encoding prober.
    const int probeLen = qMin(raw.size(), 1 << 20);
    KEncodingProber prober(KEncodingProber::Universal);
    prober.feed(QByteArray::fromRawData(raw.constData(), probeLen));
    const QByteArray probed = prober.encoding();
    if (!probed.isEmpty() && prober.confidence() >= 0.6f) {
        QStringDecoder decoder(probed);
        if (decoder.isValid()) {
            QString text = decoder.decode(raw);
            if (!decoder.hasError()) {
                return text;
            }
        }
    }

    // Last resort: lenient UTF-8.
    return QString::fromUtf8(raw);
}

bool isBinary(const QByteArray &raw)
{
    const int checkLen = qMin(raw.size(), 8192);
    for (int i = 0; i < checkLen; ++i) {
        if (raw.at(i) == '\0') {
            return true;
        }
    }
    return false;
}

bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

bool isWordBoundary(const QString &text, int before, int after)
{
    const bool beforeIsWord = (before > 0) && isWordChar(text.at(before - 1));
    const bool afterIsWord = (after < text.size()) && isWordChar(text.at(after));
    return !(beforeIsWord && afterIsWord);
}

void searchLineLiteral(const QString &line, const QString &needle,
                       bool matchCase, bool wholeWord, QVector<FindInFilesSearcher::Match> &out)
{
    const Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
    int from = 0;
    while (from <= line.size()) {
        const int idx = line.indexOf(needle, from, cs);
        if (idx < 0) {
            break;
        }
        const int end = idx + needle.size();
        if (!wholeWord || isWordBoundary(line, idx, end)) {
            FindInFilesSearcher::Match m;
            m.matchStartCol = idx;
            m.matchEndCol = end;
            out.append(m);
        }
        from = (end > idx) ? end : idx + 1;
    }
}

void searchLineRegex(const QString &line, const QRegularExpression &re, bool wholeWord,
                     QVector<FindInFilesSearcher::Match> &out)
{
    auto it = re.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const int idx = match.capturedStart();
        const int end = match.capturedEnd();
        if (!wholeWord || isWordBoundary(line, idx, end)) {
            FindInFilesSearcher::Match m;
            m.matchStartCol = idx;
            m.matchEndCol = end;
            out.append(m);
        }
        if (end == idx) {
            break;
        }
    }
}

} // namespace

FindInFilesSearcher::Stats FindInFilesSearcher::search(const Params &params,
                                                       const CancelChecker &shouldCancel,
                                                       const ProgressReporter &progress,
                                                       const FileResultCallback &fileResult,
                                                       const ContentProvider &contentProvider)
{
    const FileFilter filter = parseFilters(params.filters);

    QStringList files;
    collectFiles(params.directory, 0, filter, params.recursive, params.includeHidden, files);

    return searchFiles(files, params, shouldCancel, progress, fileResult, contentProvider);
}

FindInFilesSearcher::Stats FindInFilesSearcher::searchFiles(const QStringList &filePaths,
                                                            const Params &params,
                                                            const CancelChecker &shouldCancel,
                                                            const ProgressReporter &progress,
                                                            const FileResultCallback &fileResult,
                                                            const ContentProvider &contentProvider)
{
    Stats stats;

    const QString needle = params.needle;
    if (needle.isEmpty() && params.mode != SearchMode::Regex) {
        return stats;
    }

    // Build the regex once if in regex mode.
    QRegularExpression regex;
    if (params.mode == SearchMode::Regex) {
        QRegularExpression::PatternOptions opts = QRegularExpression::UseUnicodePropertiesOption;
        if (!params.matchCase) {
            opts |= QRegularExpression::CaseInsensitiveOption;
        }
        regex.setPattern(needle);
        regex.setPatternOptions(opts);
        if (!regex.isValid()) {
            return stats;
        }
    }

    for (const QString &filePath : filePaths) {
        if (shouldCancel && shouldCancel()) {
            stats.cancelled = true;
            break;
        }

        progress(filePath, stats.filesSearched, stats.filesWithHits, filePaths.size());

        // Prefer live content of an open document (may be untitled or carry
        // unsaved changes); fall back to reading the file from disk.
        QString content;
        if (!contentProvider || !contentProvider(filePath, content)) {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }

            const QByteArray raw = file.readAll();
            file.close();

            if (isBinary(raw)) {
                ++stats.filesSearched;
                continue;
            }

            content = decodeContent(raw);
        }

        // Iterate over lines, matching Scintilla line counting (CR, LF, CRLF each
        // count as a single line break).
        QVector<Match> matches;
        int lineStart = 0;
        int lineNum = 0;
        const int contentSize = content.size();
        bool done = false;
        while (!done) {
            // Find the next line-break character.
            int eolPos = -1;
            for (int i = lineStart; i < contentSize; ++i) {
                const QChar c = content.at(i);
                if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
                    eolPos = i;
                    break;
                }
            }

            int contentEnd;
            int nextStart;
            if (eolPos == -1) {
                contentEnd = contentSize;
                nextStart = contentSize;
                done = true;
            } else {
                contentEnd = eolPos;
                nextStart = eolPos + 1;
                // Consume the LF of a CRLF pair together.
                if (content.at(eolPos) == QLatin1Char('\r')
                    && nextStart < contentSize
                    && content.at(nextStart) == QLatin1Char('\n')) {
                    ++nextStart;
                }
            }

            const QString lineText = content.mid(lineStart, contentEnd - lineStart);

            const int matchCountBefore = matches.size();
            if (params.mode == SearchMode::Regex) {
                searchLineRegex(lineText, regex, params.wholeWord, matches);
            } else {
                searchLineLiteral(lineText, needle, params.matchCase, params.wholeWord, matches);
            }

            for (int i = matchCountBefore; i < matches.size(); ++i) {
                matches[i].line = lineNum;
                matches[i].lineText = lineText;
            }

            lineStart = nextStart;
            ++lineNum;
        }

        ++stats.filesSearched;

        if (!matches.isEmpty()) {
            stats.totalHits += matches.size();
            ++stats.filesWithHits;
            fileResult(filePath, lineNum, matches);
        }
    }

    return stats;
}
