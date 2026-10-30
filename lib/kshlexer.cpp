#include "kshlexer.h"
#include "apppaths.h"

#include <KSyntaxHighlighting/AbstractHighlighter>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/FoldingRegion>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/Theme>

#include <ILexer.h>
#include <Scintilla.h>

#include <QMetaEnum>
#include <QRegularExpression>
#include <QString>
#include <QStringView>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

// Scintilla reserves styleId STYLE_DEFAULT (31); extended (per-Format) styles
// start just above it. Base styles (one per KSH TextStyle) occupy 0..N-1.
constexpr int kExtendedStyleStart = STYLE_LASTPREDEFINED + 1;

const QMetaEnum &kshTextStyleMetaEnum()
{
    static const auto e = QMetaEnum::fromType<KSyntaxHighlighting::Theme::TextStyle>();
    return e;
}

int kshTextStyleCount()
{
    return kshTextStyleMetaEnum().keyCount();
}

} // namespace

namespace kshutils {

KSyntaxHighlighting::Repository &repository()
{
    static KSyntaxHighlighting::Repository repo;
    static bool initialized = false;
    if (!initialized) {
        repo.addCustomSearchPath(AppPaths::appDataSubdirPath(QStringLiteral(".")));
        initialized = true;
    }
    return repo;
}

} // namespace kshutils

namespace {

class KshLexer final : public Scintilla::ILexer5, public KSyntaxHighlighting::AbstractHighlighter {
private:
    int refCount_ = 1;
    std::string propertyValueStorage_;
    std::string m_definitionName;
    std::string m_themeName;
    bool m_foldingEnabled = false;
    bool m_extendedStylesEnabled = true;

    std::unordered_map<int, KSyntaxHighlighting::State> m_lineStates;

    char *m_pStyleBytes = nullptr;
    size_t m_styleBufferSize = 0;
    Sci_Position m_bufStartDoc = 0;
    Sci_Position m_curLineStartDoc = 0;
    const int *m_pQposToByte = nullptr;
    int m_qposMapSize = 0;

    bool m_foldMode = false;
    int m_depth = 0;
    int m_depthMin = 0;
    std::vector<std::pair<int, int>> m_openRegions;
    std::unordered_map<int, std::vector<std::pair<int, int>>> m_lineOpenRegions;
    std::vector<int> m_pendingEnds;
    std::vector<int> m_pendingBegins;

    struct ExtendedStyleEntry {
        KSyntaxHighlighting::Format format;
        int textStyleId = 0;
        QByteArray name;
    };
    std::unordered_map<int, int> m_formatToScinStyle;
    std::map<int, ExtendedStyleEntry> m_extendedStyles;
    std::map<int, bool> m_styleSpellCheck;
    int m_nextExtendedStyle = kExtendedStyleStart;
    std::string m_stylesPropertyCache;

    bool m_indentFoldEnabled = false;
    QList<QRegularExpression> m_indentFoldIgnorePatterns;
    int m_tabWidth = 8;

public:
    KshLexer() = default;

    // --- ILexer5 / ILexer4 ---

    int SCI_METHOD Version() const override
    {
        return Scintilla::lvRelease5;
    }

    void SCI_METHOD Release() override
    {
        --refCount_;
        if (refCount_ <= 0)
            delete this;
    }

