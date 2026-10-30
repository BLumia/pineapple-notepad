#pragma once

#include <ScintillaEdit.h>
#include <ILexer.h>

#include <QFont>
#include <QList>
#include <QPair>
#include <QStringList>
#include <QString>
#include <array>
#include <functional>
#include <limits>
#include <optional>

class QDragEnterEvent;
class QDropEvent;
class QContextMenuEvent;
class QMouseEvent;
class QPoint;
class QMenu;
class QUrl;
class QTimer;
class SpellCheckController;

class SciEdit : public ScintillaEdit
{
    Q_OBJECT
public:
    explicit SciEdit(QWidget * parent = nullptr);

    // Indicator indices of the built-in decorations that are also needed by
    // helpers outside this class (SpellCheckController, tests).
    static constexpr int kUrlHoverIndicatorId = 20;
    static constexpr int kSpellCheckIndicatorId = 29;

    enum FoldType {
        NoFoldType,
        BoxFoldType,
    };
    Q_ENUM(FoldType)

    enum AutoIndentMode {
        AutoIndentNone,
        AutoIndentBasic,
        AutoIndentAdvanced
    };
    Q_ENUM(AutoIndentMode)

    void setAutoIndentMode(AutoIndentMode mode);
    AutoIndentMode autoIndentMode() const;
    void setIndentUseTabs(bool useTabs);
    bool indentUseTabs() const;

    void setStyleFont(const QFont & font, int style = STYLE_DEFAULT);
    void setFolding(FoldType foldType, int margin = 2);
    void foldCurrentLevel();
    void unfoldCurrentLevel();
    void setLexer(Scintilla::ILexer5 * lexer);
    void setLexer(Scintilla::ILexer5 *lexer, const QString &languageIdOverride, bool colouriseNow = true);
    void setLineNumbersEnabled(bool enabled);
    void setEditorFont(const QFont &font);
    void setThemeId(const QString &themeId);
    void sendColor(unsigned int iMessage, uptr_t wParam, const QColor &col) const;
    void toggleComment();
    void toggleLineComment();
    void toggleStreamComment();
    void trimTrailingWhitespace();
    void trimLeadingWhitespace();
    void trimTrailingAndLeadingWhitespace();
    void convertTabsToSpaces(bool leadingOnly = false, bool documentWide = false);
    void convertSpacesToTabs(bool leadingOnly, bool documentWide = false);
    void convertEolToSpace();
    void removeConsecutiveDuplicateLines();
    void removeAnyDuplicateLines();
    void splitLines();
    void joinLines();
    void moveCurrentLineUp();
    void moveCurrentLineDown();
    void removeEmptyLines(bool includingBlank);
    void insertEmptyLineAbove();
    void insertEmptyLineBelow();
    void reverseLines();
    void randomizeLines();
    void sortLinesAscending();
    void sortLinesDescending();
    void properCase();
    void properCaseBlend();
    void sentenceCase();
    void sentenceCaseBlend();
    void invertCase();
    void randomCase();

    enum class Base64Padding {
        NoPadding,
        WithPadding,
    };
    enum class UrlEncoding {
        Rfc1738,
        Extended,
        Full,
    };

    void base64Encode(Base64Padding padding = Base64Padding::WithPadding, bool byLine = false, int wrapLength = 0);
    bool base64Decode(bool strict = false, bool whitespaceReset = false);
    void urlEncode(UrlEncoding method, bool byLine = false);
    bool urlDecode();
    void hexEncode(bool spaceSeparated = false);
    void hexDecode();
    void toggleBookmark();
    bool goToNextBookmark(bool wrap = true);
    bool goToPreviousBookmark(bool wrap = true);
    void clearAllBookmarks();
    void clearBookmarksInRange(int start, int end);
    void inverseBookmarks();
    void copyBookmarkedLines();
    void cutBookmarkedLines();
    void pasteToBookmarkedLines();
    void removeBookmarkedLines();
    void removeUnbookmarkedLines();

