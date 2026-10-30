#pragma once

#include <KXmlGuiWindow>

#include "editorviewmanager.h"
#include "ui_generalsettings.h"
#include "ui_fontsettings.h"
#include "ui_indentationsettings.h"
#include "ui_editingsettings.h"
#include "ui_highlightssettings.h"
#include "ui_searchsettings.h"
#include "ui_autocompletionsettings.h"
#include "ui_marginsettings.h"
#include "ui_backupsettings.h"
#include "ui_filewatchsettings.h"

#include <QHash>
#include <QMetaObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

class QLabel;
class SciEdit;
class TabWidget;
class DocumentManager;
class SessionManager;
class FindReplaceController;
class SearchResultsDock;
class FolderWorkspaceDock;
class FolderWorkspaceManager;
class DocumentMap;
class DocumentListDock;
class QuickSwitcher;
class StatusLabel;
class QDragEnterEvent;
class QDropEvent;
class QMenu;
class Macro;
class MacroRecorder;
class KRecentFilesAction;
class KToolBarPopupAction;
class KHamburgerMenu;
class FileChangeWatcher;
class GoToDialog;
class RunDialog;
class QDialog;
class QTimer;
class Exporter;
class FavoritesStore;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit MainWindow(DocumentManager *documentManager, QWidget *parent = nullptr);
    ~MainWindow();
    void openFiles(const QStringList &paths);
    void newFile();
    bool restorePreviousSession();

private slots:
    void toggleMonitoring(bool checked);
    void openFile();
    void saveFile();
    void saveAsFile();
    void saveAllFiles();
    void closeFile();
    void closeAllFiles();
    void onCurrentEditorChanged(SciEdit *editor);
    void onDocumentModified(int docIndex, bool modified);
    void onContextMenuRequested(SciEdit *editor, int docId);

protected:
    void changeEvent(QEvent *event) override;
    bool queryClose() override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void saveNewToolbarConfig() override;

private:
    void setupActions();
    void addRecentFile(const QString &filePath);
    void showSettings();
    void onColorSchemeChanged(Qt::ColorScheme scheme);
    void showGoToDialog();
    void showRunDialog();
    void showSummaryDialog();
    void applyLexer(const QString & lexer);
    void openLanguageSelector(bool defaultCustomOnly);
    void openUserLanguageEditor();
    void openLanguageManager();
    void rebuildLanguageMenu();
    bool saveDocument(int docId);
    bool saveDocumentAs(int docId);
    bool saveAllDocuments();
    bool closeDocument(int docId, bool createNewIfEmpty = true);
    bool closeAllDocuments();
    QMenu *buildDocumentContextMenu(int docId, SciEdit *editor, QWidget *parent);
    void applySettingsToEditor(SciEdit *editor);
    void applySettingsToAllEditors();
    void applyAppFont();
    void updateWindowTitle();
    void updateStatusBar();
    void updateCursorStatusLabel(SciEdit *editor);
    void updateDocumentStatsLabel(SciEdit *editor);
    void updateActions();
    void updateEditActions();
    void showFindReplaceDialog(bool showReplace);
    bool findNextInCurrentEditor(bool forward, bool showErrorMessage);
    bool replaceOneInCurrentEditor(bool showErrorMessage);
    int replaceAllInCurrentEditor(bool showErrorMessage);
    void ensureSearchResultsDock();
    // Wires navigation and Find-in-Finder signals of a search results dock
    // (the main one and any volatile "Find in these search results" docks).
    void connectSearchResultsDock(SearchResultsDock *dock);
    // "Find in these search results": re-runs a search on the files listed in
    // the emitting dock (sender()) into a new volatile results dock.
    void findInSearchResults();
    void ensureFolderWorkspaceDock();
    void ensureDocumentMapDock();
    void ensureDocumentListDock();
    void ensureQuickSwitcher();
    void openFolderWorkspace();
    bool hasUnsavedOrTempDocuments() const;
    bool saveAllUnsavedOrTempDocuments();
    void saveSession();
    void clearSession();
    void openCurrentDocumentWithEncoding(const QString &codecName, bool withBom, const QString &displayName);
    void convertCurrentDocumentToEncoding(const QString &codecName, bool withBom, const QString &displayName);
    void showCharacterSelector();
    void exportAsRtf();
    void exportAsHtml();
    bool exportCurrentDocument(Exporter *exporter, const QString &nameFilter, const QString &defaultSuffix);
    void copyAsHtml();
    void copyAsRtf();
    void copyAsAllFormats();
    void copySelectionAs(bool includePlain, bool includeHtml, bool includeRtf);
    void loadSavedMacros();
    void saveSavedMacros();
    void updateSavedMacroActions();
    void toggleFavorite();
    void openManageFavoritesDialog();
    void rebuildFavoriteActions();
    void replugActionLists();
    void restoreRecentlyClosedFile();
    void updateBackupTimer();
    void toggleMenuBar(bool visible);

    SciEdit *createEditor(int docId);
    int openFileInternal(const QString &filePath, bool activate = true, bool skipLexerApply = false);
    int newFileInternal(NewTabPlacement placement = NewTabPlacement::AppendToEnd, bool activate = true);
    void recomputeDocumentModified(int docId);

    DocumentManager *m_documentManager;
    TabWidget *m_viewManager;
    SessionManager *m_sessionManager;
    StatusLabel *m_cursorPosStatusLabel;
    StatusLabel *m_docStatsStatusLabel;
    StatusLabel *m_indentStatusLabel;
    QLabel *m_eolStatusLabel;
    QLabel *m_encodingStatusLabel;
    StatusLabel *m_languageStatusLabel;
    StatusLabel *m_overTypeStatusLabel;
    QMetaObject::Connection m_cursorPosConnection;
    QMetaObject::Connection m_textChangedConnection;
    FindReplaceController *m_findReplaceController = nullptr;
    SearchResultsDock *m_searchResultsDock = nullptr;
    FolderWorkspaceDock *m_folderWorkspaceDock = nullptr;
    FolderWorkspaceManager *m_folderWorkspaceManager = nullptr;
    DocumentMap *m_documentMap = nullptr;
    DocumentListDock *m_documentListDock = nullptr;
    QuickSwitcher *m_quickSwitcher = nullptr;
    GoToDialog *m_goToDialog = nullptr;
    RunDialog *m_runDialog = nullptr;
    KRecentFilesAction *m_recentFilesAction = nullptr;
    KHamburgerMenu *m_hamburgerMenu = nullptr;

    FavoritesStore *m_favoritesStore = nullptr;
    KToolBarPopupAction *m_favoriteToggleAction = nullptr;
    QList<QAction *> m_favoriteEntryActions;
    QList<QAction *> m_openWithEncodingListActions;
    QList<QAction *> m_convertToEncodingListActions;

    FileChangeWatcher *m_fileChangeWatcher = nullptr;
    QTimer *m_backupTimer = nullptr;

    std::unique_ptr<Macro> m_currentMacro;
    std::vector<std::unique_ptr<Macro>> m_savedMacros;
    QList<QAction *> m_savedMacroActions;
    MacroRecorder *m_macroRecorder = nullptr;
    QDialog *m_charSelectDialog = nullptr;
    QStringList m_closedTabHistory;
};

// plainly for KConfigDialog
// void setupUi(QWidget *GeneralSettings)
template <typename T>
class SettingsPage : public QWidget
{
public:
    SettingsPage(QWidget *parent)
        : QWidget(parent)
    {
        ui.setupUi(this);
    }

protected:
    T ui;
};