    const char *SCI_METHOD PropertyNames() override {
        return "lexer.ksyntaxhighlighting.definition\n"
               "lexer.ksyntaxhighlighting.theme\n"
               "lexer.ksyntaxhighlighting.extended-styles\n"
               "lexer.ksyntaxhighlighting.tabwidth\n"
               "fold\n";
    }
    int SCI_METHOD PropertyType(const char *name) override {
        if (std::strcmp(name, "lexer.ksyntaxhighlighting.definition") == 0)
            return 2;
        if (std::strcmp(name, "lexer.ksyntaxhighlighting.theme") == 0)
            return 2;
        if (std::strcmp(name, "lexer.ksyntaxhighlighting.extended-styles") == 0)
            return 0;
        if (std::strcmp(name, "lexer.ksyntaxhighlighting.tabwidth") == 0)
            return 1;
        if (std::strcmp(name, "fold") == 0)
            return 0;
        return 0;
    }
    const char *SCI_METHOD DescribeProperty(const char *name) override
    {
        if (std::strcmp(name, "lexer.ksyntaxhighlighting.definition") == 0)
            return "KSH syntax definition name (e.g. SCSS, CSS)";
        if (std::strcmp(name, "lexer.ksyntaxhighlighting.theme") == 0)
            return "KSH theme name for resolving per-Format font attributes";
        if (std::strcmp(name, "lexer.ksyntaxhighlighting.extended-styles") == 0)
            return "Enable per-Format extended styles for bold/italic/underline variants (0 or 1, default 1)";
        if (std::strcmp(name, "lexer.ksyntaxhighlighting.tabwidth") == 0)
            return "Tab width for indentation-based folding expansion";
        if (std::strcmp(name, "fold") == 0)
            return "Enable code folding (0 or 1)";
        return "";
    }
    Sci_Position SCI_METHOD PropertySet(const char *key, const char *val) override
    {
        if (!key)
            return -1;
        if (std::strcmp(key, "lexer.ksyntaxhighlighting.definition") == 0) {
            m_definitionName = val ? val : "";
            const auto def = kshutils::repository().definitionForName(QString::fromUtf8(m_definitionName));
            if (def.isValid()) {
                setDefinition(def);
                rebuildFormatMap();
                return 0;
            }
            m_formatToScinStyle.clear();
            m_extendedStyles.clear();
            m_nextExtendedStyle = kExtendedStyleStart;
            m_stylesPropertyCache.clear();
            m_indentFoldEnabled = false;
            m_indentFoldIgnorePatterns.clear();
            return -1;
        }
        if (std::strcmp(key, "lexer.ksyntaxhighlighting.theme") == 0) {
            m_themeName = val ? val : "";
            KSyntaxHighlighting::Theme theme;
            if (!m_themeName.empty())
                theme = kshutils::repository().theme(QString::fromUtf8(m_themeName));
            if (!theme.isValid())
                theme = kshutils::repository().defaultTheme(KSyntaxHighlighting::Repository::LightTheme);
            setTheme(theme);
            rebuildStylesProperty();
            return 0;
        }
        if (std::strcmp(key, "lexer.ksyntaxhighlighting.extended-styles") == 0) {
            m_extendedStylesEnabled = !(val && std::strcmp(val, "0") == 0);
            rebuildFormatMap();
            return 0;
        }
        if (std::strcmp(key, "lexer.ksyntaxhighlighting.tabwidth") == 0) {
            m_tabWidth = val ? std::max(1, std::atoi(val)) : 8;
            return 0;
        }
        if (std::strcmp(key, "fold") == 0) {
            m_foldingEnabled = val && (std::strcmp(val, "1") == 0);
            return 0;
        }
        return -1;
    }
    const char *SCI_METHOD DescribeWordListSets() override { return ""; }
    Sci_Position SCI_METHOD WordListSet(int, const char *) override { return -1; }

    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc, int,
                        Scintilla::IDocument *pAccess) override
    {
        if (!pAccess)
            return;

        const Sci_Position docLength = pAccess->Length();
        if (docLength <= 0)
            return;

        Sci_Position start = static_cast<Sci_Position>(startPos);
        if (start < 0)
            start = 0;
        if (start >= docLength)
            start = docLength - 1;
        start = pAccess->LineStart(pAccess->LineFromPosition(start));

        Sci_Position end = start + lengthDoc;
        if (end < start)
            end = start;
        if (end >= docLength)
            end = docLength;
        else
            end = pAccess->LineEnd(pAccess->LineFromPosition(end));

        if (end <= start)
            return;

        const size_t bufferLen = static_cast<size_t>(end - start);
        std::string text(bufferLen, '\0');
        pAccess->GetCharRange(text.data(), start, end - start);

        std::vector<char> styleBytes(bufferLen, 0);

        m_pStyleBytes = styleBytes.data();
        m_styleBufferSize = bufferLen;
        m_bufStartDoc = start;

        const int startLine = static_cast<int>(pAccess->LineFromPosition(start));

        KSyntaxHighlighting::State inputState;
        if (startLine > 0) {
            auto it = m_lineStates.find(startLine - 1);
            if (it != m_lineStates.end())
                inputState = it->second;
        }

        size_t pos = 0;
        int lineNum = startLine;
        while (pos < text.size()) {
            size_t lineEnd = pos;
            while (lineEnd < text.size() && text[lineEnd] != '\r' && text[lineEnd] != '\n')
                ++lineEnd;

            const char *linePtr = text.data() + pos;
            const int lineByteLen = static_cast<int>(lineEnd - pos);
            const QString lineText = QString::fromUtf8(linePtr, lineByteLen);

            std::vector<int> qposToByte;
            const bool allAscii = isAllAscii(linePtr, lineByteLen);
            if (!allAscii)
                qposToByte = buildOffsetMap(lineText, lineByteLen);

            m_curLineStartDoc = start + static_cast<Sci_Position>(pos);
            m_pQposToByte = allAscii ? nullptr : qposToByte.data();
            m_qposMapSize = static_cast<int>(qposToByte.size());

            const KSyntaxHighlighting::State newState =
                highlightLine(QStringView(lineText), inputState);

            auto prevIt = m_lineStates.find(lineNum);
            const bool hadState = (prevIt != m_lineStates.end());
            const bool changed = !hadState || (newState != prevIt->second);

            m_lineStates[lineNum] = newState;

            if (changed) {
                const int oldScin = pAccess->GetLineState(lineNum);
                pAccess->SetLineState(lineNum, oldScin + 1);
            }

            inputState = newState;

            if (lineEnd < text.size()) {
                if (text[lineEnd] == '\r' && lineEnd + 1 < text.size() && text[lineEnd + 1] == '\n')
                    pos = lineEnd + 2;
                else
                    pos = lineEnd + 1;
            } else {
                pos = lineEnd;
            }
            ++lineNum;
        }

        m_pStyleBytes = nullptr;
        m_pQposToByte = nullptr;

        pAccess->StartStyling(start);
        pAccess->SetStyles(end - start, styleBytes.data());
    }

    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc, int,
                         Scintilla::IDocument *pAccess) override
    {
        if (!pAccess || !m_foldingEnabled)
            return;

        const auto &def = definition();
        if (!def.isValid())
            return;

        const Sci_Position docLength = pAccess->Length();
        if (docLength <= 0)
            return;

        Sci_Position start = static_cast<Sci_Position>(startPos);
        if (start < 0)
            start = 0;
        if (start >= docLength)
            start = docLength - 1;
        start = pAccess->LineStart(pAccess->LineFromPosition(start));

        Sci_Position end = start + lengthDoc;
        if (end < start)
            end = start;
        if (end >= docLength)
            end = docLength;
        else
            end = pAccess->LineEnd(pAccess->LineFromPosition(end));

        if (end <= start)
            return;

        const size_t bufferLen = static_cast<size_t>(end - start);
        std::string text(bufferLen, '\0');
        pAccess->GetCharRange(text.data(), start, end - start);

        const int startLine = static_cast<int>(pAccess->LineFromPosition(start));

        KSyntaxHighlighting::State inputState;
        if (startLine > 0) {
            auto it = m_lineStates.find(startLine - 1);
            if (it != m_lineStates.end())
                inputState = it->second;
        }

        if (m_indentFoldEnabled) {
            foldByIndentation(pAccess, text, startLine, inputState);
            return;
        }

        m_openRegions.clear();
        if (startLine > 0) {
            auto it = m_lineOpenRegions.find(startLine - 1);
            if (it != m_lineOpenRegions.end())
                m_openRegions = it->second;
        }
        m_depth = 0;
        for (const auto &entry : m_openRegions)
            m_depth += entry.second;

        m_foldMode = true;

        size_t pos = 0;
        int lineNum = startLine;
        while (pos < text.size()) {
            size_t lineEnd = pos;
            while (lineEnd < text.size() && text[lineEnd] != '\r' && text[lineEnd] != '\n')
                ++lineEnd;

            const char *linePtr = text.data() + pos;
            const int lineByteLen = static_cast<int>(lineEnd - pos);
            const QString lineText = QString::fromUtf8(linePtr, lineByteLen);

            m_depthMin = m_depth;
            m_pendingEnds.clear();
            m_pendingBegins.clear();
            inputState = highlightLine(QStringView(lineText), inputState);
            processPendingFolds();

            int lev = SC_FOLDLEVELBASE + m_depthMin;
            if (m_depth > m_depthMin)
                lev |= SC_FOLDLEVELHEADERFLAG;
            if (lineByteLen == 0 || isAllWhitespace(linePtr, lineByteLen))
                lev |= SC_FOLDLEVELWHITEFLAG;

            pAccess->SetLevel(lineNum, lev);

            m_lineOpenRegions[lineNum] = m_openRegions;

            if (lineEnd < text.size()) {
                if (text[lineEnd] == '\r' && lineEnd + 1 < text.size() && text[lineEnd + 1] == '\n')
                    pos = lineEnd + 2;
                else
                    pos = lineEnd + 1;
            } else {
                pos = lineEnd;
            }
            ++lineNum;
        }

        m_foldMode = false;
    }

    void *SCI_METHOD PrivateCall(int, void *) override { return nullptr; }
    int SCI_METHOD LineEndTypesSupported() override { return 0; }
    int SCI_METHOD AllocateSubStyles(int, int) override { return -1; }
    int SCI_METHOD SubStylesStart(int) override { return -1; }
    int SCI_METHOD SubStylesLength(int) override { return 0; }
    int SCI_METHOD StyleFromSubStyle(int subStyle) override { return subStyle; }
    int SCI_METHOD PrimaryStyleFromStyle(int style) override
    {
        const int baseCount = kshTextStyleCount();
        if (style < baseCount)
            return style;
        auto it = m_extendedStyles.find(style);
        if (it != m_extendedStyles.end())
            return it->second.textStyleId;
        return style;
    }
    void SCI_METHOD FreeSubStyles() override {}
    void SCI_METHOD SetIdentifiers(int, const char *) override {}
    int SCI_METHOD DistanceToSecondaryStyles() override { return 0; }
    const char *SCI_METHOD GetSubStyleBases() override { return ""; }

    int SCI_METHOD NamedStyles() override
    {
        return m_extendedStyles.empty() ? kshTextStyleCount() : m_nextExtendedStyle;
    }

    const char *SCI_METHOD NameOfStyle(int style) override
    {
        const int baseCount = kshTextStyleCount();
        if (style < 0)
            return "";
        if (style < baseCount)
            return kshTextStyleMetaEnum().key(style);
        auto it = m_extendedStyles.find(style);
        if (it != m_extendedStyles.end())
            return it->second.name.constData();
        return "";
    }

    const char *SCI_METHOD TagsOfStyle(int) override { return ""; }
    const char *SCI_METHOD DescriptionOfStyle(int style) override { return NameOfStyle(style); }
    const char *SCI_METHOD GetName() override { return "ksyntaxhighlighting"; }
    int SCI_METHOD GetIdentifier() override { return 0; }

    const char *SCI_METHOD PropertyGet(const char *key) override
    {
        if (key && std::strcmp(key, "lexer.ksyntaxhighlighting.definition") == 0) {
            propertyValueStorage_ = m_definitionName;
            return propertyValueStorage_.c_str();
        }
        if (key && std::strcmp(key, "lexer.ksyntaxhighlighting.theme") == 0) {
            propertyValueStorage_ = m_themeName;
            return propertyValueStorage_.c_str();
        }
        if (key && std::strcmp(key, "lexer.ksyntaxhighlighting.extended-styles") == 0) {
            propertyValueStorage_ = m_extendedStylesEnabled ? "1" : "0";
            return propertyValueStorage_.c_str();
        }
        if (key && std::strcmp(key, "lexer.ksyntaxhighlighting.tabwidth") == 0) {
            propertyValueStorage_ = std::to_string(m_tabWidth);
            return propertyValueStorage_.c_str();
        }
        if (key && std::strcmp(key, "fold") == 0) {
            propertyValueStorage_ = m_foldingEnabled ? "1" : "0";
            return propertyValueStorage_.c_str();
        }
        if (key && std::strcmp(key, "lexer.ksyntaxhighlighting.styles") == 0) {
            return m_stylesPropertyCache.c_str();
        }
        propertyValueStorage_.clear();
        return propertyValueStorage_.c_str();
    }