    static constexpr int kMarkStyleCount = 5;
    static QColor markStyleColor(int styleIndex);
    static int markStyleMask(int styleIndex);
    static int allMarkStylesMask();
    int markAllInRange(int flags, const QByteArray &needle, int start, int end,
                       int styleIndex, bool bookmarkLine);
    void clearMarks(int styleIndex, int start = -1, int end = -1);
    void clearAllMarkStyles();
    void styleTokenRange(int start, int length, int styleIndex);
    QList<QPair<int, QString>> markedTexts(int styleMask);
    static QString joinMarkedTexts(const QList<QPair<int, QString>> &texts);
    bool goToNextMarked(int styleIndex, bool wrap = true);
    bool goToPreviousMarked(int styleIndex, bool wrap = true);
    void beginOrEndSelect(bool columnMode);
    bool isBeginEndSelectActive() const;
    bool isBeginEndSelectColumnMode() const;
    void cancelBeginEndSelect();
    void hideSelectedLines();
    void showAllHiddenLines();
    bool hasHiddenLines();
    QPair<int, int> findLast(int flags, const char *needle, int scanStart, int endExclusive);
    void requestUrlScan();
    void beginUrlScanBatch();
    void endUrlScanBatch();
    void setUrlScanEnabled(bool enabled);
    bool urlScanEnabled() const;
    void setViewNPC(bool visible);
    bool viewNPC() const;
    void setVerticalEdgeEnabled(bool enabled);
    bool verticalEdgeEnabled() const;
    void setChangeHistoryEnabled(bool enabled);
    bool changeHistoryEnabled() const;
    void setChangeHistorySavePoint();
    bool goToNextChange(bool wrap = true);
    bool goToPreviousChange(bool wrap = true);
    void clearChangeHistory();
    void setSmartHighlightEnabled(bool enabled);
    bool smartHighlightEnabled() const;
    void setSmartHighlightMatchCase(bool matchCase);
    bool smartHighlightMatchCase() const;
    void setSmartHighlightWholeWord(bool wholeWord);
    bool smartHighlightWholeWord() const;
    void setFindResultHighlightEnabled(bool enabled);
    bool findResultHighlightEnabled() const;
    void highlightFindResults(const QByteArray &text, int flags);
    void clearFindResultHighlight();
    void setSkipNextSmartHighlight(bool skip);

    void setTagMatchHighlightEnabled(bool enabled);
    bool tagMatchHighlightEnabled() const;
    void setTagMatchHighlightAttributes(bool attributes);
    bool tagMatchHighlightAttributes() const;

    void setAutoCompletionEnabled(bool enabled);
    bool autoCompletionEnabled() const;
    void setAutoCompletionTriggerLength(int length);
    int autoCompletionTriggerLength() const;
    void setAutoCompletionIgnoreNumbers(bool ignore);
    bool autoCompletionIgnoreNumbers() const;
    void setAutoCompletionBrief(bool brief);
    bool autoCompletionBrief() const;
    void setColumnSel2MultiEdit(bool enabled);
    bool columnSel2MultiEdit() const;
    void setMultiSelection(bool enabled);
    void setColumnSelectMode(bool enabled);
    bool columnSelectMode() const;
    void setIsLargeFile(bool isLarge);
    bool isLargeFile() const;

    void setSpellCheckEnabled(bool enabled);
    bool spellCheckEnabled() const;
    // Rebuild the Sonnet speller (dictionary/backend/language settings may
    // have changed) and re-scan.
    void refreshSpellChecker();
    // Whether words using the given style byte should be spell checked.
    // Populated from the KSH lexer serialization flag, the static language
    // style tables, or plain-text mode (no lexer set).
    bool styleNeedsSpellCheck(int styleId) const;
    void beginSpellCheckBatch();
    void endSpellCheckBatch();

    void startRecord();
    void stopRecord();
    bool isMacroRecording() const;

    void setMacroPlayback(bool playback);
    bool isMacroPlayingBack() const;

private:
    void autoIndentLine(int line);
    void autoIndentLineAdvanced(int line, wchar_t ch);
    bool isConditionExprLine(int line);
    sptr_t findMatchingBracePos(sptr_t pos, char openBrace, char closeBrace) const;
    void insertIndentation(int line, int indentAmount);
    QByteArray eolString() const;
    bool getSortLineRange(sptr_t &fromLine, sptr_t &toLine);
    void reorderLines(const std::function<void(QStringList&)> &reorderFunc);
    bool transformSelections(const std::function<std::optional<QByteArray>(const QByteArray &)> &transform);

    AutoIndentMode m_autoIndentMode = AutoIndentBasic;
    bool m_indentUseTabs = false;
    bool m_viewNPC = false;

signals:
    void cursorPosChanged(int line, int index);
    void textChanged();
    void filesDropped(const QStringList &paths);
    void bookmarkMarginContextMenuRequested(const QPoint &globalPos);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void setMarkerDefine(int markerNumber, int markerSymbol);
    static int markStyleIndicator(int styleIndex);
    void removeLinesByBookmark(bool marked);
    QString bookmarkedLinesText();
    void updateLineNumberMarginWidth();
    void applyTheme();
    void applyLexerStyles();
    void applyLexerKeywords();
    void applyLexerProperties();
    void applyKshExtendedStyles(const QString &baseThemeId,
                                const std::function<QColor(const QString &)> &roleColor);
    void clearLexerKeywords();
    void scanVisibleUrls();
    void clearUrlIndicators();
    void openUrlAtIndicatorPosition(sptr_t pos, Scintilla::KeyMod modifiers);
    void scheduleUrlScanIfTriggered();
    void highlightSelectedWord();
    void clearSmartHighlight();
    void highlightBraceMatch();
    void highlightTagMatch();
    void clearTagMatchHighlight();
    bool isXmlTagLexer() const;
    void showAutoCompletion();
    bool shouldShowAutoCompletion() const;
    void showHiddenSectionFrom(sptr_t beginLine);

    void updateBeginSelectPosition(bool isInsert, sptr_t position, sptr_t length);
    void addSpellCheckMenuSection(QMenu *menu, const QString &word, sptr_t start, sptr_t end);

    bool m_lineNumbersEnabled = false;
    int m_lastLineNumberDigits = 0;
    int m_lastLineNumberZoom = std::numeric_limits<int>::min();
    int m_lastLineNumberMarginWidth = -1;
    QString m_themeId = QStringLiteral("Pineapple Notepad Light");
    QFont m_editorFont;
    QString m_languageId;
    QTimer *m_urlScanTimer = nullptr;
    bool m_urlScanEnabled = true;
    int m_urlScanSuspendCount = 0;
    bool m_urlScanTriggered = false;
    bool m_urlScanForceOnResume = false;
    bool m_changeHistoryEnabled = false;
    bool m_smartHighlightEnabled = true;
    bool m_smartHighlightMatchCase = true;
    bool m_smartHighlightWholeWord = true;
    bool m_findResultHighlightEnabled = true;
    bool m_findResultHighlightActive = false;
    bool m_tagMatchHighlightEnabled = true;
    bool m_tagMatchHighlightAttributes = true;
    bool m_macroRecording = false;
    bool m_macroPlayback = false;
    bool m_autoCompletionEnabled = true;
    int m_autoCompletionTriggerLength = 1;
    bool m_autoCompletionIgnoreNumbers = true;
    bool m_autoCompletionBrief = true;
    bool m_isLargeFile = false;
    bool m_columnSel2MultiEdit = true;
    bool m_columnSelectMode = false;
    sptr_t m_beginSelectPosition = -1;
    bool m_beginSelectColumnMode = false;
    // Per-style-byte lookup table telling the SpellCheckController which
    // styles participate in spell checking.
    std::array<bool, 256> m_styleNeedsSpellCheck{};
    SpellCheckController *m_spellCheckController = nullptr;
};