protected:
    // --- AbstractHighlighter ---

    void applyFormat(int offset, int length, const KSyntaxHighlighting::Format &format) override
    {
        if (m_foldMode || !m_pStyleBytes || length <= 0)
            return;

        int styleId = 0;
        if (format.isValid()) {
            const auto it = m_formatToScinStyle.find(format.id());
            if (it != m_formatToScinStyle.end())
                styleId = it->second;
            else
                styleId = static_cast<int>(format.textStyle());
        }

        int byteOffsetInLine;
        int byteLen;

        if (m_pQposToByte) {
            const int safeStart = std::min(offset, m_qposMapSize - 1);
            const int safeEnd = std::min(offset + length, m_qposMapSize - 1);
            byteOffsetInLine = m_pQposToByte[safeStart];
            byteLen = m_pQposToByte[safeEnd] - byteOffsetInLine;
        } else {
            byteOffsetInLine = offset;
            byteLen = length;
        }

        const size_t startIdx = static_cast<size_t>(m_curLineStartDoc - m_bufStartDoc)
                              + static_cast<size_t>(byteOffsetInLine);

        for (int i = 0; i < byteLen; ++i) {
            const size_t idx = startIdx + static_cast<size_t>(i);
            if (idx < m_styleBufferSize)
                m_pStyleBytes[idx] = static_cast<char>(styleId);
        }
    }

private:
    static bool isAllAscii(const char *data, int len)
    {
        for (int i = 0; i < len; ++i) {
            if (static_cast<unsigned char>(data[i]) >= 0x80)
                return false;
        }
        return true;
    }

    static bool isAllWhitespace(const char *data, int len)
    {
        for (int i = 0; i < len; ++i) {
            if (data[i] != ' ' && data[i] != '\t')
                return false;
        }
        return true;
    }

    int computeIndentDepth(const char *data, int len) const
    {
        int d = 0;
        for (int i = 0; i < len; ++i) {
            if (data[i] == ' ') {
                ++d;
            } else if (data[i] == '\t') {
                if (m_tabWidth > 0)
                    d += m_tabWidth - (d % m_tabWidth);
                else
                    ++d;
            } else {
                break;
            }
        }
        return d;
    }

    bool isFoldEmptyLine(const char *linePtr, int lineByteLen, const QString &lineText) const
    {
        if (lineByteLen == 0 || isAllWhitespace(linePtr, lineByteLen))
            return true;
        for (const auto &re : m_indentFoldIgnorePatterns) {
            const auto match = re.match(lineText, 0, QRegularExpression::NormalMatch,
                                        QRegularExpression::AnchorAtOffsetMatchOption);
            if (match.hasMatch() && match.capturedLength() == lineText.length())
                return true;
        }
        return false;
    }

    void applyFolding(int, int, KSyntaxHighlighting::FoldingRegion region) override
    {
        if (!m_foldMode)
            return;
        using FR = KSyntaxHighlighting::FoldingRegion;
        const int id = region.id();
        if (region.type() == FR::Begin)
            m_pendingBegins.push_back(id);
        else if (region.type() == FR::End)
            m_pendingEnds.push_back(id);
    }

    void processPendingFolds()
    {
        for (int id : m_pendingEnds) {
            for (auto it = m_openRegions.begin(); it != m_openRegions.end(); ++it) {
                if (it->first == id) {
                    if (--it->second <= 0)
                        m_openRegions.erase(it);
                    --m_depth;
                    if (m_depth < m_depthMin)
                        m_depthMin = m_depth;
                    break;
                }
            }
        }
        for (int id : m_pendingBegins) {
            bool found = false;
            for (auto &entry : m_openRegions) {
                if (entry.first == id) {
                    ++entry.second;
                    found = true;
                    break;
                }
            }
            if (!found)
                m_openRegions.push_back({id, 1});
            ++m_depth;
        }
    }

    int peekNextNonEmptyIndent(Scintilla::IDocument *pAccess, int startDocLine, int docLineCount) const
    {
        int visited = 0;
        constexpr int lookAheadLimit = 64;
        while (startDocLine < docLineCount && visited < lookAheadLimit) {
            const Sci_Position ls = pAccess->LineStart(startDocLine);
            const Sci_Position le = pAccess->LineEnd(startDocLine);
            if (le > ls) {
                std::string buf(static_cast<size_t>(le - ls), '\0');
                pAccess->GetCharRange(buf.data(), ls, le - ls);
                const QString lineStr = QString::fromUtf8(buf.data(), static_cast<int>(buf.size()));
                if (!isFoldEmptyLine(buf.data(), static_cast<int>(buf.size()), lineStr))
                    return computeIndentDepth(buf.data(), static_cast<int>(buf.size()));
            }
            ++startDocLine;
            ++visited;
        }
        return -1;
    }

    void foldByIndentation(Scintilla::IDocument *pAccess, const std::string &text,
                           int startLine, KSyntaxHighlighting::State inputState)
    {
        struct LineInfo {
            int indent;
            bool transparent;
        };
        std::vector<LineInfo> infos;

        // Pass 1: highlight each line and collect indentation info
        size_t pos = 0;
        while (pos < text.size()) {
            size_t lineEnd = pos;
            while (lineEnd < text.size() && text[lineEnd] != '\r' && text[lineEnd] != '\n')
                ++lineEnd;

            const char *linePtr = text.data() + pos;
            const int lineByteLen = static_cast<int>(lineEnd - pos);
            const QString lineText = QString::fromUtf8(linePtr, lineByteLen);

            inputState = highlightLine(QStringView(lineText), inputState);

            const bool indentActive = inputState.indentationBasedFoldingEnabled();
            const bool empty = isFoldEmptyLine(linePtr, lineByteLen, lineText);
            const int indent = computeIndentDepth(linePtr, lineByteLen);

            infos.push_back({indent, empty || !indentActive});

            if (lineEnd < text.size()) {
                if (text[lineEnd] == '\r' && lineEnd + 1 < text.size() && text[lineEnd + 1] == '\n')
                    pos = lineEnd + 2;
                else
                    pos = lineEnd + 1;
            } else {
                pos = lineEnd;
            }
        }

        // Pre-compute next non-transparent index for each line (reverse scan)
        const int count = static_cast<int>(infos.size());
        std::vector<int> nextNonTransparent(count, -1);
        {
            int next = -1;
            for (int i = count - 1; i >= 0; --i) {
                nextNonTransparent[i] = next;
                if (!infos[static_cast<size_t>(i)].transparent)
                    next = i;
            }
        }

        const int docLineCount = static_cast<int>(pAccess->LineFromPosition(pAccess->Length())) + 1;

        // Restore previous level from document for transparent-line inheritance
        int prevLevelNum = SC_FOLDLEVELBASE;
        if (startLine > 0) {
            const int prevLev = pAccess->GetLevel(startLine - 1);
            prevLevelNum = prevLev & SC_FOLDLEVELNUMBERMASK;
        }

        // Pass 2: assign fold levels
        for (int i = 0; i < count; ++i) {
            int levelNum;
            int flags = 0;

            if (infos[static_cast<size_t>(i)].transparent) {
                // Inherit the higher of prev and next non-transparent levels
                int nextLevelNum = prevLevelNum;
                const int nti = nextNonTransparent[i];
                if (nti >= 0) {
                    nextLevelNum = SC_FOLDLEVELBASE + infos[static_cast<size_t>(nti)].indent;
                } else {
                    // Look beyond buffer
                    const int peeked = peekNextNonEmptyIndent(pAccess, startLine + count, docLineCount);
                    if (peeked >= 0)
                        nextLevelNum = SC_FOLDLEVELBASE + peeked;
                }
                levelNum = std::max(prevLevelNum, nextLevelNum);
                flags = SC_FOLDLEVELWHITEFLAG;
            } else {
                const int indent = infos[static_cast<size_t>(i)].indent;
                levelNum = SC_FOLDLEVELBASE + indent;

                // Determine header flag by comparing with next non-transparent line
                int nextIndent = indent;
                const int nti = nextNonTransparent[i];
                if (nti >= 0) {
                    nextIndent = infos[static_cast<size_t>(nti)].indent;
                } else {
                    const int peeked = peekNextNonEmptyIndent(pAccess, startLine + count, docLineCount);
                    if (peeked >= 0)
                        nextIndent = peeked;
                }

                if (nextIndent > indent)
                    flags = SC_FOLDLEVELHEADERFLAG;

                prevLevelNum = levelNum;
            }

            pAccess->SetLevel(startLine + i, levelNum | flags);
        }
    }

    void rebuildFormatMap()
    {
        m_formatToScinStyle.clear();
        m_extendedStyles.clear();
        m_styleSpellCheck.clear();
        m_nextExtendedStyle = kExtendedStyleStart;

        const auto def = definition();
        if (!def.isValid()) {
            m_indentFoldEnabled = false;
            m_indentFoldIgnorePatterns.clear();
            rebuildStylesProperty();
            return;
        }

        const auto formats = def.formats();
        for (const auto &format : formats) {
            if (!format.isValid())
                continue;
            const int formatId = format.id();
            const bool hasOverride = format.hasBoldOverride()
                                   || format.hasItalicOverride()
                                   || format.hasUnderlineOverride()
                                   || format.hasStrikeThroughOverride();
            int scinStyleId;
            if (m_extendedStylesEnabled && hasOverride) {
                scinStyleId = m_nextExtendedStyle++;
                ExtendedStyleEntry entry;
                entry.format = format;
                entry.textStyleId = static_cast<int>(format.textStyle());
                entry.name = format.name().toUtf8();
                m_extendedStyles[scinStyleId] = std::move(entry);
            } else {
                scinStyleId = static_cast<int>(format.textStyle());
            }
            m_formatToScinStyle[formatId] = scinStyleId;

            if (format.spellCheck())
                m_styleSpellCheck[scinStyleId] = true;
            else if (m_styleSpellCheck.find(scinStyleId) == m_styleSpellCheck.end())
                m_styleSpellCheck[scinStyleId] = false;
        }

        m_indentFoldEnabled = def.indentationBasedFoldingEnabled();
        m_indentFoldIgnorePatterns.clear();
        if (m_indentFoldEnabled) {
            for (const auto &pattern : def.foldingIgnoreList())
                m_indentFoldIgnorePatterns.push_back(
                    QRegularExpression(pattern, QRegularExpression::UseUnicodePropertiesOption));
        }

        rebuildStylesProperty();
    }

    void rebuildStylesProperty()
    {
        m_stylesPropertyCache.clear();

        const int baseCount = kshTextStyleCount();

        bool hasBaseSpellCheck = false;
        for (const auto &[styleId, needsCheck] : m_styleSpellCheck) {
            if (styleId < baseCount && needsCheck) {
                hasBaseSpellCheck = true;
                break;
            }
        }

        if (m_extendedStyles.empty() && !hasBaseSpellCheck)
            return;

        KSyntaxHighlighting::Theme t = theme();
        if (!t.isValid())
            t = kshutils::repository().defaultTheme(KSyntaxHighlighting::Repository::LightTheme);
        if (!t.isValid())
            return;

        m_stylesPropertyCache.reserve((m_extendedStyles.size() + baseCount) * 14);
        char flags[6];
        bool first = true;
        for (const auto &[scinStyleId, entry] : m_extendedStyles) {
            if (!first)
                m_stylesPropertyCache.push_back(';');
            first = false;
            const auto scIt = m_styleSpellCheck.find(scinStyleId);
            const bool needsSpellCheck = scIt != m_styleSpellCheck.end() && scIt->second;
            flags[0] = entry.format.isBold(t) ? 'B' : '-';
            flags[1] = entry.format.isItalic(t) ? 'I' : '-';
            flags[2] = entry.format.isUnderline(t) ? 'U' : '-';
            flags[3] = entry.format.isStrikeThrough(t) ? 'S' : '-';
            flags[4] = needsSpellCheck ? 'C' : '-';
            flags[5] = '\0';
            m_stylesPropertyCache += std::to_string(scinStyleId);
            m_stylesPropertyCache.push_back(':');
            m_stylesPropertyCache += std::to_string(entry.textStyleId);
            m_stylesPropertyCache.push_back(':');
            m_stylesPropertyCache += flags;
        }
        for (const auto &[scinStyleId, needsCheck] : m_styleSpellCheck) {
            if (scinStyleId >= baseCount || !needsCheck)
                continue;
            if (!first)
                m_stylesPropertyCache.push_back(';');
            first = false;
            m_stylesPropertyCache += std::to_string(scinStyleId);
            m_stylesPropertyCache.push_back(':');
            m_stylesPropertyCache += std::to_string(scinStyleId);
            m_stylesPropertyCache.push_back(':');
            m_stylesPropertyCache += "----C";
        }
    }

    static std::vector<int> buildOffsetMap(const QString &text, int totalBytes)
    {
        std::vector<int> map;
        map.reserve(static_cast<size_t>(text.size()) + 1);
        int byteOff = 0;
        int qi = 0;
        while (qi < text.size()) {
            map.push_back(byteOff);
            const QChar ch = text[qi];
            if (ch.isHighSurrogate() && qi + 1 < text.size() && text[qi + 1].isLowSurrogate()) {
                byteOff += 4;
                ++qi;
                map.push_back(byteOff - 4);
            } else if (ch.unicode() <= 0x7F) {
                ++byteOff;
            } else if (ch.unicode() <= 0x7FF) {
                byteOff += 2;
            } else {
                byteOff += 3;
            }
            ++qi;
        }
        map.push_back(totalBytes);
        return map;
    }
};

} // namespace

Scintilla::ILexer5 *createKshLexer()
{
    return new KshLexer();
}
