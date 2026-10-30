#include "mainwindow.h"

#include "sciedit.h"
#include "tabwidget.h"
#include "documentmanager.h"
#include "sessionmanager.h"
#include "appsettings.h"
#include "favoritesstore.h"
#include "favoritesmanagedialog.h"
#include "editorpopupconfigdialog.h"
#include "editorviewhelper.h"
#include "findreplacecontroller.h"
#include "findinfinderdialog.h"
#include "searchresultsdock.h"
#include "folderworkspacedock.h"
#include "folderworkspacemanager.h"
#include "documentmap.h"
#include "documentlistdock.h"
#include "quickswitcher.h"
#include "filechangewatcher.h"
#include "gotodialog.h"
#include "rundialog.h"
#include "macro.h"
#include "macrorecorder.h"
#include "macromanagedialog.h"
#include "macrorundialog.h"
#include "summarydialog.h"
#include "statuslabel.h"
#include "settingsdialog.h"
#include "editorthemepage.h"
#include "themeconfigstore.h"
#include "apppaths.h"
#include "sponsordialog.h"
#include "languageselectordialog.h"
#include "languagemanagerdialog.h"
#include "userlanguageeditordialog.h"
#include "languageconfigstore.h"
#include "exporters.h"

#include <optional>

#include <Sonnet/ConfigWidget>

#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDesktopServices>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QPixmap>
#include <QStyleHints>
#include <QStringBuilder>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QClipboard>
#include <QFileDialog>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QStringConverter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMessageBox>
#include <QInputDialog>
#include <QUrl>
#include <QLoggingCategory>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QSet>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QDialogButtonBox>
#include <QTimer>

#include <KActionCollection>
#include <KStandardAction>
#include <KStandardShortcut>
#include <KActionMenu>
#include <KConfigDialog>
#include <KXMLGUIFactory>
#include <KColorSchemeManager>
#include <KColorSchemeMenu>
#include <KRecentFilesAction>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KStyleManager>
#include <KCharsets>
#include <QSpinBox>
#include <KCharSelect>
#include <KToolBarPopupAction>
#include <KHamburgerMenu>

#include <Scintilla.h>

Q_LOGGING_CATEGORY(lcMainWindow, "pnotepad.mainwindow")

using namespace Qt::Literals::StringLiterals;

// Returns the text to be styled by "Style All/One Occurrences of Token":
// the current selection, or the word at the caret when nothing is selected.
static QByteArray markTokenForEditor(SciEdit *editor)
{
    const sptr_t selStart = editor->selectionStart();
    const sptr_t selEnd = editor->selectionEnd();
    if (selStart != selEnd) {
        return editor->get_text_range(int(qMin(selStart, selEnd)), int(qMax(selStart, selEnd)));
    }
    const int pos = int(editor->currentPos());
    const int wordStart = int(editor->wordStartPosition(pos, true));
    const int wordEnd = int(editor->wordEndPosition(pos, true));
    if (wordStart < wordEnd) {
        return editor->get_text_range(wordStart, wordEnd);
    }
    return {};
}

MainWindow::MainWindow(DocumentManager *documentManager, QWidget *parent)
    : KXmlGuiWindow(parent)
    , m_documentManager(documentManager)
    , m_viewManager(new TabWidget(m_documentManager, this))
    , m_sessionManager(new SessionManager(m_documentManager, this))
    , m_cursorPosStatusLabel(new StatusLabel(190, QStringLiteral("Ln: ?  Col: ?  Pos: ?"), this))
    , m_docStatsStatusLabel(new StatusLabel(180, QStringLiteral("Len: ?  Lines: ?"), this))
    , m_indentStatusLabel(new StatusLabel(80, QStringLiteral("Spaces: 4"), this))
    , m_eolStatusLabel(new StatusLabel(100, QStringLiteral("EOL: ?"), this))
    , m_encodingStatusLabel(new StatusLabel(110, QStringLiteral("UTF-8"), this))
    , m_languageStatusLabel(new StatusLabel(200, QStringLiteral("Plain Text"), this))
    , m_overTypeStatusLabel(new StatusLabel(30, QStringLiteral("INS"), this))
    , m_macroRecorder(new MacroRecorder(this))
    , m_findReplaceController(new FindReplaceController([this]() -> SciEdit * { return m_viewManager->currentEditor(); }, this))
{
    setAcceptDrops(true);
    setCentralWidget(m_viewManager);

    m_findReplaceController->setDocumentManager(m_documentManager);
    m_findReplaceController->setEditorViewManager(m_viewManager);
    m_findReplaceController->setEnsureDockCallback([this]() { ensureSearchResultsDock(); });

    // Set Statusbar
    QStatusBar *sb = statusBar();
    sb->addWidget(m_languageStatusLabel, 1);
    sb->addPermanentWidget(m_docStatsStatusLabel, 0);
    sb->addPermanentWidget(m_cursorPosStatusLabel, 0);
    sb->addPermanentWidget(m_indentStatusLabel, 0);
    sb->addPermanentWidget(m_eolStatusLabel, 0);
    sb->addPermanentWidget(m_encodingStatusLabel, 0);
    sb->addPermanentWidget(m_overTypeStatusLabel, 0);

    m_overTypeStatusLabel->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    connect(m_overTypeStatusLabel, &StatusLabel::clicked, this, [this](Qt::MouseButton button) {
        if (button != Qt::LeftButton) {
            return;
        }
        SciEdit *editor = m_viewManager->currentEditor();
        if (!editor) {
            return;
        }
        editor->setOvertype(!editor->overtype());
        m_overTypeStatusLabel->setText(editor->overtype() ? QStringLiteral("OVR") : QStringLiteral("INS"));
    });
    connect(m_cursorPosStatusLabel, &StatusLabel::doubleClicked, this, [this]() {
        showGoToDialog();
    });
    connect(m_docStatsStatusLabel, &StatusLabel::doubleClicked, this, [this]() {
        showSummaryDialog();
    });
    connect(m_languageStatusLabel, &StatusLabel::doubleClicked, this, [this]() {
        openLanguageSelector(false);
    });

    connect(m_indentStatusLabel, &StatusLabel::clicked, this, [this](Qt::MouseButton button) {
        if (button != Qt::LeftButton) {
            return;
        }
        const int docId = m_viewManager->currentDocumentId();
        if (docId < 0) {
            return;
        }
        const bool newUseTabs = !m_documentManager->getDocumentIndentUseTabs(docId);
        m_documentManager->setDocumentIndentUseTabs(docId, newUseTabs);
        if (SciEdit *editor = m_viewManager->currentEditor()) {
            editor->setIndentUseTabs(newUseTabs);
        }
        updateStatusBar();
    });

    m_indentStatusLabel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_indentStatusLabel, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const int docId = m_viewManager->currentDocumentId();
        if (docId < 0) {
            return;
        }

        auto *menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        auto *spaceAction = new QAction(tr("Indent Using Spaces"), menu);
        auto *tabAction = new QAction(tr("Indent Using Tabs"), menu);
        spaceAction->setCheckable(true);
        tabAction->setCheckable(true);
        auto *group = new QActionGroup(menu);
        group->setExclusive(true);
        group->addAction(spaceAction);
        group->addAction(tabAction);

        const bool useTabs = m_documentManager->getDocumentIndentUseTabs(docId);
        if (useTabs) {
            tabAction->setChecked(true);
        } else {
            spaceAction->setChecked(true);
        }

        const auto applyIndent = [this, docId](bool newUseTabs) {
            const int currentWidth = m_documentManager->getDocumentIndentTabWidth(docId);
            QInputDialog dialog(this);
            dialog.setWindowTitle(tr("Adjust Indent Width"));
            dialog.setLabelText(tr("Indent width:"));
            dialog.setIntRange(1, 16);
            dialog.setIntValue(currentWidth);
            dialog.setInputMode(QInputDialog::IntInput);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }
            const int newWidth = dialog.intValue();
            m_documentManager->setDocumentIndentUseTabs(docId, newUseTabs);
            m_documentManager->setDocumentIndentTabWidth(docId, newWidth);
            if (SciEdit *editor = m_viewManager->currentEditor()) {
                editor->setIndentUseTabs(newUseTabs);
                editor->setTabWidth(newWidth);
            }
            updateStatusBar();
        };

        connect(spaceAction, &QAction::triggered, this, [applyIndent]() {
            applyIndent(false);
        });
        connect(tabAction, &QAction::triggered, this, [applyIndent]() {
            applyIndent(true);
        });

        menu->addAction(spaceAction);
        menu->addAction(tabAction);

        menu->addSeparator();

        auto *spaceToTabConvertAction = new QAction(tr("Convert Indentation to Tabs"), menu);
        auto *tabToSpaceConvertAction = new QAction(tr("Convert Indentation to Spaces"), menu);

        SciEdit *editor = m_viewManager->currentEditor();
        const bool convertEnabled = editor && !editor->readOnly();
        spaceToTabConvertAction->setEnabled(convertEnabled);
        tabToSpaceConvertAction->setEnabled(convertEnabled);

        connect(spaceToTabConvertAction, &QAction::triggered, this, [this]() {
            const int docId = m_viewManager->currentDocumentId();
            if (SciEdit *e = m_viewManager->currentEditor()) {
                e->convertSpacesToTabs(true, true);
                if (docId >= 0) {
                    m_documentManager->setDocumentIndentUseTabs(docId, true);
                    e->setIndentUseTabs(true);
                }
                updateStatusBar();
            }
        });
        connect(tabToSpaceConvertAction, &QAction::triggered, this, [this]() {
            const int docId = m_viewManager->currentDocumentId();
            if (SciEdit *e = m_viewManager->currentEditor()) {
                e->convertTabsToSpaces(true, true);
                if (docId >= 0) {
                    m_documentManager->setDocumentIndentUseTabs(docId, false);
                    e->setIndentUseTabs(false);
                }
                updateStatusBar();
            }
        });

        menu->addAction(spaceToTabConvertAction);
        menu->addAction(tabToSpaceConvertAction);

        menu->popup(m_indentStatusLabel->mapToGlobal(pos));
    });

    m_eolStatusLabel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_eolStatusLabel, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto *menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        QAction *windowsAction = actionCollection()->action(QStringLiteral("eol_convert_to_crlf"));
        QAction *unixAction = actionCollection()->action(QStringLiteral("eol_convert_to_lf"));
        QAction *macAction = actionCollection()->action(QStringLiteral("eol_convert_to_cr"));

        if (windowsAction) {
            menu->addAction(windowsAction);
        }
        if (unixAction) {
            menu->addAction(unixAction);
        }
        if (macAction) {
            menu->addAction(macAction);
        }

        SciEdit *editor = m_viewManager->currentEditor();
        const bool enabled = editor && !editor->readOnly();
        if (windowsAction) {
            windowsAction->setEnabled(enabled);
        }
        if (unixAction) {
            unixAction->setEnabled(enabled);
        }
        if (macAction) {
            macAction->setEnabled(enabled);
        }

        if (editor) {
            const int docId = m_viewManager->currentDocumentId();
            const bool mixed = (docId >= 0) ? m_documentManager->isDocumentEolMixed(docId) : false;
            const int mode = static_cast<int>(editor->eOLMode());
            if (windowsAction) {
                QSignalBlocker blocker(windowsAction);
                windowsAction->setChecked(!mixed && mode == SC_EOL_CRLF);
            }
            if (unixAction) {
                QSignalBlocker blocker(unixAction);
                unixAction->setChecked(!mixed && mode == SC_EOL_LF);
            }
            if (macAction) {
                QSignalBlocker blocker(macAction);
                macAction->setChecked(!mixed && mode == SC_EOL_CR);
            }
        }

        menu->popup(m_eolStatusLabel->mapToGlobal(pos));
    });

    // Tab widget signals
    connect(m_viewManager, &TabWidget::currentEditorChanged, this, &MainWindow::onCurrentEditorChanged);
    connect(m_viewManager, &TabWidget::filesDropped, this, &MainWindow::openFiles);
    connect(m_viewManager, &TabWidget::editorCloseRequested, this, [this](int docId) {
        closeDocument(docId);
    });
    connect(m_viewManager, &TabWidget::contextMenuRequested, this, &MainWindow::onContextMenuRequested);
    connect(m_viewManager, &TabWidget::newDocumentRequested, this, [this]() {
        newFileInternal(NewTabPlacement::AppendToEnd);
        updateActions();
    });
    connect(m_viewManager, &TabWidget::bookmarkMarginContextMenuRequested, this,
            [this](SciEdit *, const QPoint &globalPos) {
        if (QWidget *container = guiFactory()->container(u"bookmark"_s, this)) {
            if (auto *menu = qobject_cast<QMenu *>(container)) {
                menu->popup(globalPos);
            }
        }
    });
    
    // Document Manager signals
    connect(m_documentManager, &DocumentManager::documentModified, this, &MainWindow::onDocumentModified);
    connect(m_documentManager, &DocumentManager::documentSaved, this, [this](int docId, const QString &) {
        m_sessionManager->removeBackupForDoc(docId);
        updateActions(); // e.g. favorites state after "Save As" changes the file path
    });
    connect(m_documentManager, &DocumentManager::documentClosed, this, [this](int docId) {
        m_sessionManager->removeBackupForDoc(docId);
    });
    connect(m_documentManager, &DocumentManager::documentReadOnlyChanged, this, [this](int docId, bool readOnly) {
        for (SciEdit *e : m_viewManager->editorsForDocId(docId))
            e->setReadOnly(readOnly);
        m_viewManager->refreshEditorsForDoc(docId);
    });

    m_fileChangeWatcher = new FileChangeWatcher(m_documentManager, this);
    m_fileChangeWatcher->setDialogParent(this);
    connect(m_fileChangeWatcher, &FileChangeWatcher::documentReloaded, this, [this](int docId) {
        m_viewManager->refreshEditorsForDoc(docId);
        updateWindowTitle();
        updateStatusBar();
        updateActions();
    });
    connect(m_fileChangeWatcher, &FileChangeWatcher::closeDocumentRequested, this, [this](int docId) {
        closeDocument(docId);
    });
    connect(m_fileChangeWatcher, &FileChangeWatcher::scrollToEndRequested, this, [this](int docId, bool wasAtEnd) {
        Document *doc = m_documentManager->getDocument(docId);
        if (doc) {
            if (SciEdit *ed = doc->firstEditor()) {
                if (!AppSettings::monitorAlwaysScrollToEnd() && !wasAtEnd) {
                    return;
                }
                ed->documentEnd();
            }
        }
    });
    connect(m_fileChangeWatcher, &FileChangeWatcher::monitoringChanged, this, [this](int docId, bool) {
        m_viewManager->refreshEditorsForDoc(docId);
        updateActions();
    });

    connect(m_sessionManager, &SessionManager::recentFileOpened, this, &MainWindow::addRecentFile);

    setupActions();

    loadSavedMacros();
    // plugActionList() is a no-op until setupGUI() built the GUI, so defer to
    // the event loop.
    QMetaObject::invokeMethod(this, [this](){
        updateSavedMacroActions();
    }, Qt::QueuedConnection);
    updateActions();
    applyAppFont();

    updateBackupTimer();
}

MainWindow::~MainWindow()
{
}

bool MainWindow::restorePreviousSession()
{
    return m_sessionManager->restoreSession(m_viewManager, this,
        [this](const QString &path) { return openFileInternal(path, false, true); },
        [this]() { return newFileInternal(NewTabPlacement::AppendToEnd, false); });
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        onColorSchemeChanged(QGuiApplication::styleHints()->colorScheme());
    }
    KXmlGuiWindow::changeEvent(event);
}

void MainWindow::showEvent(QShowEvent *event)
{
    KXmlGuiWindow::showEvent(event);
    if (auto *editor = m_viewManager->currentEditor()) {
        editor->QWidget::setFocus(Qt::OtherFocusReason);
    }
}

bool MainWindow::queryClose()
{
    if (m_backupTimer) {
        m_backupTimer->stop();
    }

    if (m_recentFilesAction) {
        qCDebug(lcMainWindow) << "Saving recent files...";
        KConfigGroup group(KSharedConfig::openConfig(), u"RecentFiles"_s);
        m_recentFilesAction->saveEntries(group);
        group.sync();
    }

    if (AppSettings::saveSessionOnClose()) {
        saveSession();
        return true;
    }

    if (!hasUnsavedOrTempDocuments()) {
        clearSession();
        return true;
    }

    const QVector<int> docIds = m_viewManager->documentIds();
    for (int i = docIds.size() - 1; i >= 0; --i) {
        if (!closeDocument(docIds[i], false)) {
            if (m_backupTimer && AppSettings::backupEnabled()) {
                m_backupTimer->start(AppSettings::backupIntervalSeconds() * 1000);
            }
            return false;
        }
    }

    clearSession();
    return true;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event && event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    KXmlGuiWindow::dragEnterEvent(event);
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event || !event->mimeData() || !event->mimeData()->hasUrls()) {
        KXmlGuiWindow::dropEvent(event);
        return;
    }

    QStringList paths;
    const QList<QUrl> urls = event->mimeData()->urls();
    paths.reserve(urls.size());
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString p = url.toLocalFile();
        if (!p.isEmpty()) {
            paths.push_back(p);
        }
    }

    if (!paths.isEmpty()) {
        openFiles(paths);
        event->acceptProposedAction();
        return;
    }

    KXmlGuiWindow::dropEvent(event);
}

void MainWindow::openFiles(const QStringList &paths)
{
    QSet<QString> seen;

    for (const QString &p : paths) {
        if (p.isEmpty()) {
            continue;
        }

        QFileInfo fi(p);
        const QString abs = fi.isAbsolute() ? fi.absoluteFilePath() : QDir::current().absoluteFilePath(p);
        const QString normalized = DocumentManager::normalizeFilePath(p);

        if (seen.contains(normalized)) {
            continue;
        }
        seen.insert(normalized);

        QFileInfo afi(abs);
        if (afi.exists()) {
            if (afi.isFile()) {
                const int docId = openFileInternal(normalized);
                if (docId >= 0) {
                    addRecentFile(normalized);
                }
            } else if (afi.isDir()) {
                ensureFolderWorkspaceDock();
                m_folderWorkspaceDock->addWorkspace(abs);
                m_folderWorkspaceDock->show();
                m_folderWorkspaceDock->raise();
            } else {
                qCDebug(lcMainWindow) << "Ignored a path which is not a file and also not a dir:" << abs;
            }
        } else {
            const int existingDocId = m_documentManager->findDocumentByFilePath(abs);
            if (existingDocId >= 0) {
                m_viewManager->switchToDocId(existingDocId);
            } else {
                const int docId = newFileInternal();
                if (docId >= 0) {
                    m_documentManager->setDocumentFilePath(docId, abs);
                    m_viewManager->refreshEditorsForDoc(docId);
                    recomputeDocumentModified(docId);

                    const QString lexerName = m_documentManager->getDocumentLanguage(docId);
                    if (!lexerName.isEmpty()) {
                        if (SciEdit *editor = m_viewManager->editorForDocId(docId)) {
                            DocumentManager::applyLexerToEditor(editor, lexerName, m_documentManager);
                        }
                    }
                }
            }
        }
    }

    if (m_viewManager->count() <= 1) {
        return;
    }

    const QVector<int> docIds = m_viewManager->documentIds();
    for (int docId : docIds) {
        if (docId < 0) {
            continue;
        }
        if (!m_documentManager->getDocumentFilePath(docId).isEmpty() || m_documentManager->isDocumentModified(docId)) {
            continue;
        }
        SciEdit *editor = m_viewManager->editorForDocId(docId);
        if (!editor) {
            continue;
        }
        if (editor->textLength() != 0) {
            continue;
        }
        closeDocument(docId);
        break;
    }
}

void MainWindow::addRecentFile(const QString &filePath)
{
    if (!m_recentFilesAction) {
        return;
    }

    if (filePath.isEmpty()) {
        return;
    }

    const QFileInfo fi(filePath);
    const QString abs = fi.isAbsolute() ? fi.absoluteFilePath() : QDir::current().absoluteFilePath(filePath);
    const QFileInfo afi(abs);
    if (!afi.exists() || !afi.isFile()) {
        return;
    }

    const QString canonical = afi.canonicalFilePath().isEmpty() ? afi.absoluteFilePath() : afi.canonicalFilePath();
    m_recentFilesAction->addUrl(QUrl::fromLocalFile(canonical));
}

void MainWindow::setupActions()
{
    const auto withEditor = [this](auto &&func) {
        if (SciEdit *editor = m_viewManager->currentEditor()) {
            func(editor);
        }
    };

    // "File" menu
    KStandardAction::openNew(this, &MainWindow::newFile, actionCollection());
    KStandardAction::open(this, &MainWindow::openFile, actionCollection());
    m_recentFilesAction = KStandardAction::openRecent(this, [this](const QUrl &url) {
        if (url.isLocalFile()) {
            openFiles(QStringList{url.toLocalFile()});
        }
    }, actionCollection());
    KStandardAction::save(this, &MainWindow::saveFile, actionCollection());
    KStandardAction::close(this, &MainWindow::closeFile, actionCollection());
    KStandardAction::quit(qApp, &QApplication::quit, actionCollection());
    QAction *saveAsAction = KStandardAction::saveAs(this, &MainWindow::saveAsFile, actionCollection());

    QAction *saveAllAction = actionCollection()->addAction(u"file_save_all"_s);
    saveAllAction->setText(tr("Save All"));
    saveAllAction->setIcon(QIcon::fromTheme(u"document-save-all"_s));
    connect(saveAllAction, &QAction::triggered, this, &MainWindow::saveAllFiles);

    QAction *closeAllAction = actionCollection()->addAction(u"file_close_all"_s);
    closeAllAction->setText(tr("Close All"));
    closeAllAction->setIcon(QIcon(":/icons/icons/document-close-all.svg"));
    connect(closeAllAction, &QAction::triggered, this, &MainWindow::closeAllFiles);

    QAction *restoreClosedAction = actionCollection()->addAction(u"file_restore_recently_closed"_s);
    restoreClosedAction->setText(tr("Restore Recently Closed File"));
    restoreClosedAction->setIcon(QIcon::fromTheme("document-open-recent"));
    actionCollection()->setDefaultShortcut(restoreClosedAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(restoreClosedAction, &QAction::triggered, this, &MainWindow::restoreRecentlyClosedFile);

    QAction *openFolderWorkspaceAction = actionCollection()->addAction(u"file_open_folder_workspace"_s);
    openFolderWorkspaceAction->setText(tr("Open Folder as Workspace..."));
    openFolderWorkspaceAction->setIcon(QIcon::fromTheme(u"document-open-folder"_s));
    connect(openFolderWorkspaceAction, &QAction::triggered, this, &MainWindow::openFolderWorkspace);

    QAction *exportRtfAction = actionCollection()->addAction(u"file_export_rtf"_s);
    exportRtfAction->setText(tr("Export as RTF..."));
    exportRtfAction->setIcon(QIcon::fromTheme(u"text-rtf"_s));
    connect(exportRtfAction, &QAction::triggered, this, &MainWindow::exportAsRtf);

    QAction *exportHtmlAction = actionCollection()->addAction(u"file_export_html"_s);
    exportHtmlAction->setText(tr("Export as HTML..."));
    exportHtmlAction->setIcon(QIcon::fromTheme(u"text-html"_s));
    connect(exportHtmlAction, &QAction::triggered, this, &MainWindow::exportAsHtml);

    // Favorites
    m_favoritesStore = new FavoritesStore(this);
    connect(m_favoritesStore, &FavoritesStore::entriesChanged, this, &MainWindow::rebuildFavoriteActions);

    m_favoriteToggleAction = new KToolBarPopupAction(
        QIcon::fromTheme(u"favorite"_s), tr("Toggle Favorite"), this);
    actionCollection()->addAction(u"file_toggle_favorite"_s, m_favoriteToggleAction);
    connect(m_favoriteToggleAction, &QAction::triggered, this, &MainWindow::toggleFavorite);

    QAction *manageFavoritesAction = actionCollection()->addAction(u"favorites_manage"_s);
    manageFavoritesAction->setText(tr("Manage Favorites..."));
    manageFavoritesAction->setIcon(QIcon::fromTheme(u"configure"_s));
    connect(manageFavoritesAction, &QAction::triggered, this, &MainWindow::openManageFavoritesDialog);

    // plugActionList() is a no-op until setupGUI() built the GUI, so defer to
    // the event loop like the encoding and language menus below.
    QMetaObject::invokeMethod(this, [this](){
        rebuildFavoriteActions();
    }, Qt::QueuedConnection);

    if (m_recentFilesAction) {
        m_recentFilesAction->setMaxItems(AppSettings::recentFilesMaxItems());
        const KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("RecentFiles"));
        m_recentFilesAction->loadEntries(group);
    }

    // "Edit" menu
    KStandardAction::undo(this, [withEditor]() { withEditor([](SciEdit *e) { e->undo(); }); }, actionCollection());
    KStandardAction::redo(this, [withEditor]() { withEditor([](SciEdit *e) { e->redo(); }); }, actionCollection());
    KStandardAction::cut(this, [withEditor]() { withEditor([](SciEdit *e) { e->cut(); }); }, actionCollection());
    KStandardAction::copy(this, [withEditor]() { withEditor([](SciEdit *e) { e->copy(); }); }, actionCollection());
    KStandardAction::paste(this, [withEditor]() { withEditor([](SciEdit *e) { e->paste(); }); }, actionCollection());
    KStandardAction::selectAll(this, [withEditor]() { withEditor([](SciEdit *e) { e->selectAll(); }); }, actionCollection());

    QAction *beginEndSelectAction = actionCollection()->addAction(u"edit_begin_end_select"_s);
    beginEndSelectAction->setText(tr("Begin/End Select"));
    beginEndSelectAction->setIcon(QIcon::fromTheme(u"edit-select-text"_s));
    beginEndSelectAction->setCheckable(true);
    actionCollection()->setDefaultShortcut(beginEndSelectAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    connect(beginEndSelectAction, &QAction::triggered, this, [this, withEditor]() {
        withEditor([](SciEdit *e) { e->beginOrEndSelect(false); e->setFocus(true); });
        updateActions();
    });

    QAction *beginEndSelectColumnModeAction = actionCollection()->addAction(u"edit_begin_end_select_column_mode"_s);
    beginEndSelectColumnModeAction->setText(tr("Begin/End Column Select"));
    beginEndSelectColumnModeAction->setIcon(QIcon::fromTheme(u"edit-select"_s));
    beginEndSelectColumnModeAction->setCheckable(true);
    actionCollection()->setDefaultShortcut(beginEndSelectColumnModeAction, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_B));
    connect(beginEndSelectColumnModeAction, &QAction::triggered, this, [this, withEditor]() {
        withEditor([](SciEdit *e) { e->beginOrEndSelect(true); e->setFocus(true); });
        updateActions();
    });

    QAction *columnSelectModeAction = actionCollection()->addAction(u"edit_column_select_mode"_s);
    columnSelectModeAction->setText(tr("Column Selection Mode"));
    columnSelectModeAction->setIcon(QIcon::fromTheme(u"edit-select"_s));
    columnSelectModeAction->setCheckable(true);
    columnSelectModeAction->setToolTip(tr("When enabled, dragging the mouse in the editor selects a rectangular block (column) of text, just like holding Alt."));
    connect(columnSelectModeAction, &QAction::triggered, this, [this, withEditor](bool checked) {
        withEditor([checked](SciEdit *e) { e->setColumnSelectMode(checked); e->setFocus(true); });
        updateActions();
    });

    QAction *copyCurrentFilePathAction = actionCollection()->addAction(u"copy_current_file_path"_s);
    copyCurrentFilePathAction->setText(tr("Current Full File Path"));
    connect(copyCurrentFilePathAction, &QAction::triggered, this, [this]() {
        const int docId = m_viewManager->currentDocumentId();
        const QString filePath = m_documentManager->getDocumentFilePath(docId);
        if (!filePath.isEmpty()) {
            QApplication::clipboard()->setText(filePath);
        }
    });

    QAction *copyCurrentFileNameAction = actionCollection()->addAction(u"copy_current_file_name"_s);
    copyCurrentFileNameAction->setText(tr("Current File Name"));
    connect(copyCurrentFileNameAction, &QAction::triggered, this, [this]() {
        const int docId = m_viewManager->currentDocumentId();
        const QString filePath = m_documentManager->getDocumentFilePath(docId);
        if (!filePath.isEmpty()) {
            QApplication::clipboard()->setText(QFileInfo(filePath).fileName());
        }
    });

    QAction *copyCurrentDirPathAction = actionCollection()->addAction(u"copy_current_dir_path"_s);
    copyCurrentDirPathAction->setText(tr("Current Directory Path"));
    connect(copyCurrentDirPathAction, &QAction::triggered, this, [this]() {
        const int docId = m_viewManager->currentDocumentId();
        const QString filePath = m_documentManager->getDocumentFilePath(docId);
        if (!filePath.isEmpty()) {
            QApplication::clipboard()->setText(QFileInfo(filePath).absolutePath());
        }
    });

    QAction *copyAllFileNamesAction = actionCollection()->addAction(u"copy_all_file_names"_s);
    copyAllFileNamesAction->setText(tr("All File Names"));
    connect(copyAllFileNamesAction, &QAction::triggered, this, [this]() {
        QStringList names;
        for (int docId : m_viewManager->documentIds()) {
            const QString filePath = m_documentManager->getDocumentFilePath(docId);
            if (!filePath.isEmpty()) {
                names.append(QFileInfo(filePath).fileName());
            }
        }
        if (!names.isEmpty()) {
            QApplication::clipboard()->setText(names.join(u'\n'));
        }
    });

    QAction *copyAllFilePathsAction = actionCollection()->addAction(u"copy_all_file_paths"_s);
    copyAllFilePathsAction->setText(tr("All Full File Paths"));
    connect(copyAllFilePathsAction, &QAction::triggered, this, [this]() {
        QStringList paths;
        for (int docId : m_viewManager->documentIds()) {
            const QString filePath = m_documentManager->getDocumentFilePath(docId);
            if (!filePath.isEmpty()) {
                paths.append(filePath);
            }
        }
        if (!paths.isEmpty()) {
            QApplication::clipboard()->setText(paths.join(u'\n'));
        }
    });

    QAction *copyAsHtmlAction = actionCollection()->addAction(u"copy_as_html"_s);
    copyAsHtmlAction->setText(tr("Copy as HTML"));
    connect(copyAsHtmlAction, &QAction::triggered, this, &MainWindow::copyAsHtml);

    QAction *copyAsRtfAction = actionCollection()->addAction(u"copy_as_rtf"_s);
    copyAsRtfAction->setText(tr("Copy as RTF"));
    connect(copyAsRtfAction, &QAction::triggered, this, &MainWindow::copyAsRtf);

    QAction *copyAllFormatsAction = actionCollection()->addAction(u"copy_all_formats"_s);
    copyAllFormatsAction->setText(tr("Copy All Formats"));
    connect(copyAllFormatsAction, &QAction::triggered, this, &MainWindow::copyAsAllFormats);

    QAction *characterSelectorAction = actionCollection()->addAction(u"character_selector"_s);
    characterSelectorAction->setText(tr("Character Selector"));
    characterSelectorAction->setIcon(QIcon::fromTheme("accessories-character-map"));
    connect(characterSelectorAction, &QAction::triggered, this, &MainWindow::showCharacterSelector);
    QAction *deleteAction = actionCollection()->addAction(u"edit_delete"_s);
    deleteAction->setText(tr("Delete"));
    deleteAction->setIcon(QIcon::fromTheme(u"edit-clear"_s));
    actionCollection()->setDefaultShortcut(deleteAction, QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->clear(); });
    });

    QAction *trimTrailingSpaceAction = actionCollection()->addAction(u"trim_trailing_space"_s);
    trimTrailingSpaceAction->setText(tr("Trim Trailing Space"));
    connect(trimTrailingSpaceAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->trimTrailingWhitespace(); });
    });

    QAction *trimLeadingSpaceAction = actionCollection()->addAction(u"trim_leading_space"_s);
    trimLeadingSpaceAction->setText(tr("Trim Leading Space"));
    connect(trimLeadingSpaceAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->trimLeadingWhitespace(); });
    });

    QAction *trimTrailingLeadingSpaceAction = actionCollection()->addAction(u"trim_trailing_leading_space"_s);
    trimTrailingLeadingSpaceAction->setText(tr("Trim Trailing and Leading Space"));
    connect(trimTrailingLeadingSpaceAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->trimTrailingAndLeadingWhitespace(); });
    });

    QAction *tabToSpaceAction = actionCollection()->addAction(u"tab_to_space_all"_s);
    tabToSpaceAction->setText(tr("TAB to Space (All)"));
    connect(tabToSpaceAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->convertTabsToSpaces(); });
    });

    QAction *tabToSpaceLeadingAction = actionCollection()->addAction(u"tab_to_space_leading"_s);
    tabToSpaceLeadingAction->setText(tr("TAB to Space (Leading)"));
    connect(tabToSpaceLeadingAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->convertTabsToSpaces(true); });
    });

    QAction *spaceToTabAllAction = actionCollection()->addAction(u"space_to_tab_all"_s);
    spaceToTabAllAction->setText(tr("Space to TAB (All)"));
    connect(spaceToTabAllAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->convertSpacesToTabs(false); });
    });

    QAction *spaceToTabLeadingAction = actionCollection()->addAction(u"space_to_tab_leading"_s);
    spaceToTabLeadingAction->setText(tr("Space to TAB (Leading)"));
    connect(spaceToTabLeadingAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->convertSpacesToTabs(true); });
    });

    QAction *eolToSpaceAction = actionCollection()->addAction(u"eol_to_space"_s);
    eolToSpaceAction->setText(tr("EOL to Space"));
    connect(eolToSpaceAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->convertEolToSpace(); });
    });

    QAction *duplicateLineAction = actionCollection()->addAction(u"duplicate_current_line"_s);
    duplicateLineAction->setText(tr("Duplicate Current Line"));
    connect(duplicateLineAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->lineDuplicate(); });
    });

    QAction *removeConsecutiveDupLinesAction = actionCollection()->addAction(u"remove_consecutive_dup_lines"_s);
    removeConsecutiveDupLinesAction->setText(tr("Remove Consecutive Duplicate Lines"));
    connect(removeConsecutiveDupLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->removeConsecutiveDuplicateLines(); });
    });

    QAction *removeAnyDupLinesAction = actionCollection()->addAction(u"remove_any_dup_lines"_s);
    removeAnyDupLinesAction->setText(tr("Remove Duplicate Lines"));
    connect(removeAnyDupLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->removeAnyDuplicateLines(); });
    });

    QAction *splitLinesAction = actionCollection()->addAction(u"split_lines"_s);
    splitLinesAction->setText(tr("Split Lines"));
    connect(splitLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->splitLines(); });
    });

    QAction *joinLinesAction = actionCollection()->addAction(u"join_lines"_s);
    joinLinesAction->setText(tr("Join Lines"));
    connect(joinLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->joinLines(); });
    });

    QAction *moveLineUpAction = actionCollection()->addAction(u"move_line_up"_s);
    moveLineUpAction->setText(tr("Move Up Current Line"));
    connect(moveLineUpAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->moveCurrentLineUp(); });
    });

    QAction *moveLineDownAction = actionCollection()->addAction(u"move_line_down"_s);
    moveLineDownAction->setText(tr("Move Down Current Line"));
    connect(moveLineDownAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->moveCurrentLineDown(); });
    });

    QAction *removeEmptyLinesAction = actionCollection()->addAction(u"remove_empty_lines"_s);
    removeEmptyLinesAction->setText(tr("Remove Empty Lines"));
    connect(removeEmptyLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->removeEmptyLines(false); });
    });

    QAction *removeEmptyLinesWithBlankAction = actionCollection()->addAction(u"remove_empty_lines_with_blank"_s);
    removeEmptyLinesWithBlankAction->setText(tr("Remove Empty Lines (Including Blank characters)"));
    connect(removeEmptyLinesWithBlankAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->removeEmptyLines(true); });
    });

    QAction *insertEmptyLineAboveAction = actionCollection()->addAction(u"insert_empty_line_above"_s);
    insertEmptyLineAboveAction->setText(tr("Insert Empty Line Above"));
    connect(insertEmptyLineAboveAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->insertEmptyLineAbove(); });
    });

    QAction *insertEmptyLineBelowAction = actionCollection()->addAction(u"insert_empty_line_below"_s);
    insertEmptyLineBelowAction->setText(tr("Insert Empty Line Below"));
    connect(insertEmptyLineBelowAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->insertEmptyLineBelow(); });
    });

    QAction *reverseLinesAction = actionCollection()->addAction(u"reverse_lines"_s);
    reverseLinesAction->setText(tr("Reverse Line Order"));
    connect(reverseLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->reverseLines(); });
    });

    QAction *randomizeLinesAction = actionCollection()->addAction(u"randomize_lines"_s);
    randomizeLinesAction->setText(tr("Randomize Line Order"));
    connect(randomizeLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->randomizeLines(); });
    });

    QAction *sortLinesAscAction = actionCollection()->addAction(u"sort_lines_asc"_s);
    sortLinesAscAction->setText(tr("Sort Lines in Ascending Order"));
    connect(sortLinesAscAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->sortLinesAscending(); });
    });

    QAction *sortLinesDescAction = actionCollection()->addAction(u"sort_lines_desc"_s);
    sortLinesDescAction->setText(tr("Sort Lines in Descending Order"));
    connect(sortLinesDescAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->sortLinesDescending(); });
    });

    QAction *increaseIndentAction = actionCollection()->addAction(u"increase_indent"_s);
    increaseIndentAction->setText(tr("Increase Indent"));
    increaseIndentAction->setIcon(QIcon::fromTheme(u"format-indent-more"_s));
    connect(increaseIndentAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->tab(); });
    });

    QAction *decreaseIndentAction = actionCollection()->addAction(u"decrease_indent"_s);
    decreaseIndentAction->setText(tr("Decrease Indent"));
    decreaseIndentAction->setIcon(QIcon::fromTheme(u"format-indent-less"_s));
    connect(decreaseIndentAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->backTab(); });
    });

    const auto convertCurrentEditorEol = [this](int eolMode) {
        SciEdit *editor = m_viewManager->currentEditor();
        if (!editor || editor->readOnly()) {
            return;
        }

        editor->beginUndoAction();
        editor->convertEOLs(eolMode);
        editor->setEOLMode(eolMode);
        editor->endUndoAction();

        const int docId = m_viewManager->currentDocumentId();
        if (docId >= 0) {
            m_documentManager->setDocumentEolMode(docId, eolMode, false);
        }
        updateStatusBar();
    };

    QAction *convertEolToCrlfAction = actionCollection()->addAction(u"eol_convert_to_crlf"_s);
    convertEolToCrlfAction->setText(tr("Windows (CR LF)"));
    convertEolToCrlfAction->setCheckable(true);
    connect(convertEolToCrlfAction, &QAction::triggered, this, [convertCurrentEditorEol]() {
        convertCurrentEditorEol(SC_EOL_CRLF);
    });

    QAction *convertEolToLfAction = actionCollection()->addAction(u"eol_convert_to_lf"_s);
    convertEolToLfAction->setText(tr("Unix (LF)"));
    convertEolToLfAction->setCheckable(true);
    connect(convertEolToLfAction, &QAction::triggered, this, [convertCurrentEditorEol]() {
        convertCurrentEditorEol(SC_EOL_LF);
    });

    QAction *convertEolToCrAction = actionCollection()->addAction(u"eol_convert_to_cr"_s);
    convertEolToCrAction->setText(tr("Macintosh (CR)"));
    convertEolToCrAction->setCheckable(true);
    connect(convertEolToCrAction, &QAction::triggered, this, [convertCurrentEditorEol]() {
        convertCurrentEditorEol(SC_EOL_CR);
    });

    auto *eolActionGroup = new QActionGroup(this);
    eolActionGroup->setExclusive(true);
    convertEolToCrlfAction->setActionGroup(eolActionGroup);
    convertEolToLfAction->setActionGroup(eolActionGroup);
    convertEolToCrAction->setActionGroup(eolActionGroup);

    QAction *convertToUpperCaseAction = actionCollection()->addAction(u"convert_to_upper_case"_s);
    convertToUpperCaseAction->setText(tr("UPPERCASE"));
    convertToUpperCaseAction->setIcon(QIcon::fromTheme("format-text-uppercase"));
    actionCollection()->setDefaultShortcut(convertToUpperCaseAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U));
    connect(convertToUpperCaseAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->upperCase(); });
    });

    QAction *convertToLowerCaseAction = actionCollection()->addAction(u"convert_to_lower_case"_s);
    convertToLowerCaseAction->setText(tr("lowercase"));
    convertToLowerCaseAction->setIcon(QIcon::fromTheme("format-text-lowercase"));
    actionCollection()->setDefaultShortcut(convertToLowerCaseAction, QKeySequence(Qt::CTRL | Qt::Key_U));
    connect(convertToLowerCaseAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->lowerCase(); });
    });

    QAction *convertToProperCaseAction = actionCollection()->addAction(u"convert_to_proper_case"_s);
    convertToProperCaseAction->setText(tr("Proper Case"));
    actionCollection()->setDefaultShortcut(convertToProperCaseAction, QKeySequence(Qt::ALT | Qt::Key_U));
    connect(convertToProperCaseAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->properCase(); });
    });

    QAction *convertToProperCaseBlendAction = actionCollection()->addAction(u"convert_to_proper_case_blend"_s);
    convertToProperCaseBlendAction->setText(tr("Proper Case (blend)"));
    actionCollection()->setDefaultShortcut(convertToProperCaseBlendAction, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_U));
    connect(convertToProperCaseBlendAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->properCaseBlend(); });
    });

    QAction *convertToSentenceCaseAction = actionCollection()->addAction(u"convert_to_sentence_case"_s);
    convertToSentenceCaseAction->setText(tr("Sentence case"));
    actionCollection()->setDefaultShortcut(convertToSentenceCaseAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_U));
    connect(convertToSentenceCaseAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->sentenceCase(); });
    });

    QAction *convertToSentenceCaseBlendAction = actionCollection()->addAction(u"convert_to_sentence_case_blend"_s);
    convertToSentenceCaseBlendAction->setText(tr("Sentence case (blend)"));
    actionCollection()->setDefaultShortcut(convertToSentenceCaseBlendAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_U));
    connect(convertToSentenceCaseBlendAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->sentenceCaseBlend(); });
    });

    QAction *convertToInvertCaseAction = actionCollection()->addAction(u"convert_to_invert_case"_s);
    convertToInvertCaseAction->setText(tr("iNVERT cASE"));
    connect(convertToInvertCaseAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->invertCase(); });
    });

    QAction *convertToRandomCaseAction = actionCollection()->addAction(u"convert_to_random_case"_s);
    convertToRandomCaseAction->setText(tr("rAnDOm CasE"));
    connect(convertToRandomCaseAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->randomCase(); });
    });

    QAction *toggleCommentAction = actionCollection()->addAction(u"toggle_comment"_s);
    toggleCommentAction->setText(tr("Toggle Comment"));
    toggleCommentAction->setIcon(QIcon::fromTheme(u"comment"_s));
    actionCollection()->setDefaultShortcut(toggleCommentAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Slash));
    connect(toggleCommentAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->toggleComment(); });
    });

    QAction *toggleLineCommentAction = actionCollection()->addAction(u"toggle_line_comment"_s);
    toggleLineCommentAction->setText(tr("Toggle Line Comment"));
    toggleLineCommentAction->setIcon(QIcon::fromTheme(u"comment"_s));
    actionCollection()->setDefaultShortcut(toggleLineCommentAction, QKeySequence(Qt::CTRL | Qt::Key_Slash));
    connect(toggleLineCommentAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->toggleLineComment(); });
    });

    QAction *toggleStreamCommentAction = actionCollection()->addAction(u"toggle_stream_comment"_s);
    toggleStreamCommentAction->setText(tr("Toggle Block Comment"));
    toggleStreamCommentAction->setIcon(QIcon::fromTheme(u"comment"_s));
    actionCollection()->setDefaultShortcut(toggleStreamCommentAction, QKeySequence(Qt::SHIFT | Qt::ALT | Qt::Key_A));
    connect(toggleStreamCommentAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->toggleStreamComment(); });
    });

    QAction *columnModeHint = actionCollection()->addAction(u"column_mode_hint"_s);
    columnModeHint->setText(tr("Column Mode..."));
    connect(columnModeHint, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, tr("Column Mode Tip"), tr("There are multiple ways to switch to column-select mode:") % '\n' % '\n' %
            tr("1. (Keyboard and Mouse)  Hold Alt while left-click dragging") % '\n' %
            tr("2. (Keyboard only)  Hold Alt+Shift while using arrow keys") % '\n' %
            tr("3. (Keyboard or Mouse)  Put caret at desired start of column block position, then"
                "trigger \"Begin/End Column Select\" action;"
                "Move caret to desired end of column block position, then"
                "trigger \"Begin/End Column Select\" action again") % '\n' %
            tr("4. (Mouse only)  Toggle \"Column Selection Mode\" action and preform left-click dragging"));
    });

    QAction *toggleReadOnlyAction = actionCollection()->addAction(u"toggle_read_only"_s);
    toggleReadOnlyAction->setText(tr("Set Read-Only"));
    toggleReadOnlyAction->setCheckable(true);
    toggleReadOnlyAction->setIcon(QIcon::fromTheme(u"object-locked"_s, QIcon::fromTheme(u"lock"_s)));
    connect(toggleReadOnlyAction, &QAction::triggered, this, [this](bool readOnly) {
        const int docId = m_viewManager->currentDocumentId();
        if (docId >= 0) {
            m_documentManager->setDocumentReadOnly(docId, readOnly);
        }
        updateActions();
    });

    // "Search" menu
    KStandardAction::find(this, [this](){
        m_findReplaceController->showDialog(false);
    }, actionCollection());
    KStandardAction::findNext(this, [this](){
        m_findReplaceController->findNext(true, true);
    }, actionCollection());
    KStandardAction::findPrev(this, [this](){
        m_findReplaceController->findNext(false, true);
    }, actionCollection());
    KStandardAction::replace(this, [this](){
        m_findReplaceController->showDialog(true);
    }, actionCollection());

    QAction *findInFilesAction = actionCollection()->addAction(u"find_in_files"_s);
    findInFilesAction->setText(tr("Find in Files..."));
    findInFilesAction->setIcon(QIcon::fromTheme(u"edit-find-in-file"_s, QIcon::fromTheme(u"edit-find"_s)));
    actionCollection()->setDefaultShortcut(findInFilesAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    connect(findInFilesAction, &QAction::triggered, this, [this]() {
        m_findReplaceController->showFindInFilesDialog();
    });

    QAction *quickFindNextAction = actionCollection()->addAction(u"quick_find_next"_s);
    quickFindNextAction->setText(tr("Quick Find Next"));
    quickFindNextAction->setIcon(QIcon::fromTheme(u"arrow-down-double"_s));
    actionCollection()->setDefaultShortcut(quickFindNextAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F3));
    connect(quickFindNextAction, &QAction::triggered, this, [this]() {
        m_findReplaceController->quickFindNext(true);
    });

    QAction *quickFindPrevAction = actionCollection()->addAction(u"quick_find_previous"_s);
    quickFindPrevAction->setText(tr("Quick Find Previous"));
    quickFindPrevAction->setIcon(QIcon::fromTheme(u"arrow-up-double"_s));
    actionCollection()->setDefaultShortcut(quickFindPrevAction, QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_F3));
    connect(quickFindPrevAction, &QAction::triggered, this, [this]() {
        m_findReplaceController->quickFindNext(false);
    });

    QAction *toggleBookmarkAction = actionCollection()->addAction(u"bookmark_toggle"_s);
    toggleBookmarkAction->setText(tr("Toggle Bookmark"));
    toggleBookmarkAction->setIcon(QIcon::fromTheme(u"bookmark-new"_s, QIcon::fromTheme(u"bookmark"_s)));
    actionCollection()->setDefaultShortcut(toggleBookmarkAction, QKeySequence(Qt::CTRL | Qt::Key_F2));
    connect(toggleBookmarkAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->toggleBookmark(); e->setFocus(true); });
    });

    QAction *nextBookmarkAction = actionCollection()->addAction(u"bookmark_next"_s);
    nextBookmarkAction->setText(tr("Next Bookmark"));
    nextBookmarkAction->setIcon(QIcon::fromTheme(u"go-next"_s));
    actionCollection()->setDefaultShortcut(nextBookmarkAction, QKeySequence(Qt::Key_F2));
    connect(nextBookmarkAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->goToNextBookmark(true); e->setFocus(true); });
    });

    QAction *prevBookmarkAction = actionCollection()->addAction(u"bookmark_previous"_s);
    prevBookmarkAction->setText(tr("Previous Bookmark"));
    prevBookmarkAction->setIcon(QIcon::fromTheme(u"go-previous"_s));
    actionCollection()->setDefaultShortcut(prevBookmarkAction, QKeySequence(Qt::SHIFT | Qt::Key_F2));
    connect(prevBookmarkAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->goToPreviousBookmark(true); e->setFocus(true); });
    });

    QAction *clearAllBookmarksAction = actionCollection()->addAction(u"bookmark_clear_all"_s);
    clearAllBookmarksAction->setText(tr("Clear All Bookmarks"));
    clearAllBookmarksAction->setIcon(QIcon::fromTheme(u"edit-clear"_s));
    actionCollection()->setDefaultShortcut(clearAllBookmarksAction, QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_F2));
    connect(clearAllBookmarksAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->clearAllBookmarks(); e->setFocus(true); });
    });

    QAction *cutBookmarkedLinesAction = actionCollection()->addAction(u"bookmark_cut_lines"_s);
    cutBookmarkedLinesAction->setText(tr("Cut Bookmarked Lines"));
    cutBookmarkedLinesAction->setIcon(QIcon::fromTheme(u"edit-cut"_s));
    connect(cutBookmarkedLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->cutBookmarkedLines(); e->setFocus(true); });
    });

    QAction *copyBookmarkedLinesAction = actionCollection()->addAction(u"bookmark_copy_lines"_s);
    copyBookmarkedLinesAction->setText(tr("Copy Bookmarked Lines"));
    copyBookmarkedLinesAction->setIcon(QIcon::fromTheme(u"edit-copy"_s));
    connect(copyBookmarkedLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->copyBookmarkedLines(); e->setFocus(true); });
    });

    QAction *pasteToBookmarkedLinesAction = actionCollection()->addAction(u"bookmark_paste_to_lines"_s);
    pasteToBookmarkedLinesAction->setText(tr("Paste to (Replace) Bookmarked Lines"));
    pasteToBookmarkedLinesAction->setIcon(QIcon::fromTheme(u"edit-paste"_s));
    connect(pasteToBookmarkedLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->pasteToBookmarkedLines(); e->setFocus(true); });
    });

    QAction *removeBookmarkedLinesAction = actionCollection()->addAction(u"bookmark_remove_lines"_s);
    removeBookmarkedLinesAction->setText(tr("Remove Bookmarked Lines"));
    removeBookmarkedLinesAction->setIcon(QIcon::fromTheme(u"edit-delete"_s, QIcon::fromTheme(u"list-remove"_s)));
    connect(removeBookmarkedLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->removeBookmarkedLines(); e->setFocus(true); });
    });

    QAction *removeUnbookmarkedLinesAction = actionCollection()->addAction(u"bookmark_remove_unmarked_lines"_s);
    removeUnbookmarkedLinesAction->setText(tr("Remove Non-Bookmarked Lines"));
    removeUnbookmarkedLinesAction->setIcon(QIcon::fromTheme(u"edit-delete"_s, QIcon::fromTheme(u"list-remove"_s)));
    connect(removeUnbookmarkedLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->removeUnbookmarkedLines(); e->setFocus(true); });
    });

    QAction *inverseBookmarksAction = actionCollection()->addAction(u"bookmark_inverse"_s);
    inverseBookmarksAction->setText(tr("Inverse Bookmarks"));
    inverseBookmarksAction->setIcon(QIcon::fromTheme(u"bookmark"_s));
    connect(inverseBookmarksAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->inverseBookmarks(); e->setFocus(true); });
    });

    QAction *markAction = actionCollection()->addAction(u"search_mark"_s);
    markAction->setText(tr("Mark..."));
    actionCollection()->setDefaultShortcut(markAction, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(markAction, &QAction::triggered, this, [this]() {
        m_findReplaceController->showMarkDialog();
    });

    static const QStringList markStyleNames = {
        tr("1st Style", "mark style"),
        tr("2nd Style", "mark style"),
        tr("3rd Style", "mark style"),
        tr("4th Style", "mark style"),
        tr("5th Style", "mark style"),
    };
    static const Qt::Key markStyleKeys[SciEdit::kMarkStyleCount] = {
        Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5,
    };

    for (int i = 0; i < SciEdit::kMarkStyleCount; ++i) {
        const QString &styleName = markStyleNames.at(i);
        QPixmap colorPixmap(16, 16);
        colorPixmap.fill(SciEdit::markStyleColor(i));
        const QIcon styleIcon(colorPixmap);

        QAction *markStyleAllAction = actionCollection()->addAction(u"mark_style_all_%1"_s.arg(i + 1));
        markStyleAllAction->setText(tr("Mark All Occurrences with %1").arg(styleName));
        markStyleAllAction->setIcon(styleIcon);
        connect(markStyleAllAction, &QAction::triggered, this, [this, i, withEditor]() {
            withEditor([i](SciEdit *e) {
                const QByteArray token = markTokenForEditor(e);
                if (token.isEmpty()) {
                    return;
                }
                int flags = 0;
                if (AppSettings::markAllCaseSensitive()) {
                    flags |= SCFIND_MATCHCASE;
                }
                if (AppSettings::markAllWordOnly()) {
                    flags |= SCFIND_WHOLEWORD;
                }
                e->markAllInRange(flags, token, 0, int(e->textLength()), i, false);
                e->setFocus(true);
            });
        });

        QAction *markStyleOneAction = actionCollection()->addAction(u"mark_style_one_%1"_s.arg(i + 1));
        markStyleOneAction->setText(tr("Mark One Token with %1").arg(styleName));
        markStyleOneAction->setIcon(styleIcon);
        connect(markStyleOneAction, &QAction::triggered, this, [this, i, withEditor]() {
            withEditor([i](SciEdit *e) {
                const QByteArray token = markTokenForEditor(e);
                if (token.isEmpty()) {
                    return;
                }
                const sptr_t selStart = e->selectionStart();
                const sptr_t selEnd = e->selectionEnd();
                if (selStart != selEnd) {
                    e->styleTokenRange(int(qMin(selStart, selEnd)), int(qMax(selStart, selEnd) - qMin(selStart, selEnd)), i);
                } else {
                    const int pos = int(e->currentPos());
                    const int wordStart = int(e->wordStartPosition(pos, true));
                    const int wordEnd = int(e->wordEndPosition(pos, true));
                    if (wordStart < wordEnd) {
                        e->styleTokenRange(wordStart, wordEnd - wordStart, i);
                    }
                }
                e->setFocus(true);
            });
        });

        QAction *markClearStyleAction = actionCollection()->addAction(u"mark_clear_style_%1"_s.arg(i + 1));
        markClearStyleAction->setText(tr("Clear %1").arg(styleName));
        markClearStyleAction->setIcon(styleIcon);
        connect(markClearStyleAction, &QAction::triggered, this, [withEditor, i]() {
            withEditor([i](SciEdit *e) { e->clearMarks(i); e->setFocus(true); });
        });

        QAction *markJumpUpAction = actionCollection()->addAction(u"mark_jump_up_%1"_s.arg(i + 1));
        markJumpUpAction->setText(tr("Jump Up to %1").arg(styleName));
        markJumpUpAction->setIcon(styleIcon);
        actionCollection()->setDefaultShortcut(markJumpUpAction,
            QKeySequence(Qt::CTRL | Qt::SHIFT | markStyleKeys[i]));
        connect(markJumpUpAction, &QAction::triggered, this, [withEditor, i]() {
            withEditor([i](SciEdit *e) { e->goToPreviousMarked(i, true); e->setFocus(true); });
        });

        QAction *markJumpDownAction = actionCollection()->addAction(u"mark_jump_down_%1"_s.arg(i + 1));
        markJumpDownAction->setText(tr("Jump Down to %1").arg(styleName));
        markJumpDownAction->setIcon(styleIcon);
        actionCollection()->setDefaultShortcut(markJumpDownAction,
            QKeySequence(Qt::CTRL | markStyleKeys[i]));
        connect(markJumpDownAction, &QAction::triggered, this, [withEditor, i]() {
            withEditor([i](SciEdit *e) { e->goToNextMarked(i, true); e->setFocus(true); });
        });

        QAction *markCopyStyledAction = actionCollection()->addAction(u"mark_copy_styled_%1"_s.arg(i + 1));
        markCopyStyledAction->setText(tr("Copy Text Marked with %1").arg(styleName));
        markCopyStyledAction->setIcon(styleIcon);
        connect(markCopyStyledAction, &QAction::triggered, this, [this, i, withEditor]() {
            withEditor([this, i](SciEdit *e) {
                const QList<QPair<int, QString>> marked = e->markedTexts(1 << i);
                if (marked.isEmpty()) {
                    statusBar()->showMessage(tr("No marked text."));
                } else {
                    QApplication::clipboard()->setText(SciEdit::joinMarkedTexts(marked));
                    statusBar()->showMessage(tr("%n fragment(s) copied.", nullptr, marked.size()));
                }
                e->setFocus(true);
            });
        });
    }

    QAction *markClearAllStylesAction = actionCollection()->addAction(u"mark_clear_all_styles"_s);
    markClearAllStylesAction->setText(tr("Clear all Styles"));
    markClearAllStylesAction->setIcon(QIcon::fromTheme(u"edit-clear"_s));
    connect(markClearAllStylesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->clearAllMarkStyles(); e->setFocus(true); });
    });

    QAction *markCopyAllStyledAction = actionCollection()->addAction(u"mark_copy_all_styled"_s);
    markCopyAllStyledAction->setText(tr("Copy Text Marked with All Styles"));
    markCopyAllStyledAction->setIcon(QIcon::fromTheme(u"edit-copy"_s));
    connect(markCopyAllStyledAction, &QAction::triggered, this, [this, withEditor]() {
        withEditor([this](SciEdit *e) {
            const QList<QPair<int, QString>> marked = e->markedTexts(SciEdit::allMarkStylesMask());
            if (marked.isEmpty()) {
                statusBar()->showMessage(tr("No marked text."));
            } else {
                QApplication::clipboard()->setText(SciEdit::joinMarkedTexts(marked));
                statusBar()->showMessage(tr("%n fragment(s) copied.", nullptr, marked.size()));
            }
            e->setFocus(true);
        });
    });

    QAction *nextChangeAction = actionCollection()->addAction(u"change_next"_s);
    nextChangeAction->setText(tr("Next Change"));
    nextChangeAction->setIcon(QIcon::fromTheme(u"go-next"_s));
    connect(nextChangeAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->goToNextChange(true); e->setFocus(true); });
    });

    QAction *prevChangeAction = actionCollection()->addAction(u"change_previous"_s);
    prevChangeAction->setText(tr("Previous Change"));
    prevChangeAction->setIcon(QIcon::fromTheme(u"go-previous"_s));
    connect(prevChangeAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->goToPreviousChange(true); e->setFocus(true); });
    });

    QAction *clearChangeHistoryAction = actionCollection()->addAction(u"change_clear"_s);
    clearChangeHistoryAction->setText(tr("Clear Change History"));
    clearChangeHistoryAction->setIcon(QIcon::fromTheme(u"edit-clear"_s));
    connect(clearChangeHistoryAction, &QAction::triggered, this, [this]() {
        if (SciEdit *editor = m_viewManager->currentEditor()) {
            const int docId = m_viewManager->currentDocumentId();
            const bool wasModified = docId >= 0 && m_documentManager->isDocumentModified(docId);
            editor->clearChangeHistory();
            if (wasModified) {
                m_documentManager->setDocumentModified(docId, true);
            }
            editor->setFocus(true);
        }
    });

    QAction *goToAction = actionCollection()->addAction(u"go_to"_s);
    goToAction->setText(tr("Go to..."));
    goToAction->setIcon(QIcon::fromTheme(u"go-jump"_s, QIcon::fromTheme(u"go-next"_s)));
    actionCollection()->setDefaultShortcuts(goToAction, KStandardShortcut::gotoLine());
    connect(goToAction, &QAction::triggered, this, [this]() {
        showGoToDialog();
    });

    QAction *toggleSearchResultsAction = actionCollection()->addAction(u"toggle_search_results"_s);
    toggleSearchResultsAction->setText(tr("Search Results Panel"));
    toggleSearchResultsAction->setCheckable(true);
    toggleSearchResultsAction->setChecked(false);
    connect(toggleSearchResultsAction, &QAction::triggered, this, [this](bool checked) {
        ensureSearchResultsDock();
        m_searchResultsDock->setVisible(checked);
    });

    QAction *toggleFolderWorkspaceAction = actionCollection()->addAction(u"toggle_folder_workspace"_s);
    toggleFolderWorkspaceAction->setText(tr("Folder as Workspace"));
    toggleFolderWorkspaceAction->setIcon(QIcon::fromTheme(u"folder-symbolic"_s));
    toggleFolderWorkspaceAction->setCheckable(true);
    toggleFolderWorkspaceAction->setChecked(false);
    connect(toggleFolderWorkspaceAction, &QAction::triggered, this, [this](bool checked) {
        ensureFolderWorkspaceDock();
        m_folderWorkspaceDock->setVisible(checked);
    });

    QAction *toggleDocumentListAction = actionCollection()->addAction(u"toggle_document_list"_s);
    toggleDocumentListAction->setText(tr("Document List"));
    toggleDocumentListAction->setIcon(QIcon::fromTheme(u"view-list-details"_s));
    toggleDocumentListAction->setCheckable(true);
    toggleDocumentListAction->setChecked(false);
    connect(toggleDocumentListAction, &QAction::triggered, this, [this](bool checked) {
        ensureDocumentListDock();
        m_documentListDock->setVisible(checked);
    });

    QAction *toggleDocumentMapAction = actionCollection()->addAction(u"toggle_document_map"_s);
    toggleDocumentMapAction->setText(tr("Document Map"));
    toggleDocumentMapAction->setIcon(QIcon::fromTheme("map-symbolic"));
    toggleDocumentMapAction->setCheckable(true);
    toggleDocumentMapAction->setChecked(false);
    connect(toggleDocumentMapAction, &QAction::triggered, this, [this](bool checked) {
        ensureDocumentMapDock();
        m_documentMap->setVisible(checked);
        resizeDocks({m_documentMap}, { 120 }, Qt::Horizontal);
    });

    QAction *lruNextAction = actionCollection()->addAction(u"view_lru_document_next"_s);
    lruNextAction->setText(tr("Last Used Views"));
#ifdef Q_OS_MACOS
    KActionCollection::setDefaultShortcut(lruNextAction, Qt::MetaModifier | Qt::Key_Tab);
#else
    KActionCollection::setDefaultShortcut(lruNextAction, Qt::ControlModifier | Qt::Key_Tab);
#endif
    connect(lruNextAction, &QAction::triggered, this, [this]() {
        ensureQuickSwitcher();
        m_quickSwitcher->step(+1);
    });

    QAction *lruPrevAction = actionCollection()->addAction(u"view_lru_document_prev"_s);
    lruPrevAction->setText(tr("Last Used Views (Reverse)"));
#ifdef Q_OS_MACOS
    KActionCollection::setDefaultShortcut(lruPrevAction, Qt::MetaModifier | Qt::ShiftModifier | Qt::Key_Tab);
#else
    KActionCollection::setDefaultShortcut(lruPrevAction, Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_Tab);
#endif
    connect(lruPrevAction, &QAction::triggered, this, [this]() {
        ensureQuickSwitcher();
        m_quickSwitcher->step(-1);
    });

    QAction *nextTabAction = actionCollection()->addAction(u"tab_next"_s);
    nextTabAction->setText(tr("Next Tab"));
    KActionCollection::setDefaultShortcut(nextTabAction, Qt::CTRL | Qt::Key_PageDown);
    connect(nextTabAction, &QAction::triggered, this, [this]() {
        const int n = m_viewManager->editorsInSameTabGroup(m_viewManager->currentEditor()).size();
        if (n > 1)
            m_viewManager->setCurrentIndex((m_viewManager->currentIndex() + 1) % n);
    });

    QAction *prevTabAction = actionCollection()->addAction(u"tab_prev"_s);
    prevTabAction->setText(tr("Previous Tab"));
    KActionCollection::setDefaultShortcut(prevTabAction, Qt::CTRL | Qt::Key_PageUp);
    connect(prevTabAction, &QAction::triggered, this, [this]() {
        const int n = m_viewManager->editorsInSameTabGroup(m_viewManager->currentEditor()).size();
        if (n > 1)
            m_viewManager->setCurrentIndex((m_viewManager->currentIndex() - 1 + n) % n);
    });

    for (int i = 1; i <= 9; ++i) {
        QAction *gotoTabAction = actionCollection()->addAction(u"tab_goto_%1"_s.arg(i));
        gotoTabAction->setText(tr("Tab %1").arg(i));
        KActionCollection::setDefaultShortcut(gotoTabAction, Qt::ControlModifier | Qt::KeypadModifier | static_cast<Qt::Key>(Qt::Key_0 + i));
        connect(gotoTabAction, &QAction::triggered, this, [this, i]() {
            const int n = m_viewManager->editorsInSameTabGroup(m_viewManager->currentEditor()).size();
            if (i <= n)
                m_viewManager->setCurrentIndex(i - 1);
        });
    }

    // "Encoding" menu
    struct EncodingQuickEntry {
        QString displayName;
        QString codec;
        bool bom;
    };
    const QList<EncodingQuickEntry> quickEntries = {
        { u"ANSI (Latin1)"_s, u"ISO-8859-1"_s, false },
        { u"UTF-8"_s, u"UTF-8"_s, false },
        { u"UTF-8 (BOM)"_s, u"UTF-8"_s, true },
        { u"UTF-16 BE (BOM)"_s, u"UTF-16BE"_s, true },
        { u"UTF-16 LE (BOM)"_s, u"UTF-16LE"_s, true },
    };

    QMetaObject::invokeMethod(this, [this, quickEntries](){
        m_openWithEncodingListActions.clear();

        for (const auto &entry : quickEntries) {
            auto *action = new QAction(tr("%1", "Encoding name").arg(entry.displayName), this);
            connect(action, &QAction::triggered, this, [this, entry]() {
                openCurrentDocumentWithEncoding(entry.codec, entry.bom, entry.displayName);
            });
            m_openWithEncodingListActions.append(action);
        }

        auto *moreEncodingsMenu = new KActionMenu(tr("Character Set"), this);
        moreEncodingsMenu->setPopupMode(QToolButton::InstantPopup);

        const QList<QStringList> encodingGroups = EncodingGroupActionMenu::encodingGroups();
        for (const QStringList &group : encodingGroups) {
            if (group.size() < 2) {
                continue;
            }
            const QString groupName = group.first();
            QStringList encodingNames = group.mid(1);

            auto *groupMenu = new EncodingGroupActionMenu(groupName, encodingNames, this);
            connect(groupMenu, &EncodingGroupActionMenu::encodingSelected, this, [this](const QString &codecName, bool withBom, const QString &displayName){
                openCurrentDocumentWithEncoding(codecName, withBom, displayName);
            });
            moreEncodingsMenu->addAction(groupMenu);
        }
        m_openWithEncodingListActions.append(moreEncodingsMenu);

        unplugActionList("open_with_encoding_list");
        plugActionList(QStringLiteral("open_with_encoding_list"), m_openWithEncodingListActions);

        m_convertToEncodingListActions.clear();

        for (const auto &entry : quickEntries) {
            auto *action = new QAction(tr("Convert to %1").arg(entry.displayName), this);
            connect(action, &QAction::triggered, this, [this, entry]() {
                convertCurrentDocumentToEncoding(entry.codec, entry.bom, entry.displayName);
            });
            m_convertToEncodingListActions.append(action);
        }

        auto *moreConvertMenu = new KActionMenu(tr("Convert to..."), this);
        moreConvertMenu->setPopupMode(QToolButton::InstantPopup);

        for (const QStringList &group : encodingGroups) {
            if (group.size() < 2) {
                continue;
            }
            const QString groupName = group.first();
            QStringList encodingNames = group.mid(1);

            auto *groupMenu = new EncodingGroupActionMenu(groupName, encodingNames, this);
            connect(groupMenu, &EncodingGroupActionMenu::encodingSelected, this, [this](const QString &codecName, bool withBom, const QString &displayName){
                convertCurrentDocumentToEncoding(codecName, withBom, displayName);
            });
            moreConvertMenu->addAction(groupMenu);
        }
        m_convertToEncodingListActions.append(moreConvertMenu);

        unplugActionList("convert_to_encoding_list");
        plugActionList(QStringLiteral("convert_to_encoding_list"), m_convertToEncodingListActions);
    }, Qt::QueuedConnection);
    
    // "Language" menu
    QAction *lexerNoneAction = actionCollection()->addAction(u"lexer_none"_s);
    lexerNoneAction->setText(tr("None (Normal Text)"));
    connect(lexerNoneAction, &QAction::triggered, this, [this](){
        if (SciEdit *editor = m_viewManager->currentEditor()) {
            editor->setLexer(nullptr);
            const int docId = m_viewManager->currentDocumentId();
            if (docId >= 0) {
                m_documentManager->setDocumentLanguage(docId, QString());
            }
            updateStatusBar();
        }
    });

    QMetaObject::invokeMethod(this, [this](){
        rebuildLanguageMenu();
    }, Qt::QueuedConnection);

    QAction *languageSelectAction = actionCollection()->addAction(u"language_select"_s);
    languageSelectAction->setText(tr("Select Language..."));
    connect(languageSelectAction, &QAction::triggered, this, [this]() {
        openLanguageSelector(false);
    });

    QAction *newUserLanguageAction = actionCollection()->addAction(u"user_language_new"_s);
    newUserLanguageAction->setText(tr("New Custom Language..."));
    connect(newUserLanguageAction, &QAction::triggered, this, [this]() {
        openUserLanguageEditor();
    });

    QAction *manageLanguagesAction = actionCollection()->addAction(u"manage_languages"_s);
    manageLanguagesAction->setText(tr("Manage Languages..."));
    connect(manageLanguagesAction, &QAction::triggered, this, [this]() {
        openLanguageManager();
    });

    QAction *openKshUserDefinitionFolderAction = actionCollection()->addAction(u"open_ksh_user_definition_folder"_s);
    openKshUserDefinitionFolderAction->setText(tr("Open User Definition Folder..."));
    connect(openKshUserDefinitionFolderAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::appDataSubdirPath(QStringLiteral("syntax"))));
    });

    // "Settings" menu
    actionCollection()->addAction(u"style_menu"_s, KStyleManager::createConfigureAction(this));

    // "Tools" menu
    QAction *base64EncodeAction = actionCollection()->addAction(u"tools_base64_encode"_s);
    base64EncodeAction->setText(tr("Base64 Encode"));
    connect(base64EncodeAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->base64Encode(SciEdit::Base64Padding::NoPadding);
    });

    QAction *base64EncodePadAction = actionCollection()->addAction(u"tools_base64_encode_pad"_s);
    base64EncodePadAction->setText(tr("Base64 Encode with padding"));
    connect(base64EncodePadAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->base64Encode(SciEdit::Base64Padding::WithPadding);
    });

    QAction *base64EncodeEolAction = actionCollection()->addAction(u"tools_base64_encode_eol"_s);
    base64EncodeEolAction->setText(tr("Base64 Encode with Unix EOL"));
    connect(base64EncodeEolAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->base64Encode(SciEdit::Base64Padding::WithPadding, false, 64);
    });

    QAction *base64EncodeByLineAction = actionCollection()->addAction(u"tools_base64_encode_byline"_s);
    base64EncodeByLineAction->setText(tr("Base64 Encode by line"));
    connect(base64EncodeByLineAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->base64Encode(SciEdit::Base64Padding::NoPadding, true);
    });

    QAction *base64DecodeAction = actionCollection()->addAction(u"tools_base64_decode"_s);
    base64DecodeAction->setText(tr("Base64 Decode"));
    connect(base64DecodeAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->base64Decode();
    });

    QAction *base64DecodeStrictAction = actionCollection()->addAction(u"tools_base64_decode_strict"_s);
    base64DecodeStrictAction->setText(tr("Base64 Decode strict"));
    connect(base64DecodeStrictAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor()) {
            if (!e->base64Decode(true))
                QMessageBox::warning(this, tr("Base64 Decode"), tr("The selected text is not valid Base64."));
        }
    });

    QAction *base64DecodeByLineAction = actionCollection()->addAction(u"tools_base64_decode_byline"_s);
    base64DecodeByLineAction->setText(tr("Base64 Decode by line"));
    connect(base64DecodeByLineAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->base64Decode(false, true);
    });

    QAction *urlEncodeRfc1738Action = actionCollection()->addAction(u"tools_url_encode_rfc1738"_s);
    urlEncodeRfc1738Action->setText(tr("URL Encode (RFC1738)"));
    connect(urlEncodeRfc1738Action, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->urlEncode(SciEdit::UrlEncoding::Rfc1738);
    });

    QAction *urlEncodeRfc1738ByLineAction = actionCollection()->addAction(u"tools_url_encode_rfc1738_byline"_s);
    urlEncodeRfc1738ByLineAction->setText(tr("URL Encode (RFC1738) by line"));
    connect(urlEncodeRfc1738ByLineAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->urlEncode(SciEdit::UrlEncoding::Rfc1738, true);
    });

    QAction *urlEncodeExtendedAction = actionCollection()->addAction(u"tools_url_encode_extended"_s);
    urlEncodeExtendedAction->setText(tr("URL Encode (Extended)"));
    connect(urlEncodeExtendedAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->urlEncode(SciEdit::UrlEncoding::Extended);
    });

    QAction *urlEncodeExtendedByLineAction = actionCollection()->addAction(u"tools_url_encode_extended_byline"_s);
    urlEncodeExtendedByLineAction->setText(tr("URL Encode (Extended) by line"));
    connect(urlEncodeExtendedByLineAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->urlEncode(SciEdit::UrlEncoding::Extended, true);
    });

    QAction *urlEncodeFullAction = actionCollection()->addAction(u"tools_url_encode_full"_s);
    urlEncodeFullAction->setText(tr("URL Encode (Full)"));
    connect(urlEncodeFullAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->urlEncode(SciEdit::UrlEncoding::Full);
    });

    QAction *urlEncodeFullByLineAction = actionCollection()->addAction(u"tools_url_encode_full_byline"_s);
    urlEncodeFullByLineAction->setText(tr("URL Encode (Full) by line"));
    connect(urlEncodeFullByLineAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->urlEncode(SciEdit::UrlEncoding::Full, true);
    });

    QAction *urlDecodeAction = actionCollection()->addAction(u"tools_url_decode"_s);
    urlDecodeAction->setText(tr("URL Decode"));
    connect(urlDecodeAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor()) {
            if (!e->urlDecode())
                QMessageBox::warning(this, tr("URL Decode"), tr("The selected text contains an invalid percent-escape."));
        }
    });

    QAction *hexEncodeAction = actionCollection()->addAction(u"tools_hex_encode"_s);
    hexEncodeAction->setText(tr("Text to HEX"));
    connect(hexEncodeAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->hexEncode(false);
    });

    QAction *hexEncodeSpaceAction = actionCollection()->addAction(u"tools_hex_encode_space"_s);
    hexEncodeSpaceAction->setText(tr("Text to HEX (space separated)"));
    connect(hexEncodeSpaceAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->hexEncode(true);
    });

    QAction *hexDecodeAction = actionCollection()->addAction(u"tools_hex_decode"_s);
    hexDecodeAction->setText(tr("HEX to Text"));
    connect(hexDecodeAction, &QAction::triggered, this, [this]() {
        if (SciEdit *e = m_viewManager->currentEditor())
            e->hexDecode();
    });

    // "Run" menu
    QAction *runDialogAction = actionCollection()->addAction(u"run_dialog"_s);
    runDialogAction->setText(tr("&Run..."));
    actionCollection()->setDefaultShortcut(runDialogAction, QKeySequence(Qt::Key_F5));
    connect(runDialogAction, &QAction::triggered, this, [this]() {
        showRunDialog();
    });

    // "Macro" menu
    QAction *macroRecordStartAction = actionCollection()->addAction(u"macro_record_start"_s);
    macroRecordStartAction->setText(tr("Re&cord Macro"));
    macroRecordStartAction->setIcon(QIcon(":/icons/icons/macro-record.svg"));

    QAction *macroRecordStopAction = actionCollection()->addAction(u"macro_record_stop"_s);
    macroRecordStopAction->setText(tr("S&top Record Macro"));
    macroRecordStopAction->setIcon(QIcon(":/icons/icons/macro-record-stop.svg"));

    QAction *macroPlayAction = actionCollection()->addAction(u"macro_play"_s);
    macroPlayAction->setText(tr("&Play Macro"));
    macroPlayAction->setIcon(QIcon(":/icons/icons/macro-playback-start.svg"));

    QAction *macroRepeatAction = actionCollection()->addAction(u"macro_repeat"_s);
    macroRepeatAction->setText(tr("&Repeat Macro"));
    macroRepeatAction->setIcon(QIcon(":/icons/icons/macro-playback-repeat.svg"));

    QAction *macroSaveAction = actionCollection()->addAction(u"macro_save"_s);
    macroSaveAction->setText(tr("&Save Macro"));
    macroSaveAction->setIcon(QIcon(":/icons/icons/macro-save.svg"));

    connect(macroRecordStartAction, &QAction::triggered, this, [this]() {
        SciEdit *editor = m_viewManager->currentEditor();
        if (!editor || m_macroRecorder->isRecording()) {
            return;
        }
        m_currentMacro.reset();
        m_macroRecorder->start(editor);
        updateActions();
    });

    connect(macroRecordStopAction, &QAction::triggered, this, [this]() {
        if (!m_macroRecorder->isRecording()) {
            return;
        }
        m_currentMacro = m_macroRecorder->stop();
        if (m_currentMacro && m_currentMacro->isEmpty()) {
            m_currentMacro.reset();
        }
        updateActions();
    });

    connect(macroPlayAction, &QAction::triggered, this, [this]() {
        SciEdit *editor = m_viewManager->currentEditor();
        if (!editor || m_macroRecorder->isRecording() || !m_currentMacro) {
            return;
        }
        m_currentMacro->replay(editor, 1);
    });

    connect(macroRepeatAction, &QAction::triggered, this, [this]() {
        SciEdit *editor = m_viewManager->currentEditor();
        if (!editor || m_macroRecorder->isRecording()) {
            return;
        }

        const bool hasCurrentMacro = m_currentMacro && !m_currentMacro->isEmpty();
        if (!hasCurrentMacro && m_savedMacros.empty()) {
            return;
        }

        QVector<Macro *> savedPtrs;
        savedPtrs.reserve(static_cast<int>(m_savedMacros.size()));
        for (int i = 0; i < static_cast<int>(m_savedMacros.size()); ++i) {
            savedPtrs.push_back(m_savedMacros[i].get());
        }

        MacroRunDialog dialog(this);
        dialog.setMacros(m_currentMacro.get(), savedPtrs);

        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        Macro *macro = dialog.selectedMacro();
        if (!macro) {
            return;
        }

        const int count = dialog.runCount();
        if (count == -1) {
            macro->replayTillEndOfFile(editor);
        } else {
            macro->replay(editor, count);
        }
    });

    connect(macroSaveAction, &QAction::triggered, this, [this]() {
        if (m_macroRecorder->isRecording() || !m_currentMacro) {
            return;
        }
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Save Macro"), tr("Macro name:"), QLineEdit::Normal, QString(), &ok);
        if (!ok || name.trimmed().isEmpty()) {
            return;
        }
        auto savedMacro = std::make_unique<Macro>(*m_currentMacro);
        savedMacro->setName(name);
        m_savedMacros.push_back(std::move(savedMacro));
        saveSavedMacros();
        updateSavedMacroActions();
    });

    QAction *macroManageAction = actionCollection()->addAction(u"macro_manage"_s);
    macroManageAction->setText(tr("&Manage Macros..."));
    connect(macroManageAction, &QAction::triggered, this, [this]() {
        MacroManageDialog dialog(m_savedMacros, this);
        dialog.exec();
        saveSavedMacros();
        updateSavedMacroActions();
    });

    // "Help" menu
    QAction * sponsorAction = actionCollection()->addAction(u"help_sponsor"_s);
    sponsorAction->setText(tr("Sponsor"));
    sponsorAction->setIcon(QIcon::fromTheme(u"donate"_s));
    connect(sponsorAction, &QAction::triggered, this, [this]() {
        SponsorDialog dialog(this);
        dialog.exec();
    });

    // Toolbar actions
    KStandardAction::zoomIn(this, [withEditor]() {
        withEditor([](SciEdit *e) { e->zoomIn(); });
    }, actionCollection());
    KStandardAction::zoomOut(this, [withEditor]() {
        withEditor([](SciEdit *e) { e->zoomOut(); });
    }, actionCollection());

    QAction *zoomResetAction = actionCollection()->addAction(u"zoom_reset"_s);
    zoomResetAction->setText(tr("Restore Default Zoom"));
    zoomResetAction->setIcon(QIcon::fromTheme(u"zoom-original"_s));
    connect(zoomResetAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->setZoom(0); });
    });

    KStandardAction::preferences(this, &MainWindow::showSettings, actionCollection());

    QAction *toggleAlwaysOnTopAction = actionCollection()->addAction(u"toggle_always_on_top"_s);
    toggleAlwaysOnTopAction->setText(tr("Always on Top"));
    toggleAlwaysOnTopAction->setIcon(QIcon::fromTheme(u"window-pin"_s));
    toggleAlwaysOnTopAction->setCheckable(true);
    toggleAlwaysOnTopAction->setChecked(false);
    connect(toggleAlwaysOnTopAction, &QAction::toggled, this, [this](bool checked){
        setWindowFlag(Qt::WindowStaysOnTopHint, checked);
        show();
    });

    QAction *toggleWrapModeAction = actionCollection()->addAction(u"toggle_wrap_mode"_s);
    toggleWrapModeAction->setText(tr("Toggle Wrap Mode"));
    toggleWrapModeAction->setIcon(QIcon::fromTheme(u"text-wrap"_s));
    toggleWrapModeAction->setCheckable(true);
    toggleWrapModeAction->setChecked(AppSettings::wordWrapEnabled());
    connect(toggleWrapModeAction, &QAction::toggled, this, [this](bool checked){
        AppSettings::setWordWrapEnabled(checked);
        applySettingsToAllEditors();
        AppSettings::self()->save();
    });

    QAction *foldAllAction = actionCollection()->addAction(u"fold_all"_s);
    foldAllAction->setText(tr("Fold All"));
    connect(foldAllAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->foldAll(SC_FOLDACTION_CONTRACT); });
    });

    QAction *unfoldAllAction = actionCollection()->addAction(u"unfold_all"_s);
    unfoldAllAction->setText(tr("Unfold All"));
    connect(unfoldAllAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->foldAll(SC_FOLDACTION_EXPAND); });
    });

    QAction *foldCurrentLevelAction = actionCollection()->addAction(u"fold_current_level"_s);
    foldCurrentLevelAction->setText(tr("Fold Current Level"));
    connect(foldCurrentLevelAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->foldCurrentLevel(); });
    });

    QAction *unfoldCurrentLevelAction = actionCollection()->addAction(u"unfold_current_level"_s);
    unfoldCurrentLevelAction->setText(tr("Unfold Current Level"));
    connect(unfoldCurrentLevelAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->unfoldCurrentLevel(); });
    });

    QAction *hideLinesAction = actionCollection()->addAction(u"hide_lines"_s);
    hideLinesAction->setText(tr("Hide Lines"));
    actionCollection()->setDefaultShortcut(hideLinesAction, QKeySequence(Qt::CTRL | Qt::Key_H));
    connect(hideLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->hideSelectedLines(); });
    });

    QAction *showHiddenLinesAction = actionCollection()->addAction(u"show_hidden_lines"_s);
    showHiddenLinesAction->setText(tr("Show Hidden Lines"));
    connect(showHiddenLinesAction, &QAction::triggered, this, [withEditor]() {
        withEditor([](SciEdit *e) { e->showAllHiddenLines(); });
    });

    QAction *summaryAction = actionCollection()->addAction(u"summary"_s);
    summaryAction->setText(tr("Summary..."));
    summaryAction->setIcon(QIcon::fromTheme(u"documentinfo"_s));
    connect(summaryAction, &QAction::triggered, this, [this]() {
        showSummaryDialog();
    });

    QAction *toggleMonitoringAction = actionCollection()->addAction(u"toggle_monitoring"_s);
    toggleMonitoringAction->setText(tr("Monitor File (tail -f)"));
    toggleMonitoringAction->setIcon(QIcon::fromTheme("view-visible"));
    toggleMonitoringAction->setCheckable(true);
    connect(toggleMonitoringAction, &QAction::triggered, this, &MainWindow::toggleMonitoring);

    auto *toggleWhitespaceVisibilityAction = new KToolBarPopupAction(
        QIcon::fromTheme(u"invisible-char"_s, QIcon(":/icons/icons/invisible-char.svg")),
        tr("Show All Characters"), this);
    toggleWhitespaceVisibilityAction->setCheckable(true);
    toggleWhitespaceVisibilityAction->setChecked(AppSettings::whitespaceVisible());
    actionCollection()->addAction(u"toggle_show_all_characters"_s, toggleWhitespaceVisibilityAction);
    connect(toggleWhitespaceVisibilityAction, &QAction::toggled, this, [this](bool checked){
        AppSettings::setWhitespaceVisible(checked);
        AppSettings::setShowSpaceAndTab(checked);
        AppSettings::setShowEndOfLine(checked);
        AppSettings::setShowNonPrintingCharacters(checked);
        applySettingsToAllEditors();
        AppSettings::self()->save();
        updateActions();
    });

    QAction *toggleShowSpaceAndTabAction = actionCollection()->addAction(u"toggle_show_space_and_tab"_s);
    toggleShowSpaceAndTabAction->setText(tr("Show Space and Tab"));
    toggleShowSpaceAndTabAction->setCheckable(true);
    toggleShowSpaceAndTabAction->setChecked(AppSettings::showSpaceAndTab());
    connect(toggleShowSpaceAndTabAction, &QAction::toggled, this, [this](bool checked){
        AppSettings::setShowSpaceAndTab(checked);
        applySettingsToAllEditors();
        AppSettings::self()->save();
        updateActions();
    });

    QAction *toggleShowEndOfLineAction = actionCollection()->addAction(u"toggle_show_end_of_line"_s);
    toggleShowEndOfLineAction->setText(tr("Show End of Line"));
    toggleShowEndOfLineAction->setCheckable(true);
    toggleShowEndOfLineAction->setChecked(AppSettings::showEndOfLine());
    connect(toggleShowEndOfLineAction, &QAction::toggled, this, [this](bool checked){
        AppSettings::setShowEndOfLine(checked);
        applySettingsToAllEditors();
        AppSettings::self()->save();
        updateActions();
    });

    QAction *toggleShowNPCAction = actionCollection()->addAction(u"toggle_show_non_printing_characters"_s);
    toggleShowNPCAction->setText(tr("Show Non-Printing Characters"));
    toggleShowNPCAction->setCheckable(true);
    toggleShowNPCAction->setChecked(AppSettings::showNonPrintingCharacters());
    connect(toggleShowNPCAction, &QAction::toggled, this, [this](bool checked){
        AppSettings::setShowNonPrintingCharacters(checked);
        applySettingsToAllEditors();
        AppSettings::self()->save();
        updateActions();
    });

    auto *showSymbolMenu = toggleWhitespaceVisibilityAction->popupMenu();
    showSymbolMenu->addAction(toggleShowSpaceAndTabAction);
    showSymbolMenu->addAction(toggleShowEndOfLineAction);
    showSymbolMenu->addAction(toggleShowNPCAction);

    QAction *toggleIndentGuideAction = actionCollection()->addAction(u"toggle_indent_guide"_s);
    toggleIndentGuideAction->setText(tr("Toggle Indent Guide"));
    toggleIndentGuideAction->setIcon(QIcon::fromTheme(u"indent-guide"_s, QIcon(":/icons/icons/indent-guide.svg")));
    toggleIndentGuideAction->setCheckable(true);
    toggleIndentGuideAction->setChecked(AppSettings::indentGuidesEnabled());
    connect(toggleIndentGuideAction, &QAction::toggled, this, [this](bool checked){
        AppSettings::setIndentGuidesEnabled(checked);
        applySettingsToAllEditors();
        AppSettings::self()->save();
    });

    QAction *toggleWrapSymbolAction = actionCollection()->addAction(u"toggle_wrap_symbol"_s);
    toggleWrapSymbolAction->setText(tr("Show Wrap Symbol"));
    toggleWrapSymbolAction->setIcon(QIcon::fromTheme(u"text-wrap"_s));
    toggleWrapSymbolAction->setCheckable(true);
    toggleWrapSymbolAction->setChecked(AppSettings::wrapSymbolEnabled());
    connect(toggleWrapSymbolAction, &QAction::toggled, this, [this](bool checked){
        AppSettings::setWrapSymbolEnabled(checked);
        applySettingsToAllEditors();
        AppSettings::self()->save();
    });

    QAction *toggleSpellCheckAction = actionCollection()->addAction(u"toggle_spell_check"_s);
    toggleSpellCheckAction->setText(tr("Spell Check"));
    toggleSpellCheckAction->setIcon(QIcon::fromTheme(u"tools-check-spelling"_s));
    toggleSpellCheckAction->setCheckable(true);
    // Per-document toggle (mirrors toggle_read_only): the state lives on the
    // Document, new documents start from Sonnet's "enabled by default".
    connect(toggleSpellCheckAction, &QAction::triggered, this, [this](bool checked) {
        const int docId = m_viewManager->currentDocumentId();
        if (docId >= 0) {
            m_documentManager->setDocumentSpellCheckEnabled(docId, checked);
        }
        updateActions();
    });

    QAction *toggleVerticalEdgeAction = actionCollection()->addAction(u"toggle_vertical_edge"_s);
    toggleVerticalEdgeAction->setText(tr("Show Vertical Edge"));
    toggleVerticalEdgeAction->setCheckable(true);
    toggleVerticalEdgeAction->setChecked(AppSettings::verticalEdgeEnabled());
    connect(toggleVerticalEdgeAction, &QAction::toggled, this, [this](bool checked){
        AppSettings::setVerticalEdgeEnabled(checked);
        applySettingsToAllEditors();
        AppSettings::self()->save();
    });

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme scheme) {
        onColorSchemeChanged(scheme);
    });

    QAction *configureEditorPopupAction = actionCollection()->addAction(u"settings_configure_editor_popup"_s);
    configureEditorPopupAction->setText(tr("Configure Editor Context Menu..."));
    configureEditorPopupAction->setIcon(QIcon::fromTheme(u"settings-configure"_s));
    // On macOS, Qt treats actions whose text starts with "Config"/"Preference"/"Options"/
    // "Setting"/"Setup" as Preferences actions and relocates them into the Application menu,
    // replacing them with the generic "Preferences..." item. Opt out explicitly so the action
    // stays in the Settings menu on all platforms.
    configureEditorPopupAction->setMenuRole(QAction::NoRole);
    connect(configureEditorPopupAction, &QAction::triggered, this, [this]() {
        EditorPopupConfigDialog dlg(guiFactory(), this, this);
        dlg.exec();
    });

    // Load themes
    KColorSchemeManager *manager = KColorSchemeManager::instance();
    auto *colorSelectionMenu = KColorSchemeMenu::createMenu(manager, this);
    colorSelectionMenu->menu()->setTitle(tr("&Window Color Scheme"));
    actionCollection()->addAction(QStringLiteral("colorscheme_menu"), colorSelectionMenu);

    // The hamburger menu button only becomes visible while the menu bar is
    // hidden, so the default layout (traditional menu bar + toolbar) is kept.
    m_hamburgerMenu = KStandardAction::hamburgerMenu(nullptr, nullptr, actionCollection());
    QAction *showMenuBarAction = KStandardAction::showMenubar(this, &MainWindow::toggleMenuBar, actionCollection());

    setupGUI();

    m_hamburgerMenu->setMenuBar(menuBar());
    m_hamburgerMenu->setShowMenuBarAction(showMenuBarAction);
    // KMainWindow may have restored a hidden menu bar from the window state config.
    showMenuBarAction->setChecked(!menuBar()->isHidden());
}

void MainWindow::toggleMenuBar(bool visible)
{
    menuBar()->setVisible(visible);
    setSettingsDirty();
}

void MainWindow::saveNewToolbarConfig()
{
    KXmlGuiWindow::saveNewToolbarConfig();

    // The GUI was rebuilt from scratch (removeClient + addClient), which wiped
    // every plugged action list; plug them again.
    replugActionLists();
}

void MainWindow::replugActionLists()
{
    rebuildLanguageMenu();
    rebuildFavoriteActions();
    updateSavedMacroActions();

    // The encoding entries are static, so re-plug the existing actions as-is.
    unplugActionList(u"open_with_encoding_list"_s);
    if (!m_openWithEncodingListActions.isEmpty()) {
        plugActionList(u"open_with_encoding_list"_s, m_openWithEncodingListActions);
    }
    unplugActionList(u"convert_to_encoding_list"_s);
    if (!m_convertToEncodingListActions.isEmpty()) {
        plugActionList(u"convert_to_encoding_list"_s, m_convertToEncodingListActions);
    }
}

void MainWindow::ensureSearchResultsDock()
{
    if (m_searchResultsDock) {
        return;
    }

    m_searchResultsDock = new SearchResultsDock(this);
    m_searchResultsDock->setDocumentManager(m_documentManager);
    m_searchResultsDock->setEditorViewManager(m_viewManager);
    m_searchResultsDock->applyEditorSettings();
    m_findReplaceController->setSearchResultsDock(m_searchResultsDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_searchResultsDock);

    connectSearchResultsDock(m_searchResultsDock);

    connect(m_searchResultsDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (QAction *a = actionCollection()->action(QStringLiteral("toggle_search_results"))) {
            a->setChecked(visible);
        }
    });
}

void MainWindow::connectSearchResultsDock(SearchResultsDock *dock)
{
    connect(dock, &SearchResultsDock::searchResultActivated, this,
        [this](int docId, int lineNumber, int startPos, int endPos) {
            SciEdit *editor = m_viewManager->editorForDocId(docId);
            if (editor) {
                m_viewManager->switchToEditor(editor);
                editor->setSel(startPos, endPos);
                editor->scrollCaret();
                editor->setFocus(true);
            }
        });

    connect(dock, &SearchResultsDock::searchResultActivatedForFile, this,
        [this](const QString &filePath, int lineNumber, int startCol, int endCol) {
            int docId = openFileInternal(filePath);
            if (docId < 0) {
                // Untitled documents have no path to open; switch to the open
                // document listed under that title instead (Find-in-Finder
                // on "Untitled-N" entries).
                docId = m_documentManager->findUntitledDocumentByTitle(filePath);
            }
            if (docId < 0) {
                return;
            }
            SciEdit *editor = m_viewManager->editorForDocId(docId);
            if (!editor) {
                return;
            }
            m_viewManager->switchToEditor(editor);
            const int lineStart = editor->positionFromLine(lineNumber);
            const int lineEnd = editor->lineEndPosition(lineNumber);
            const QByteArray lineBytes = editor->textRange(lineStart, lineEnd);
            const QString lineStr = QString::fromUtf8(lineBytes);
            const int s = qBound(0, startCol, lineStr.length());
            const int e = qBound(0, endCol, lineStr.length());
            const int startPos = lineStart + lineStr.left(qMin(s, e)).toUtf8().size();
            const int endPos = lineStart + lineStr.left(qMax(s, e)).toUtf8().size();
            editor->setSel(startPos, endPos);
            editor->scrollCaret();
            editor->setFocus(true);
        });

    // Find-in-Finder works on any results panel, including the
    // results of a previous Find-in-Finder (chained refinement).
    connect(dock, &SearchResultsDock::findInResultsRequested, this, &MainWindow::findInSearchResults);

    // Volatile docks are destroyed when their "Close these search results"
    // action is used.
    connect(dock, &SearchResultsDock::closeRequested, this, [dock]() {
        dock->deleteLater();
    });
}

void MainWindow::findInSearchResults()
{
    auto *sourceDock = qobject_cast<SearchResultsDock *>(sender());
    if (!sourceDock) {
        return;
    }

    FindInFinderDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString needle = dialog.findTextForSearch();
    if (needle.isEmpty() && dialog.searchMode() != SearchMode::Regex) {
        return;
    }

    const QStringList files = sourceDock->allResultFilePaths();
    if (files.isEmpty()) {
        return;
    }

    FindInFilesSearcher::Params params;
    params.needle = needle;
    params.matchCase = dialog.matchCase();
    params.wholeWord = dialog.wholeWord();
    params.mode = dialog.searchMode();

    // Built before the destination dock so the dialog stays visually on top.
    QHash<QString, QSet<int>> lineFilter;
    const QHash<QString, QSet<int>> *lineFilterPtr = nullptr;
    if (dialog.matchLineNumber()) {
        lineFilter = sourceDock->resultLinesByFile();
        lineFilterPtr = &lineFilter;
    }

    auto *destDock = new SearchResultsDock(this);
    destDock->setVolatile(true);
    connectSearchResultsDock(destDock);
    addDockWidget(Qt::BottomDockWidgetArea, destDock);
    if (m_searchResultsDock) {
        tabifyDockWidget(m_searchResultsDock, destDock);
    }
    destDock->applyEditorSettings();

    m_findReplaceController->findInFilesList(params, files, lineFilterPtr, destDock);
}

void MainWindow::ensureFolderWorkspaceDock()
{
    if (m_folderWorkspaceDock) {
        return;
    }

    m_folderWorkspaceDock = new FolderWorkspaceDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_folderWorkspaceDock);

    m_folderWorkspaceManager = new FolderWorkspaceManager(this);
    m_folderWorkspaceDock->setManager(m_folderWorkspaceManager);

    connect(m_folderWorkspaceDock, &FolderWorkspaceDock::fileDoubleClicked, this,
        [this](const QString &filePath) {
            openFileInternal(filePath);
        });

    connect(m_folderWorkspaceDock, &FolderWorkspaceDock::selectFolderRequested, this,
        &MainWindow::openFolderWorkspace);

    connect(m_folderWorkspaceDock, &FolderWorkspaceDock::useCurrentDocumentDirectoryRequested, this,
        [this]() {
            int docId = m_viewManager->currentDocumentId();
            if (docId < 0) {
                QMessageBox::warning(this, tr("Folder as Workspace"),
                    tr("No document is currently open."));
                return;
            }
            const QString filePath = m_documentManager->getDocumentFilePath(docId);
            if (filePath.isEmpty()) {
                QMessageBox::warning(this, tr("Folder as Workspace"),
                    tr("The current document has not been saved to disk."));
                return;
            }
            m_folderWorkspaceDock->addWorkspace(QFileInfo(filePath).absolutePath());
        });

    connect(m_folderWorkspaceDock, &FolderWorkspaceDock::useCurrentDocumentGitRootRequested, this,
        [this]() {
            int docId = m_viewManager->currentDocumentId();
            if (docId < 0) {
                QMessageBox::warning(this, tr("Folder as Workspace"),
                    tr("No document is currently open."));
                return;
            }
            const QString filePath = m_documentManager->getDocumentFilePath(docId);
            if (filePath.isEmpty()) {
                QMessageBox::warning(this, tr("Folder as Workspace"),
                    tr("The current document has not been saved to disk."));
                return;
            }
            const QString dirPath = QFileInfo(filePath).absolutePath();
            if (QStandardPaths::findExecutable(u"git"_s).isEmpty()) {
                QMessageBox::warning(this, tr("Folder as Workspace"),
                    tr("Git is not installed or not found in PATH."));
                return;
            }
            QProcess git;
            git.setWorkingDirectory(dirPath);
            git.start(u"git"_s, {u"rev-parse"_s, u"--show-toplevel"_s});
            if (!git.waitForFinished(5000) || git.exitCode() != 0) {
                QMessageBox::warning(this, tr("Folder as Workspace"),
                    tr("Could not determine the Git repository for the current document."));
                return;
            }
            const QString gitRoot = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
            if (gitRoot.isEmpty() || !QDir(gitRoot).exists()) {
                QMessageBox::warning(this, tr("Folder as Workspace"),
                    tr("Could not determine the Git repository for the current document."));
                return;
            }
            m_folderWorkspaceDock->addWorkspace(gitRoot);
        });

    connect(m_folderWorkspaceDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (QAction *a = actionCollection()->action(QStringLiteral("toggle_folder_workspace"))) {
            a->setChecked(visible);
        }
    });
}

void MainWindow::ensureDocumentMapDock()
{
    if (m_documentMap) {
        return;
    }

    m_documentMap = new DocumentMap(m_documentManager, this);
    addDockWidget(Qt::RightDockWidgetArea, m_documentMap);

    connect(m_viewManager, &TabWidget::currentEditorChanged, m_documentMap, [this](SciEdit *editor) {
        if (m_documentMap->isVisible()) {
            m_documentMap->setActiveEditor(editor);
        }
    });

    connect(m_documentMap, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (QAction *a = actionCollection()->action(QStringLiteral("toggle_document_map"))) {
            a->setChecked(visible);
        }
        if (visible) {
            m_documentMap->setActiveEditor(m_viewManager->currentEditor());
        } else {
            m_documentMap->setActiveEditor(nullptr);
        }
    });
}

void MainWindow::ensureDocumentListDock()
{
    if (m_documentListDock) {
        return;
    }

    m_documentListDock = new DocumentListDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_documentListDock);
    m_documentListDock->setDocumentManager(m_documentManager);
    m_documentListDock->setEditorViewManager(m_viewManager);
    m_documentListDock->setCurrentDocId(m_viewManager->currentDocumentId());

    connect(m_documentListDock, &DocumentListDock::documentActivated, this, [this](int docId) {
        m_viewManager->switchToDocId(docId);
    });
    connect(m_documentListDock, &DocumentListDock::closeRequested, this, [this](int docId) {
        closeDocument(docId);
    });
    connect(m_documentListDock, &DocumentListDock::contextMenuRequested, this, [this](int docId, const QPoint &pos) {
        QMenu *menu = buildDocumentContextMenu(docId, nullptr, this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->exec(pos);
    });

    connect(m_documentListDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (QAction *a = actionCollection()->action(u"toggle_document_list"_s)) {
            a->setChecked(visible);
        }
    });
}

void MainWindow::ensureQuickSwitcher()
{
    if (m_quickSwitcher) {
        return;
    }

    m_quickSwitcher = new QuickSwitcher(m_documentManager, this);
    m_quickSwitcher->hide();

    if (QAction *aNext = actionCollection()->action(QStringLiteral("view_lru_document_next")))
        m_quickSwitcher->addAction(aNext);
    if (QAction *aPrev = actionCollection()->action(QStringLiteral("view_lru_document_prev")))
        m_quickSwitcher->addAction(aPrev);

    connect(m_quickSwitcher, &QuickSwitcher::documentActivated, this, [this](int docId) {
        m_viewManager->switchToDocId(docId);
    });
}

void MainWindow::openFolderWorkspace()
{
    QFileDialog::Options options;
    if (!AppSettings::useNativeFileDialog()) {
        options |= QFileDialog::DontUseNativeDialog;
    }

    QString dirPath = QFileDialog::getExistingDirectory(this,
        tr("Open Folder as Workspace"),
        QString(),
        options);

    if (dirPath.isEmpty()) {
        return;
    }

    ensureFolderWorkspaceDock();
    m_folderWorkspaceDock->addWorkspace(dirPath);
}

static std::optional<qlonglong> parseNumericInput(const QString &input)
{
    const QString s = input.trimmed();
    if (s.isEmpty()) {
        return std::nullopt;
    }

    bool ok = false;
    int base = 10;
    QStringView sv(s);
    if (sv.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        base = 16;
        sv = sv.mid(2);
    }
    const qlonglong value = sv.toString().toLongLong(&ok, base);
    if (!ok) {
        return std::nullopt;
    }
    return value;
}

void MainWindow::showGoToDialog()
{
    SciEdit *editor = m_viewManager->currentEditor();
    if (!editor) {
        return;
    }

    if (!m_goToDialog) {
        m_goToDialog = new GoToDialog(this);
        connect(m_goToDialog, &GoToDialog::goToRequested, this, [this](bool lineMode, const QString &text) {
            SciEdit *editor = m_viewManager->currentEditor();
            if (!editor) {
                return;
            }

            const auto parsed = parseNumericInput(text);
            if (!parsed.has_value()) {
                return;
            }

            if (lineMode) {
                const qlonglong lineInput = qMax<qlonglong>(1, *parsed);
                const sptr_t maxLine = qMax<sptr_t>(1, editor->lineCount());
                const sptr_t targetLine = sptr_t(qBound<qlonglong>(1, lineInput, qlonglong(maxLine))) - 1;
                editor->gotoLine(targetLine);
                editor->scrollCaret();
            } else {
                const qlonglong posInput = qMax<qlonglong>(0, *parsed);
                const sptr_t len = editor->length();
                const sptr_t targetPos = sptr_t(qBound<qlonglong>(0, posInput, qlonglong(len)));
                editor->gotoPos(targetPos);
                editor->scrollCaret();
            }

            m_goToDialog->hide();
            editor->setFocus(true);
        });
    }

    const int currentLine = int(editor->lineFromPosition(editor->currentPos())) + 1;
    const qlonglong currentPos = qlonglong(editor->currentPos());
    m_goToDialog->openWithDefaults(currentLine, currentPos);
}

void MainWindow::showRunDialog()
{
    SciEdit *editor = m_viewManager->currentEditor();
    if (!editor) {
        return;
    }

    if (!m_runDialog) {
        m_runDialog = new RunDialog(this);
    }

    const int docId = m_viewManager->currentDocumentId();
    m_runDialog->setRunContext(editor, m_documentManager, docId);
    m_runDialog->show();
    m_runDialog->raise();
    m_runDialog->activateWindow();
}

void MainWindow::showSummaryDialog()
{
    SciEdit *editor = m_viewManager->currentEditor();
    if (!editor) {
        return;
    }

    const sptr_t length = editor->length();
    const sptr_t lines = editor->lineCount();
    const QByteArray textBytes = editor->getText(length);
    const QString text = QString::fromUtf8(textBytes);

    auto numberToString = [](qlonglong value) {
        return QLocale::system().toString(value);
    };
    auto countCharsWithoutLineEnding = [](const QString &value) {
        qlonglong count = 0;
        for (const QChar &ch : value) {
            if (ch != QLatin1Char('\n') && ch != QLatin1Char('\r')) {
                ++count;
            }
        }
        return count;
    };
    auto countWords = [](const QString &value) {
        qlonglong count = 0;
        bool inWord = false;
        for (const QChar &ch : value) {
            const bool isWord = ch.isLetterOrNumber() || ch == QLatin1Char('_');
            if (isWord) {
                if (!inWord) {
                    ++count;
                    inWord = true;
                }
            } else {
                inWord = false;
            }
        }
        return count;
    };

    const qlonglong charCount = countCharsWithoutLineEnding(text);
    const qlonglong wordCount = countWords(text);

    const int selectionRanges = static_cast<int>(editor->selections());
    qlonglong selectedBytes = 0;
    qlonglong selectedChars = 0;
    for (int i = 0; i < selectionRanges; ++i) {
        const sptr_t start = editor->selectionNStart(i);
        const sptr_t end = editor->selectionNEnd(i);
        const sptr_t minPos = qMin(start, end);
        const sptr_t maxPos = qMax(start, end);
        const sptr_t rangeLength = maxPos - minPos;
        if (rangeLength <= 0) {
            continue;
        }
        selectedBytes += rangeLength;

        const QByteArray rangeBytes = editor->get_text_range(minPos, maxPos);
        const QString rangeText = QString::fromUtf8(rangeBytes);
        selectedChars += countCharsWithoutLineEnding(rangeText);
    }

    const QString filePath = m_documentManager->getDocumentFilePath(m_viewManager->currentDocumentId());
    SummaryData data;
    if (!filePath.isEmpty()) {
        const QFileInfo info(filePath);
        data.filePath = QDir::toNativeSeparators(filePath);
        data.created = QLocale::system().toString(info.birthTime(), QLocale::ShortFormat);
        data.modified = QLocale::system().toString(info.lastModified(), QLocale::ShortFormat);
        data.hasFileInfo = true;
    }
    data.characters = numberToString(charCount);
    data.words = numberToString(wordCount);
    data.lines = numberToString(lines);
    data.documentLength = numberToString(length);
    data.selectedCharacters = numberToString(selectedChars);
    data.selectedBytes = numberToString(selectedBytes);
    data.selectionRanges = numberToString(selectionRanges);

    SummaryDialog dialog(this);
    dialog.setSummaryData(data);
    dialog.exec();
}

namespace {

// Search settings page with dependent-widget enabling: the selection fill
// limit and the word-under-caret option only apply while "Fill find field
// with selected text" is checked.
class SearchSettingsPage : public SettingsPage<Ui::SearchSettings>
{
public:
    explicit SearchSettingsPage(QWidget *parent)
        : SettingsPage<Ui::SearchSettings>(parent)
    {
        const auto updateDependent = [this](bool checked) {
            ui.kcfg_fillFindFieldMaxChars->setEnabled(checked);
            ui.kcfg_fillFindFieldWithWordAtCaret->setEnabled(checked);
        };
        connect(ui.kcfg_fillFindFieldWithSelection, &QCheckBox::toggled,
                this, updateDependent);
        updateDependent(ui.kcfg_fillFindFieldWithSelection->isChecked());
    }
};

} // namespace

void MainWindow::showSettings()
{
    if (KConfigDialog::showDialog(QStringLiteral("settings"))) {
        return;
    }
    auto *dialog = new SettingsDialog(this, QStringLiteral("settings"), AppSettings::self());
    dialog->setFaceType(KPageDialog::FlatList);

    auto *page = new SettingsPage<Ui::GeneralSettings>(dialog);
    dialog->addPage(page, tr("General"), "preferences-system");

    auto *fontPage = new SettingsPage<Ui::FontSettings>(dialog);
    dialog->addPage(fontPage, tr("Font"), "preferences-desktop-font");

    auto *editingPage = new SettingsPage<Ui::EditingSettings>(dialog);
    dialog->addPage(editingPage, tr("Editing"), "accessories-text-editor");

    auto *editorThemePage = new EditorThemePage(dialog);
    dialog->addPage(editorThemePage, tr("Editor Theme"), "preferences-desktop-color");
    dialog->setEditorThemePage(editorThemePage);
    connect(editorThemePage, &EditorThemePage::themeColorsChanged, this, [this]() {
        applySettingsToAllEditors();
    });

    auto *marginPage = new SettingsPage<Ui::MarginSettings>(dialog);
    dialog->addPage(marginPage, tr("Margin & Edge"), "bordertool");

    auto *indentPage = new SettingsPage<Ui::IndentationSettings>(dialog);
    dialog->addPage(indentPage, tr("Indentation"), "preferences-desktop-keyboard");

    auto *highlightPage = new SettingsPage<Ui::HighlightsSettings>(dialog);
    dialog->addPage(highlightPage, tr("Highlighting"), "draw-highlight");

    auto *searchPage = new SearchSettingsPage(dialog);
    dialog->addPage(searchPage, tr("Search"), "edit-find");

    auto *backupPage = new SettingsPage<Ui::BackupSettings>(dialog);
    dialog->addPage(backupPage, tr("Backup"), "document-save");

    auto *autoCompletionPage = new SettingsPage<Ui::AutoCompletionSettings>(dialog);
    dialog->addPage(autoCompletionPage, tr("Auto Completion"), "auto-type");

    // Spell check: dictionary/backend/language configuration only; the
    // on/off toggle is per document (View menu). Sonnet::ConfigWidget does
    // not persist by itself, so save and apply changes live.
    auto *spellCheckConfig = new Sonnet::ConfigWidget(dialog);
    dialog->addPage(spellCheckConfig, tr("Spell Check"), "tools-check-spelling");
    connect(spellCheckConfig, &Sonnet::ConfigWidget::configChanged, spellCheckConfig, [spellCheckConfig]() {
        spellCheckConfig->save();
    });
    connect(spellCheckConfig, &Sonnet::ConfigWidget::configChanged, this, [this]() {
        const QVector<SciEdit *> allEditors = m_viewManager->editors();
        for (SciEdit *editor : allEditors) {
            editor->refreshSpellChecker();
        }
    });

    auto *fileWatchPage = new SettingsPage<Ui::FileWatchSettings>(dialog);
    dialog->addPage(fileWatchPage, tr("File Watching"), "view-visible");

    connect(dialog, &KConfigDialog::settingsChanged, this, [this]() {
        applyAppFont();
        applySettingsToAllEditors();
        updateBackupTimer();
        m_fileChangeWatcher->refreshFromFileSettings();
        if (m_recentFilesAction) {
            m_recentFilesAction->setMaxItems(AppSettings::recentFilesMaxItems());
        }
    });

    dialog->show();
}

void MainWindow::onColorSchemeChanged(Qt::ColorScheme scheme)
{
    qCDebug(lcMainWindow) << "onColorSchemeChanged" << scheme << isVisible();
    if (!KColorSchemeManager::instance()->activeSchemeId().isEmpty()) {
        scheme = KColorSchemeManager::instance()->activeSchemeId().contains("Dark")
            ? Qt::ColorScheme::Dark
            : Qt::ColorScheme::Light;
    }

    const QString currentId = AppSettings::editorTheme().trimmed().isEmpty()
        ? QStringLiteral("Pineapple Notepad Light")
        : AppSettings::editorTheme().trimmed();
    const QString variantId = ThemeConfigStore::themeVariant(currentId, scheme == Qt::ColorScheme::Dark);
    if (!variantId.isEmpty() && variantId != currentId) {
        AppSettings::setEditorTheme(variantId);
        AppSettings::self()->save();
    }
    applySettingsToAllEditors();
}

bool MainWindow::hasUnsavedOrTempDocuments() const
{
    const QVector<int> docIds = m_viewManager->documentIds();
    for (int docId : docIds) {
        if (docId < 0) {
            continue;
        }
        const bool isModified = m_documentManager->isDocumentModified(docId);
        const bool isUntitled = m_documentManager->isDocumentUntitled(docId);
        if (isModified) {
            return true;
        }
        if (isUntitled) {
            const SciEdit *editor = m_viewManager->editorForDocId(docId);
            const bool isEmpty = editor ? (editor->textLength() == 0) : true;
            if (!isEmpty) {
                return true;
            }
        }
    }
    return false;
}

bool MainWindow::saveAllUnsavedOrTempDocuments()
{
    const QVector<int> docIds = m_viewManager->documentIds();
    for (int docId : docIds) {
        if (docId < 0) {
            continue;
        }
        const bool isModified = m_documentManager->isDocumentModified(docId);
        const bool isUntitled = m_documentManager->isDocumentUntitled(docId);
        if (!isModified && !isUntitled) {
            continue;
        }
        if (isUntitled && !isModified) {
            const SciEdit *editor = m_viewManager->editorForDocId(docId);
            const bool isEmpty = editor ? (editor->textLength() == 0) : true;
            if (isEmpty) {
                continue;
            }
        }
        m_viewManager->switchToDocId(docId);
        if (!saveDocument(docId)) {
            return false;
        }
    }
    return true;
}

void MainWindow::saveSession()
{
    m_sessionManager->saveSession(m_viewManager);
}

void MainWindow::clearSession()
{
    m_sessionManager->clearSession();
}

void MainWindow::updateBackupTimer()
{
    const bool enabled = AppSettings::backupEnabled();
    const int interval = AppSettings::backupIntervalSeconds();
    if (enabled && interval > 0) {
        if (!m_backupTimer) {
            m_backupTimer = new QTimer(this);
            connect(m_backupTimer, &QTimer::timeout, this, [this]() {
                qCDebug(lcMainWindow) << "Periodic backup...";
                m_sessionManager->saveSession(m_viewManager);
            });
        }
        m_backupTimer->start(interval * 1000);
    } else {
        if (m_backupTimer) {
            m_backupTimer->stop();
        }
    }
}

void MainWindow::applyLexer(const QString &lexerName)
{
    if (SciEdit *editor = m_viewManager->currentEditor()) {
        DocumentManager::applyLexerToEditor(editor, lexerName, m_documentManager);
        const int docId = m_viewManager->currentDocumentId();
        if (docId >= 0) {
            m_documentManager->setDocumentLanguage(docId, lexerName);
        }
        updateStatusBar();
    }
}

void MainWindow::openLanguageSelector(bool defaultCustomOnly)
{
    LanguageSelectorDialog dlg(defaultCustomOnly, this);
    connect(&dlg, &LanguageSelectorDialog::languageMenuChanged, this, &MainWindow::rebuildLanguageMenu);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const QString languageId = dlg.selectedLanguageId();
    if (languageId.isEmpty()) {
        return;
    }
    applyLexer(languageId);
}

void MainWindow::openUserLanguageEditor()
{
    UserLanguageEditorDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        rebuildLanguageMenu();
    }
}

void MainWindow::openLanguageManager()
{
    LanguageManagerDialog dlg(this);
    connect(&dlg, &LanguageManagerDialog::languagesChanged, this, &MainWindow::rebuildLanguageMenu);
    dlg.exec();
}

void MainWindow::rebuildLanguageMenu()
{
    // Resolve the effective entry list: the user's pinned list, or the full
    // built-in catalog when the user has not customized it yet.
    QList<LanguageMenuEntry> entries;
    const QStringList configured = AppSettings::languageMenuEntries();
    if (configured.isEmpty()) {
        entries = LanguageConfigStore::defaultMenuEntries();
    } else {
        for (const QString &encoded : configured) {
            const auto entry = LanguageConfigStore::decodeMenuEntry(encoded);
            if (entry.valid) {
                entries.append(entry);
            }
        }
    }

    struct MenuItem {
        QString languageId;
        QString displayName;
    };
    QList<MenuItem> items;
    items.reserve(entries.size());
    for (const auto &entry : std::as_const(entries)) {
        items.append({ entry.languageId, entry.displayName });
    }
    std::stable_sort(items.begin(), items.end(), [](const MenuItem &a, const MenuItem &b) {
        return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
    });

    // Group by the first character of the display name (A-Z submenus).
    QMap<QChar, QList<MenuItem>> grouped;
    for (const MenuItem &item : std::as_const(items)) {
        const QString sortName = item.displayName.isEmpty() ? item.languageId : item.displayName;
        grouped[sortName.at(0).toLower()].append(item);
    }

    auto lexerGroupListActions = QList<QAction*>();
    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
        const QList<MenuItem> groupItems = it.value();
        if (groupItems.size() == 1) {
            const MenuItem &item = groupItems.first();
            auto *action = new QAction(item.displayName, this);
            connect(action, &QAction::triggered, this, [this, item]() {
                applyLexer(item.languageId);
            });
            lexerGroupListActions.append(action);
        } else {
            QStringList languageIds;
            languageIds.reserve(groupItems.size());
            for (const MenuItem &item : groupItems) {
                languageIds.append(item.languageId);
            }
            auto *lexerGroupMenu = new LexerGroupActionMenu(QString(it.key().toUpper()), this);
            lexerGroupMenu->setLanguageIds(languageIds);
            connect(lexerGroupMenu, &LexerGroupActionMenu::lexerSelected, this, &MainWindow::applyLexer);
            lexerGroupListActions.append(lexerGroupMenu);
        }
    }

    unplugActionList("lexer_group_list");
    plugActionList(QStringLiteral("lexer_group_list"), lexerGroupListActions);
}

bool MainWindow::saveDocument(int docId)
{
    if (docId < 0) {
        return false;
    }

    if (m_documentManager->isDocumentUntitled(docId)) {
        const QString filePath = m_documentManager->getDocumentFilePath(docId);
        if (filePath.isEmpty()) {
            return saveDocumentAs(docId);
        }
        return m_documentManager->saveDocumentAs(docId, filePath);
    }

    const bool saved = m_documentManager->saveDocument(docId);
    if (saved) {
        if (SciEdit *editor = m_viewManager->editorForDocId(docId)) {
            editor->setSavePoint();
        }
    } else {
        const QString path = m_documentManager->getDocumentFilePath(docId);
        QMessageBox::warning(this, tr("Save Error"),
            tr("Failed to save file:\n%1").arg(path));
    }
    return saved;
}

bool MainWindow::saveDocumentAs(int docId)
{
    if (docId < 0) {
        return false;
    }

    SciEdit *editor = m_viewManager->editorForDocId(docId);
    if (!editor) {
        return false;
    }

    QFileDialog::Options options;
    if (!AppSettings::useNativeFileDialog()) {
        options |= QFileDialog::DontUseNativeDialog;
    }

    const QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save File"),
        m_documentManager->getDocumentTitle(docId),
        tr("All Files (*.*)"),
        nullptr,
        options);

    if (fileName.isEmpty()) {
        return false;
    }

    const bool saved = m_documentManager->saveDocumentAs(docId, fileName);
    if (saved) {
        const QString lexerName = m_documentManager->getDocumentLanguage(docId);
        DocumentManager::applyLexerToEditor(editor, lexerName, m_documentManager);
        editor->setSavePoint();
        m_viewManager->refreshEditorsForDoc(docId);
    } else {
        QMessageBox::warning(this, tr("Save Error"),
            tr("Failed to save file:\n%1").arg(fileName));
    }
    return saved;
}

bool MainWindow::saveAllDocuments()
{
    const QVector<int> docIds = m_viewManager->documentIds();
    for (int docId : docIds) {
        if (docId < 0) {
            continue;
        }
        if (!m_documentManager->isDocumentModified(docId)) {
            continue;
        }
        m_viewManager->switchToDocId(docId);
        if (!saveDocument(docId)) {
            return false;
        }
    }
    return true;
}

bool MainWindow::closeDocument(int docId, bool createNewIfEmpty)
{
    if (docId < 0) {
        return false;
    }

    SciEdit *editor = m_viewManager->editorForDocId(docId);
    if (!editor) {
        return false;
    }

    const bool isUntitledTempEmptyDoc = m_documentManager->isDocumentUntitled(docId) && m_documentManager->getDocumentFilePath(docId).isEmpty() && (editor->textLength() == 0);
    if (m_documentManager->isDocumentModified(docId) && !isUntitledTempEmptyDoc) {
        const QString title = m_documentManager->getDocumentTitle(docId);
        const int ret = QMessageBox::question(this, tr("Save Changes"),
            tr("The document '%1' has been modified.\nDo you want to save your changes?").arg(title),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (ret == QMessageBox::Save) {
            bool saved = false;
            if (m_documentManager->isDocumentUntitled(docId)) {
                saved = saveDocumentAs(docId);
            } else {
                saved = saveDocument(docId);
            }

            if (!saved) {
                return false;
            }
        } else if (ret == QMessageBox::Cancel) {
            return false;
        }
    }

    Document *doc = m_documentManager->getDocument(docId);
    if (doc) {
        doc->removeEditorReference(editor);
    }

    m_viewManager->removeAllEditorsForDoc(docId);

    if (!isUntitledTempEmptyDoc) {
        const QString closedFilePath = m_documentManager->getDocumentFilePath(docId);
        if (!closedFilePath.isEmpty()) {
            m_closedTabHistory.removeAll(closedFilePath);
            m_closedTabHistory.prepend(closedFilePath);
            while (m_closedTabHistory.size() > 30)
                m_closedTabHistory.removeLast();
        }
    }

    if (!doc || !doc->hasAnyEditor()) {
        m_documentManager->closeDocument(docId);
    }

    if (createNewIfEmpty && m_viewManager->count() == 0) {
        newFileInternal();
    }

    return true;
}

bool MainWindow::closeAllDocuments()
{
    const QVector<int> docIds = m_viewManager->documentIds();
    for (int i = docIds.size() - 1; i >= 0; --i) {
        if (!closeDocument(docIds[i], false)) {
            return false;
        }
    }
    if (m_viewManager->count() == 0) {
        newFileInternal();
    }
    return true;
}

SciEdit *MainWindow::createEditor(int docId)
{
    SciEdit *editor = new SciEdit(m_viewManager->viewWidget());
    m_documentManager->setDocumentIndentTabWidth(docId, AppSettings::tabWidth());
    m_documentManager->setDocumentIndentUseTabs(docId, AppSettings::indentUseTabs());
    editor->setTabWidth(m_documentManager->getDocumentIndentTabWidth(docId));
    editor->setIndentUseTabs(m_documentManager->getDocumentIndentUseTabs(docId));
    editor->setEditorFont(AppSettings::editorFont());
    editor->setThemeId(AppSettings::editorTheme());
    editor->setAutoIndentMode(static_cast<SciEdit::AutoIndentMode>(AppSettings::autoIndentMode()));
    editor->setMultiSelection(AppSettings::multiSelection());
    editor->QObject::setProperty("documentId", docId);

    if (Document *doc = m_documentManager->getDocument(docId)) {
        doc->attachEditor(editor);
    }

    connect(editor, &ScintillaEditBase::savePointChanged, this, [this, docId]() {
        recomputeDocumentModified(docId);
    });

    return editor;
}

void MainWindow::recomputeDocumentModified(int docId)
{
    SciEdit *editor = m_viewManager->editorForDocId(docId);
    if (!editor) {
        return;
    }
    const bool savePointDirty = editor->modify();
    const bool modified = savePointDirty || (m_documentManager->isDocumentUntitled(docId) &&
        (!m_documentManager->getDocumentFilePath(docId).isEmpty() || editor->length() > 0));
    m_documentManager->setDocumentModified(docId, modified);
}

int MainWindow::newFileInternal(NewTabPlacement placement, bool activate)
{
    int docId = m_documentManager->createNewDocument();
    if (docId == -1) {
        return -1;
    }

    SciEdit *editor = createEditor(docId);

    const int eolMode = AppSettings::newDocumentEolMode();
    editor->setEOLMode(eolMode);
    m_documentManager->setDocumentEolMode(docId, eolMode);

    m_viewManager->addEditor(docId, editor, activate, placement);
    editor->requestUrlScan();

    return docId;
}

int MainWindow::openFileInternal(const QString &filePath, bool activate, bool skipLexerApply)
{
    const QString normalizedPath = DocumentManager::normalizeFilePath(filePath);

    const int existingDocId = m_documentManager->findDocumentByFilePath(filePath);
    if (existingDocId >= 0) {
        m_closedTabHistory.removeOne(normalizedPath);
        if (activate) {
            m_viewManager->switchToDocId(existingDocId);
        }
        return existingDocId;
    }

    int docId = m_documentManager->openDocument(normalizedPath);
    if (docId == -1) {
        return -1;
    }

    SciEdit *editor = createEditor(docId);

    if (!skipLexerApply) {
        const QString lexerName = m_documentManager->getDocumentLanguage(docId);
        Document *doc = m_documentManager->getDocument(docId);
        DocumentManager::applyLexerToEditor(editor, lexerName, m_documentManager, doc && doc->isLargeFile);
    }

    m_viewManager->addEditor(docId, editor, activate);
    m_closedTabHistory.removeOne(normalizedPath);

    return docId;
}

void MainWindow::restoreRecentlyClosedFile()
{
    while (!m_closedTabHistory.isEmpty()) {
        const QString filePath = m_closedTabHistory.takeFirst();
        if (QFileInfo::exists(filePath)) {
            openFileInternal(filePath);
            updateActions();
            return;
        }
    }
}

void MainWindow::newFile()
{
    const NewTabPlacement placement = static_cast<NewTabPlacement>(AppSettings::newTabPlacement());
    newFileInternal(placement);
    updateActions();
}

void MainWindow::openFile()
{
    QFileDialog::Options options;
    if (!AppSettings::useNativeFileDialog()) {
        options |= QFileDialog::DontUseNativeDialog;
    }

    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const int currentDocId = m_viewManager->currentDocumentId();
    const QString currentPath = m_documentManager->getDocumentFilePath(currentDocId);
    if (!currentPath.isEmpty()) {
        const QString currentDir = QFileInfo(currentPath).absolutePath();
        if (QDir(currentDir).exists()) {
            defaultDir = currentDir;
        }
    }

    QString fileName = QFileDialog::getOpenFileName(this, 
        tr("Open File"), 
        defaultDir, 
        tr("All Files (*.*)"),
        nullptr,
        options);
    
    if (!fileName.isEmpty()) {
        const int docId = openFileInternal(fileName);
        if (docId >= 0) {
            addRecentFile(fileName);
        }
    }
}

void MainWindow::saveFile()
{
    const int docId = m_viewManager->currentDocumentId();
    if (docId < 0) {
        return;
    }

    const bool saved = saveDocument(docId);
    if (!saved) {
        return;
    }

    const QString filePath = m_documentManager->getDocumentFilePath(docId);
    if (!filePath.isEmpty()) {
        addRecentFile(filePath);
    }
}

void MainWindow::saveAsFile()
{
    const int docId = m_viewManager->currentDocumentId();
    if (docId < 0) {
        return;
    }

    const bool saved = saveDocumentAs(docId);
    if (!saved) {
        return;
    }

    const QString filePath = m_documentManager->getDocumentFilePath(docId);
    if (!filePath.isEmpty()) {
        addRecentFile(filePath);
    }
}

void MainWindow::exportAsRtf()
{
    const int docId = m_viewManager->currentDocumentId();
    if (docId < 0) {
        return;
    }
    SciEdit *editor = m_viewManager->editorForDocId(docId);
    if (!editor) {
        return;
    }

    RtfExporter exporter(editor);
    exportCurrentDocument(&exporter, tr("RTF Files (*.rtf)"), u".rtf"_s);
}

void MainWindow::exportAsHtml()
{
    const int docId = m_viewManager->currentDocumentId();
    if (docId < 0) {
        return;
    }
    SciEdit *editor = m_viewManager->editorForDocId(docId);
    if (!editor) {
        return;
    }

    HtmlExporter exporter(editor, m_documentManager->getDocumentTitle(docId));
    exportCurrentDocument(&exporter, tr("HTML Files (*.html *.htm)"), u".html"_s);
}

bool MainWindow::exportCurrentDocument(Exporter *exporter, const QString &nameFilter, const QString &defaultSuffix)
{
    const int docId = m_viewManager->currentDocumentId();
    if (docId < 0 || !exporter) {
        return false;
    }
    SciEdit *editor = m_viewManager->editorForDocId(docId);
    if (!editor) {
        return false;
    }

    QString suggestedName;
    const QString docPath = m_documentManager->getDocumentFilePath(docId);
    if (!docPath.isEmpty()) {
        const QFileInfo fi(docPath);
        suggestedName = fi.dir().filePath(fi.completeBaseName() + defaultSuffix);
    } else {
        suggestedName = m_documentManager->getDocumentTitle(docId) + defaultSuffix;
    }

    QFileDialog::Options options;
    if (!AppSettings::useNativeFileDialog()) {
        options |= QFileDialog::DontUseNativeDialog;
    }

    QString selectedFilter = nameFilter;
    const QString fileName = QFileDialog::getSaveFileName(this,
        tr("Export As"),
        suggestedName,
        nameFilter + u";;"_s + tr("All Files (*)"),
        &selectedFilter,
        options);

    if (fileName.isEmpty()) {
        return false;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Export Error"),
            tr("Failed to export to:\n%1").arg(fileName));
        return false;
    }

    {
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        exporter->convert(stream);
        stream.flush();
    }

    const bool writeFailed = (file.error() != QFileDevice::NoError);
    file.close();

    if (writeFailed) {
        QMessageBox::warning(this, tr("Export Error"),
            tr("Failed to export to:\n%1").arg(fileName));
        return false;
    }

    return true;
}

void MainWindow::copyAsHtml()
{
    copySelectionAs(false, true, false);
}

void MainWindow::copyAsRtf()
{
    copySelectionAs(false, false, true);
}

void MainWindow::copyAsAllFormats()
{
    copySelectionAs(true, true, true);
}

void MainWindow::copySelectionAs(bool includePlain, bool includeHtml, bool includeRtf)
{
    const int docId = m_viewManager->currentDocumentId();
    if (docId < 0) {
        return;
    }
    SciEdit *editor = m_viewManager->editorForDocId(docId);
    if (!editor) {
        return;
    }

    // Copy the selection if there is one, otherwise the whole document.
    const bool hasSelection = !editor->selectionEmpty();
    const int start = hasSelection ? static_cast<int>(editor->selectionStart()) : 0;
    const int end = hasSelection ? static_cast<int>(editor->selectionEnd()) : static_cast<int>(editor->length());

    auto convertRange = [editor, start, end](Exporter *exporter) {
        QByteArray buffer;
        QTextStream stream(&buffer);
        stream.setEncoding(QStringConverter::Utf8);
        exporter->convertRange(stream, start, end);
        stream.flush();
        return buffer;
    };

    auto *mimeData = new QMimeData;

    if (includePlain) {
        mimeData->setText(QString::fromUtf8(editor->textRange(start, end)));
    }
    if (includeHtml) {
        HtmlExporter htmlExporter(editor, m_documentManager->getDocumentTitle(docId));
        mimeData->setData(u"text/html"_s, convertRange(&htmlExporter));
    }
    if (includeRtf) {
        RtfExporter rtfExporter(editor);
        const QByteArray rtf = convertRange(&rtfExporter);
        mimeData->setData(u"text/rtf"_s, rtf);
        // Qt does not map text/rtf to the Windows clipboard format, so also
        // provide the format name that Windows RTF consumers look for.
        mimeData->setData(u"Rich Text Format"_s, rtf);
    }

    QApplication::clipboard()->setMimeData(mimeData);
}

void MainWindow::saveAllFiles()
{
    saveAllDocuments();
}

void MainWindow::closeFile()
{
    const int docId = m_viewManager->currentDocumentId();
    if (docId >= 0) {
        closeDocument(docId);
    }
}

void MainWindow::closeAllFiles()
{
    closeAllDocuments();
}

void MainWindow::showCharacterSelector()
{
    if (m_charSelectDialog) {
        m_charSelectDialog->raise();
        m_charSelectDialog->activateWindow();
        return;
    }
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Character Selector"));
    dialog->setWindowIcon(QIcon::fromTheme("accessories-character-map"));
    auto *layout = new QVBoxLayout(dialog);
    auto *selector = new KCharSelect(dialog, dialog);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    auto *insertBtn = new QPushButton(tr("Insert"), dialog);
    buttons->addButton(insertBtn, QDialogButtonBox::ActionRole);
    layout->addWidget(selector);
    layout->addWidget(buttons);
    selector->setCurrentFont(AppSettings::editorFont());
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    auto codePointToString = [](uint cp) {
        if (QChar::requiresSurrogates(cp)) {
            char32_t ucs4[1] = { static_cast<char32_t>(cp) };
            return QString::fromUcs4(ucs4, 1);
        }
        return QString(QChar(ushort(cp)));
    };
    auto insertText = [this](const QString &s){
        SciEdit *editor = m_viewManager->currentEditor();
        if (editor && !editor->readOnly() && !s.isEmpty()) {
            const QByteArray utf8 = s.toUtf8();
            editor->replaceSel(utf8.constData());
            editor->setFocus(true);
        }
    };
    connect(selector, &KCharSelect::charSelected, this, [insertText](const QChar &ch){
        insertText(QString(ch));
    });
    connect(insertBtn, &QPushButton::clicked, this, [selector, insertText, codePointToString](){
        const uint cp = selector->currentCodePoint();
        insertText(codePointToString(cp));
    });
    connect(dialog, &QObject::destroyed, this, [this](){
        m_charSelectDialog = nullptr;
    });
    m_charSelectDialog = dialog;
    dialog->show();
}

void MainWindow::onContextMenuRequested(SciEdit *editor, int docId)
{
    if (docId < 0 || !editor) {
        return;
    }
    QMenu *menu = buildDocumentContextMenu(docId, editor, this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->exec(QCursor::pos());
}

QMenu *MainWindow::buildDocumentContextMenu(int docId, SciEdit *editor, QWidget *parent)
{
    const Document *doc = m_documentManager->getDocument(docId);
    const bool hasDoc = (doc != nullptr);
    const bool isMonitored = hasDoc && doc->isMonitoringOn;
    const bool isLocalFile = hasDoc && !doc->untitled && !doc->filePath.isEmpty();

    auto *menu = new QMenu(parent);

    auto *closeAction = menu->addAction(QIcon::fromTheme(u"document-close"_s), tr("Close"));
    QAction *closeOthersAction = nullptr;
    QAction *closeLeftAction = nullptr;
    QAction *closeRightAction = nullptr;
    if (editor) {
        closeOthersAction = menu->addAction(tr("Close Others"));
        closeLeftAction = menu->addAction(tr("Close to the Left"));
        closeRightAction = menu->addAction(tr("Close to the Right"));
    }
    menu->addSeparator();
    auto *saveAction = menu->addAction(QIcon::fromTheme(u"document-save"_s), tr("Save"));
    auto *saveAsAction = menu->addAction(QIcon::fromTheme(u"document-save-as"_s), tr("Save As..."));
    auto *reloadAction = menu->addAction(QIcon::fromTheme(u"view-refresh"_s), tr("Reload"));
    menu->addSeparator();
    auto *readOnlyAction = menu->addAction(QIcon::fromTheme(u"object-locked"_s, QIcon::fromTheme(u"lock"_s)), tr("Set Read-Only"));
    readOnlyAction->setCheckable(true);
    readOnlyAction->setChecked(hasDoc && doc->readOnly);
    menu->addSeparator();
    auto *copyMenu = menu->addMenu(ki18n("Copy to Clipboard").toString());
    copyMenu->setIcon(QIcon::fromTheme(u"edit-copy"_s));
    auto *copyFullPath = copyMenu->addAction(tr("Full Path"));
    auto *copyFileName = copyMenu->addAction(tr("File Name"));
    auto *copyDirPath = copyMenu->addAction(tr("Directory Path"));

    reloadAction->setEnabled(isLocalFile);
    readOnlyAction->setEnabled(!isMonitored);
    copyFullPath->setEnabled(isLocalFile);
    copyFileName->setEnabled(isLocalFile);
    copyDirPath->setEnabled(isLocalFile);

    QObject::connect(closeAction, &QAction::triggered, menu, [this, docId] { closeDocument(docId); });
    if (closeOthersAction) {
        QObject::connect(closeOthersAction, &QAction::triggered, menu, [this, editor] {
            const QVector<SciEdit*> groupEditors = m_viewManager->editorsInSameTabGroup(editor);
            const int idx = groupEditors.indexOf(editor);
            for (int i = groupEditors.size() - 1; i >= 0; --i) {
                if (i == idx) continue;
                const int id = m_viewManager->docIdForEditor(groupEditors[i]);
                if (id >= 0 && !closeDocument(id, false)) return;
            }
        });
    }
    if (closeLeftAction) {
        QObject::connect(closeLeftAction, &QAction::triggered, menu, [this, editor] {
            const QVector<SciEdit*> groupEditors = m_viewManager->editorsInSameTabGroup(editor);
            const int idx = groupEditors.indexOf(editor);
            for (int i = idx - 1; i >= 0; --i) {
                const int id = m_viewManager->docIdForEditor(groupEditors[i]);
                if (id >= 0 && !closeDocument(id, false)) return;
            }
        });
    }
    if (closeRightAction) {
        QObject::connect(closeRightAction, &QAction::triggered, menu, [this, editor] {
            const QVector<SciEdit*> groupEditors = m_viewManager->editorsInSameTabGroup(editor);
            const int idx = groupEditors.indexOf(editor);
            for (int i = groupEditors.size() - 1; i > idx; --i) {
                const int id = m_viewManager->docIdForEditor(groupEditors[i]);
                if (id >= 0 && !closeDocument(id, false)) return;
            }
        });
    }
    QObject::connect(saveAction, &QAction::triggered, menu, [this, docId] {
        if (saveDocument(docId)) {
            m_viewManager->refreshEditorsForDoc(docId);
        }
    });
    QObject::connect(saveAsAction, &QAction::triggered, menu, [this, docId] {
        if (saveDocumentAs(docId)) {
            m_viewManager->refreshEditorsForDoc(docId);
        }
    });
    QObject::connect(reloadAction, &QAction::triggered, menu, [this, docId] {
        if (m_documentManager->isDocumentUntitled(docId) || m_documentManager->getDocumentFilePath(docId).isEmpty()) {
            return;
        }
        const QString title = tr("Reload Document");
        const QString docTitle = m_documentManager->getDocumentTitle(docId);
        const QString modifiedMsg = tr("The document '%1' has been modified.").arg(docTitle) % '\n' %
                                    tr("Reloading will discard your unsaved changes.") % "\n\n" %
                                    tr("Reload now?");
        m_fileChangeWatcher->promptAndReloadDocument(docId, false, title, modifiedMsg, QString(), false);
    });
    QObject::connect(readOnlyAction, &QAction::triggered, menu, [this, docId, readOnlyAction] {
        m_documentManager->setDocumentReadOnly(docId, readOnlyAction->isChecked());
    });
    QObject::connect(copyFullPath, &QAction::triggered, menu, [this, docId] {
        QApplication::clipboard()->setText(m_documentManager->getDocumentFilePath(docId));
    });
    QObject::connect(copyFileName, &QAction::triggered, menu, [this, docId] {
        const QFileInfo fi(m_documentManager->getDocumentFilePath(docId));
        QApplication::clipboard()->setText(fi.fileName());
    });
    QObject::connect(copyDirPath, &QAction::triggered, menu, [this, docId] {
        const QFileInfo fi(m_documentManager->getDocumentFilePath(docId));
        QApplication::clipboard()->setText(fi.absolutePath());
    });

    return menu;
}

void MainWindow::toggleMonitoring(bool checked)
{
    const int docId = m_viewManager->currentDocumentId();
    if (docId < 0) {
        return;
    }

    if (checked) {
        if (!m_fileChangeWatcher->setMonitoring(docId, true)) {
            if (QAction *a = actionCollection()->action(QStringLiteral("toggle_monitoring"))) {
                QSignalBlocker blocker(a);
                a->setChecked(false);
            }
        }
    } else {
        m_fileChangeWatcher->setMonitoring(docId, false);
    }
}

void MainWindow::openCurrentDocumentWithEncoding(const QString &codecName, bool withBom, const QString &displayName)
{
    SciEdit *editor = m_viewManager->currentEditor();
    if (!editor) {
        return;
    }

    const int docId = m_viewManager->currentDocumentId();
    if (docId < 0) {
        return;
    }

    const bool isUntitled = m_documentManager->isDocumentUntitled(docId);
    const bool isModified = m_documentManager->isDocumentModified(docId);

    if (isUntitled) {
        if (!saveDocument(docId)) {
            return;
        }
    } else if (isModified) {
        const QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Reload Document"),
            tr("The document has unsaved changes.\nDo you want to save before reloading with a different encoding?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Cancel) {
            return;
        }
        if (reply == QMessageBox::Save) {
            if (!saveDocument(docId)) {
                return;
            }
        }
    }

    const QString filePath = m_documentManager->getDocumentFilePath(docId);
    if (filePath.isEmpty()) {
        return;
    }

    Document *doc = m_documentManager->getDocument(docId);
    if (!doc) {
        return;
    }

    const sptr_t oldPos = editor->currentPos();
    const sptr_t oldFirstVisible = editor->firstVisibleLine();
    const sptr_t oldXOffset = editor->xOffset();

    if (!doc->loadFromDiskInto(editor, codecName.toLatin1(), withBom, displayName)) {
        QMessageBox::warning(this, tr("Open With Encoding"), tr("Failed to reload file with the selected encoding."));
        return;
    }

    editor->gotoPos(oldPos);
    editor->setFirstVisibleLine(oldFirstVisible);
    editor->setXOffset(oldXOffset);
    editor->scrollCaret();

    m_documentManager->setDocumentModified(docId, false);
    m_documentManager->setDocumentEncodingModified(docId, false);

    updateStatusBar();
    updateWindowTitle();
}

void MainWindow::convertCurrentDocumentToEncoding(const QString &codecName, bool withBom, const QString &displayName)
{
    SciEdit *editor = m_viewManager->currentEditor();
    if (!editor) {
        return;
    }

    const int docId = m_viewManager->currentDocumentId();
    if (docId < 0) {
        return;
    }

    m_documentManager->setDocumentEncodingDetails(docId, displayName, codecName.toLatin1(), withBom);
    m_documentManager->setDocumentEncodingModified(docId, true);

    updateStatusBar();
}

void MainWindow::onCurrentEditorChanged(SciEdit *editor)
{
    qCDebug(lcMainWindow) << "Current editor changed to:" << editor;
    const int docId = editor ? m_viewManager->docIdForEditor(editor) : -1;
    if (docId >= 0)
        m_documentManager->touchDocument(docId);
    m_fileChangeWatcher->setCurrentDocument(docId);
    if (m_documentListDock)
        m_documentListDock->setCurrentDocId(docId);
    updateWindowTitle();
    updateStatusBar();
    updateActions();
    
    disconnect(m_cursorPosConnection);
    disconnect(m_textChangedConnection);
    if (editor) {
        applySettingsToEditor(editor);
        m_cursorPosConnection = connect(editor, &SciEdit::cursorPosChanged, this, [this](int, int) {
            SciEdit *current = m_viewManager->currentEditor();
            updateCursorStatusLabel(current);
            if (current) {
                m_overTypeStatusLabel->setText(current->overtype() ? QStringLiteral("OVR") : QStringLiteral("INS"));
            }
        });
        m_textChangedConnection = connect(editor, &SciEdit::textChanged, this, [this]() {
            updateDocumentStatsLabel(m_viewManager->currentEditor());
            updateEditActions();
        });
        updateCursorStatusLabel(editor);
        updateDocumentStatsLabel(editor);
        editor->setFocus(true);
    }
}

void MainWindow::applySettingsToEditor(SciEdit *editor)
{
    if (!editor) {
        return;
    }
    const int docId = m_viewManager->docIdForEditor(editor);
    if (docId >= 0) {
        editor->setTabWidth(m_documentManager->getDocumentIndentTabWidth(docId));
        editor->setIndentUseTabs(m_documentManager->getDocumentIndentUseTabs(docId));
    }
    editor->setEditorFont(AppSettings::editorFont());
    editor->setThemeId(AppSettings::editorTheme());
    editor->setIndentationGuides(AppSettings::indentGuidesEnabled() ? SC_IV_LOOKBOTH : SC_IV_NONE);
    editor->setEdgeColumn(AppSettings::verticalEdgeColumn());
    editor->setVerticalEdgeEnabled(AppSettings::verticalEdgeEnabled());
    editor->setLineNumbersEnabled(AppSettings::showLineNumbers());
    editor->setWrapMode(AppSettings::wordWrapEnabled() ? AppSettings::wrapMode() : SC_WRAP_NONE);
    editor->setWrapIndentMode(AppSettings::wrapIndentMode());
    editor->setWrapVisualFlags(AppSettings::wrapSymbolEnabled() ? SC_WRAPVISUALFLAG_END : SC_WRAPVISUALFLAG_NONE);
    editor->setAutoIndentMode(static_cast<SciEdit::AutoIndentMode>(AppSettings::autoIndentMode()));
    editor->setBackSpaceUnIndents(AppSettings::backspaceUnindent());
    editor->setMultiSelection(AppSettings::multiSelection());
    editor->setColumnSel2MultiEdit(AppSettings::columnSel2MultiEdit());
    if (AppSettings::changeHistoryEnabled()) {
        if (!editor->canUndo() && !editor->canRedo()) {
            editor->setChangeHistoryEnabled(true);
        }
    } else {
        editor->setChangeHistoryEnabled(false);
    }
    editor->setViewWS(AppSettings::showSpaceAndTab() ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE);
    editor->setViewEOL(AppSettings::showEndOfLine());
    editor->setViewNPC(AppSettings::showNonPrintingCharacters());
    editor->setSmartHighlightEnabled(AppSettings::smartHighlightEnabled());
    editor->setSmartHighlightMatchCase(AppSettings::smartHighlightMatchCase());
    editor->setSmartHighlightWholeWord(AppSettings::smartHighlightWholeWord());
    editor->setFindResultHighlightEnabled(AppSettings::findResultHighlightEnabled());
    editor->setTagMatchHighlightEnabled(AppSettings::tagMatchHighlightEnabled());
    editor->setTagMatchHighlightAttributes(AppSettings::tagMatchHighlightAttributes());
    editor->setAutoCompletionEnabled(AppSettings::autoCompletionEnabled());
    editor->setAutoCompletionTriggerLength(AppSettings::autoCompletionTriggerLength());
    editor->setAutoCompletionIgnoreNumbers(AppSettings::autoCompletionIgnoreNumbers());
    editor->setAutoCompletionBrief(AppSettings::autoCompletionBrief());
    editor->setEndAtLastLine(!AppSettings::scrollBeyondLastLine());
    editor->setUrlScanEnabled(AppSettings::urlScanEnabled());
    editor->setSpellCheckEnabled(m_documentManager->getDocumentSpellCheckEnabled(docId));
}

void MainWindow::applyAppFont()
{
    // No sane way to retrive system default font, so we need to manually
    // save it when necessary.
    static std::optional<QFont> s_appDefaultFont = {};

    if (AppSettings::overrideAppFont()) {
        s_appDefaultFont = QApplication::font();
        QApplication::setFont(AppSettings::appFont());
        qCDebug(lcMainWindow) << "Application font overrided to" << s_appDefaultFont;
    } else if (s_appDefaultFont) {
        QApplication::setFont(s_appDefaultFont.value());
        s_appDefaultFont = std::nullopt;
        qCDebug(lcMainWindow) << "Application font restored to" << s_appDefaultFont;
    }
}

void MainWindow::applySettingsToAllEditors()
{
    const QVector<SciEdit *> allEditors = m_viewManager->editors();
    for (SciEdit *editor : allEditors) {
        applySettingsToEditor(editor);
    }

    if (m_searchResultsDock) {
        m_searchResultsDock->applyEditorSettings();
    }
}

void MainWindow::onDocumentModified(int docIndex, bool modified)
{
    Q_UNUSED(docIndex)
    Q_UNUSED(modified)
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    QString title;

    if (SciEdit *editor = m_viewManager->currentEditor()) {
        Q_UNUSED(editor)
        int docId = m_viewManager->currentDocumentId();
        if (docId >= 0) {
            const QString filePath = m_documentManager->getDocumentFilePath(docId);
            QString fileName = filePath.isEmpty() ? m_documentManager->getDocumentTitle(docId) : QDir::toNativeSeparators(filePath);
            bool isModified = m_documentManager->isDocumentModified(docId);
            
            title = QStringLiteral("%1%2").arg(fileName, isModified ? QStringLiteral("*") : QString());
        }
    }
    
    setWindowTitle(title);
}

void MainWindow::updateCursorStatusLabel(SciEdit *editor)
{
    if (!editor) {
        m_cursorPosStatusLabel->setText(QStringLiteral("Ln: ?  Col: ?  Pos: ?"));
        return;
    }

    const sptr_t curPos = editor->currentPos();
    const int line = editor->lineFromPosition(curPos) + 1;
    const int column = editor->column(curPos) + 1;
    const sptr_t selStart = editor->selectionStart();
    const sptr_t selEnd = editor->selectionEnd();
    if (selStart != selEnd) {
        const sptr_t start = qMin(selStart, selEnd);
        const sptr_t end = qMax(selStart, selEnd);
        const sptr_t length = end - start;
        const int startLine = editor->lineFromPosition(start);
        const int endLine = editor->lineFromPosition(end);
        const int lineCount = endLine >= startLine ? (endLine - startLine + 1) : 1;
        m_cursorPosStatusLabel->setText(tr("Ln: %1  Col: %2  Sel: %3 | %4").arg(line).arg(column).arg(length).arg(lineCount));
        return;
    }

    const sptr_t pos = curPos + 1;
    m_cursorPosStatusLabel->setText(tr("Ln: %1  Col: %2  Pos: %3").arg(line).arg(column).arg(pos));
}

void MainWindow::updateDocumentStatsLabel(SciEdit *editor)
{
    if (!editor) {
        m_docStatsStatusLabel->setText(QStringLiteral("Len: ?  Lines: ?"));
        return;
    }

    const sptr_t length = editor->textLength();
    const sptr_t lines = editor->lineCount();
    m_docStatsStatusLabel->setText(tr("Len: %1  Lines: %2").arg(length).arg(lines));
}

void MainWindow::updateStatusBar()
{
    if (SciEdit *editor = m_viewManager->currentEditor()) {
        int docId = m_viewManager->currentDocumentId();
        if (docId >= 0) {
            QString encoding = m_documentManager->getDocumentEncoding(docId);
            const QString lexerName = m_documentManager->getDocumentLanguage(docId);
            const QString language = lexerName.isEmpty() ? QStringLiteral("Plain Text") : LanguageConfigStore::displayName(lexerName);
            
            m_encodingStatusLabel->setText(encoding);
            m_languageStatusLabel->setText(language);

            QString eol;
            if (m_documentManager->isDocumentEolMixed(docId)) {
                eol = tr("Mixed");
            } else {
                switch (editor->eOLMode()) {
                case SC_EOL_CRLF:
                    eol = tr("Windows (CR LF)");
                    break;
                case SC_EOL_LF:
                    eol = tr("Unix (LF)");
                    break;
                case SC_EOL_CR:
                    eol = tr("Macintosh (CR)");
                    break;
                default:
                    eol = QStringLiteral("?");
                    break;
                }
            }
            m_eolStatusLabel->setText(eol);

            const bool useTabs = m_documentManager->getDocumentIndentUseTabs(docId);
            const int indentWidth = m_documentManager->getDocumentIndentTabWidth(docId);
            m_indentStatusLabel->setText(useTabs ? tr("Tabs: %1").arg(indentWidth)
                                                 : tr("Spaces: %1").arg(indentWidth));

            const bool mixed = m_documentManager->isDocumentEolMixed(docId);
            const int mode = static_cast<int>(editor->eOLMode());
            if (QAction *a = actionCollection()->action(QStringLiteral("eol_convert_to_crlf"))) {
                QSignalBlocker blocker(a);
                a->setChecked(!mixed && mode == SC_EOL_CRLF);
            }
            if (QAction *a = actionCollection()->action(QStringLiteral("eol_convert_to_lf"))) {
                QSignalBlocker blocker(a);
                a->setChecked(!mixed && mode == SC_EOL_LF);
            }
            if (QAction *a = actionCollection()->action(QStringLiteral("eol_convert_to_cr"))) {
                QSignalBlocker blocker(a);
                a->setChecked(!mixed && mode == SC_EOL_CR);
            }
        }
        updateCursorStatusLabel(editor);
        updateDocumentStatsLabel(editor);
        m_overTypeStatusLabel->setText(editor->overtype() ? QStringLiteral("OVR") : QStringLiteral("INS"));
    } else {
        m_encodingStatusLabel->setText("UTF-8");
        m_languageStatusLabel->setText("Plain Text");
        m_eolStatusLabel->setText(tr("Unix (LF)"));
        m_indentStatusLabel->setText(tr("Spaces: %1").arg(AppSettings::tabWidth()));
        updateCursorStatusLabel(nullptr);
        updateDocumentStatsLabel(nullptr);
        m_overTypeStatusLabel->setText(QStringLiteral("INS"));
    }
}

void MainWindow::updateEditActions()
{
    SciEdit *currentEditor = m_viewManager->currentEditor();
    const bool hasEditor = (currentEditor != nullptr);
    const bool isReadOnly = hasEditor ? currentEditor->readOnly() : true;

    if (QAction *a = actionCollection()->action(KStandardAction::name(KStandardAction::Undo))) {
        a->setEnabled(hasEditor && currentEditor->canUndo());
    }
    if (QAction *a = actionCollection()->action(KStandardAction::name(KStandardAction::Redo))) {
        a->setEnabled(hasEditor && currentEditor->canRedo());
    }
    for (const auto &standardAction : {KStandardAction::Cut, KStandardAction::Copy}) {
        if (QAction *a = actionCollection()->action(KStandardAction::name(standardAction))) {
            a->setEnabled(hasEditor);
        }
    }
    if (QAction *a = actionCollection()->action(KStandardAction::name(KStandardAction::Paste))) {
        a->setEnabled(hasEditor && currentEditor->canPaste());
    }
    if (QAction *a = actionCollection()->action(u"edit_delete"_s)) {
        a->setEnabled(hasEditor && !isReadOnly);
    }
}

void MainWindow::updateActions()
{
    SciEdit *currentEditor = m_viewManager->currentEditor();
    const bool hasEditor = (currentEditor != nullptr);
    const bool isReadOnly = hasEditor ? currentEditor->readOnly() : true;
    const bool isRecordingMacro = m_macroRecorder && m_macroRecorder->isRecording();
    const bool hasRecordedMacro = m_currentMacro && !m_currentMacro->isEmpty();
    const bool hasAnyMacro = hasRecordedMacro || !m_savedMacros.empty();

    updateEditActions();

    // Begin/End Select: check the active variant, disable the other (mutual exclusion)
    const bool beginActive = hasEditor && currentEditor->isBeginEndSelectActive();
    const bool beginColumn = hasEditor && currentEditor->isBeginEndSelectColumnMode();
    if (QAction *a = actionCollection()->action(u"edit_begin_end_select"_s)) {
        a->setEnabled(hasEditor && !(beginActive && beginColumn));
        a->setChecked(beginActive && !beginColumn);
    }
    if (QAction *a = actionCollection()->action(u"edit_begin_end_select_column_mode"_s)) {
        a->setEnabled(hasEditor && !(beginActive && !beginColumn));
        a->setChecked(beginActive && beginColumn);
    }

    // Column Selection Mode toggle: per-editor state, reflects the current editor.
    if (QAction *a = actionCollection()->action(u"edit_column_select_mode"_s)) {
        a->setEnabled(hasEditor);
        a->setChecked(hasEditor && currentEditor->columnSelectMode());
    }

    // Actions that only need an editor
    for (const auto &name : {
        "fold_all"_L1, "unfold_all"_L1, "fold_current_level"_L1,
        "unfold_current_level"_L1, "summary"_L1, "go_to"_L1,
        "file_export_rtf"_L1, "file_export_html"_L1,
        "copy_as_html"_L1, "copy_as_rtf"_L1, "copy_all_formats"_L1,
    }) {
        QAction *a = actionCollection()->action(name);
        Q_CHECK_PTR(a);
        a->setEnabled(hasEditor);
    }

    // Actions that need an editor and must not be read-only
    for (const auto &name : {
        "character_selector"_L1, "trim_trailing_space"_L1, "trim_leading_space"_L1,
        "trim_trailing_leading_space"_L1, "tab_to_space_all"_L1, "tab_to_space_leading"_L1,
        "space_to_tab_all"_L1, "space_to_tab_leading"_L1, "eol_to_space"_L1,
        "duplicate_current_line"_L1, "remove_consecutive_dup_lines"_L1,
        "remove_any_dup_lines"_L1, "split_lines"_L1, "join_lines"_L1,
        "move_line_up"_L1, "move_line_down"_L1, "remove_empty_lines"_L1,
        "remove_empty_lines_with_blank"_L1, "insert_empty_line_above"_L1,
        "insert_empty_line_below"_L1, "reverse_lines"_L1, "randomize_lines"_L1,
        "sort_lines_asc"_L1, "sort_lines_desc"_L1, "increase_indent"_L1,
        "decrease_indent"_L1, "eol_convert_to_crlf"_L1, "eol_convert_to_lf"_L1,
        "eol_convert_to_cr"_L1,
        "convert_to_upper_case"_L1, "convert_to_lower_case"_L1,
        "convert_to_proper_case"_L1, "convert_to_proper_case_blend"_L1,
        "convert_to_sentence_case"_L1, "convert_to_sentence_case_blend"_L1,
        "convert_to_invert_case"_L1, "convert_to_random_case"_L1,
        "tools_base64_encode"_L1, "tools_base64_encode_pad"_L1,
        "tools_base64_encode_eol"_L1, "tools_base64_encode_byline"_L1,
        "tools_base64_decode"_L1, "tools_base64_decode_strict"_L1,
        "tools_base64_decode_byline"_L1, "tools_url_encode_rfc1738"_L1,
        "tools_url_encode_rfc1738_byline"_L1, "tools_url_encode_extended"_L1,
        "tools_url_encode_extended_byline"_L1, "tools_url_encode_full"_L1,
        "tools_url_encode_full_byline"_L1, "tools_url_decode"_L1,
        "tools_hex_encode"_L1, "tools_hex_encode_space"_L1, "tools_hex_decode"_L1,
        "bookmark_cut_lines"_L1, "bookmark_paste_to_lines"_L1,
        "bookmark_remove_lines"_L1, "bookmark_remove_unmarked_lines"_L1,
    }) {
        QAction *a = actionCollection()->action(name);
        Q_CHECK_PTR(a);
        a->setEnabled(hasEditor && !isReadOnly);
    }

    // Toggle actions: enabled when hasEditor, checked tracks a setting
    struct ToggleEntry { QLatin1StringView name; bool checked; };
    for (const auto &[name, checked] : {
        ToggleEntry{"toggle_indent_guide"_L1, AppSettings::indentGuidesEnabled()},
        ToggleEntry{"toggle_wrap_mode"_L1, AppSettings::wordWrapEnabled()},
        ToggleEntry{"toggle_wrap_symbol"_L1, AppSettings::wrapSymbolEnabled()},
        ToggleEntry{"toggle_show_space_and_tab"_L1, AppSettings::showSpaceAndTab()},
        ToggleEntry{"toggle_show_end_of_line"_L1, AppSettings::showEndOfLine()},
        ToggleEntry{"toggle_show_non_printing_characters"_L1, AppSettings::showNonPrintingCharacters()},
    }) {
        QAction *a = actionCollection()->action(name);
        Q_CHECK_PTR(a);
        a->setEnabled(hasEditor);
        a->setChecked(checked);
    }

    // Special cases
    if (QAction *a = actionCollection()->action(QStringLiteral("toggle_show_all_characters"))) {
        a->setEnabled(hasEditor);
        a->setChecked(AppSettings::showSpaceAndTab()
            && AppSettings::showEndOfLine()
            && AppSettings::showNonPrintingCharacters());
    }

    if (QAction *a = actionCollection()->action(u"toggle_spell_check"_s)) {
        a->setEnabled(hasEditor);
        a->setChecked(hasEditor && currentEditor->spellCheckEnabled());
    }

    if (QAction *readOnlyAction = actionCollection()->action(QStringLiteral("toggle_read_only"))) {
        const int docId = m_viewManager->currentDocumentId();
        readOnlyAction->setEnabled(hasEditor && !m_fileChangeWatcher->isDocumentMonitored(docId));
        readOnlyAction->setChecked(hasEditor ? isReadOnly : false);
    }

    if (QAction *a = actionCollection()->action(QStringLiteral("toggle_monitoring"))) {
        const int docId = m_viewManager->currentDocumentId();
        const bool canMonitor = hasEditor && docId >= 0 && !m_documentManager->isDocumentUntitled(docId) && !m_documentManager->getDocumentFilePath(docId).isEmpty();
        a->setEnabled(canMonitor);
        a->setChecked(docId >= 0 && m_fileChangeWatcher->isDocumentMonitored(docId));
    }

    if (m_favoriteToggleAction) {
        const int docId = m_viewManager->currentDocumentId();
        const QString path = (docId >= 0 && !m_documentManager->isDocumentUntitled(docId))
            ? m_documentManager->getDocumentFilePath(docId)
            : QString();
        const bool favored = !path.isEmpty() && m_favoritesStore->isFavorite(path);
        // Keep the action clickable for untitled documents so the toolbar popup
        // stays reachable; toggleFavorite() explains why it cannot be favorited.
        m_favoriteToggleAction->setEnabled(hasEditor && docId >= 0);
        m_favoriteToggleAction->setText(tr("Toggle Favorites"));
        m_favoriteToggleAction->setIcon(QIcon::fromTheme(favored ? u"favorite-favorited"_s : u"favorite"_s));
    }

    // Macro actions: each has a unique enable condition
    struct MacroEntry { QLatin1StringView name; bool enabled; };
    for (const auto &[name, enabled] : {
        MacroEntry{"macro_record_start"_L1, hasEditor && !isRecordingMacro},
        MacroEntry{"macro_record_stop"_L1, isRecordingMacro},
        MacroEntry{"macro_play"_L1, hasEditor && hasRecordedMacro && !isRecordingMacro},
        MacroEntry{"macro_repeat"_L1, hasEditor && hasAnyMacro && !isRecordingMacro},
        MacroEntry{"macro_save"_L1, hasRecordedMacro && !isRecordingMacro},
    }) {
        QAction *a = actionCollection()->action(name);
        Q_CHECK_PTR(a);
        a->setEnabled(enabled);
    }
}

void MainWindow::showFindReplaceDialog(bool showReplace)
{
    m_findReplaceController->showDialog(showReplace);
}

bool MainWindow::findNextInCurrentEditor(bool forward, bool showErrorMessage)
{
    return m_findReplaceController->findNext(forward, showErrorMessage);
}

bool MainWindow::replaceOneInCurrentEditor(bool showErrorMessage)
{
    return m_findReplaceController->replaceOne(showErrorMessage);
}

int MainWindow::replaceAllInCurrentEditor(bool showErrorMessage)
{
    return m_findReplaceController->replaceAll(showErrorMessage);
}

void MainWindow::loadSavedMacros()
{
    const KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("SavedMacros"));
    const int count = group.readEntry(QStringLiteral("count"), 0);
    for (int i = 0; i < count; ++i) {
        const QByteArray data = group.readEntry(QStringLiteral("macro_%1").arg(i), QByteArray());
        auto macro = Macro::deserialize(data);
        if (macro && !macro->isEmpty()) {
            m_savedMacros.push_back(std::move(macro));
        }
    }
}

void MainWindow::saveSavedMacros()
{
    KSharedConfig::openConfig()->deleteGroup(QStringLiteral("SavedMacros"));
    KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("SavedMacros"));

    group.writeEntry(QStringLiteral("count"), static_cast<int>(m_savedMacros.size()));
    for (int i = 0; i < static_cast<int>(m_savedMacros.size()); ++i) {
        group.writeEntry(QStringLiteral("macro_%1").arg(i), m_savedMacros[i]->serialize());
    }
    group.sync();
}

void MainWindow::updateSavedMacroActions()
{
    qDeleteAll(m_savedMacroActions);
    m_savedMacroActions.clear();

    for (int i = 0; i < static_cast<int>(m_savedMacros.size()); ++i) {
        const auto &macro = m_savedMacros[i];
        auto *action = new QAction(macro->name(), this);
        if (!macro->shortcut().isEmpty()) {
            action->setShortcut(macro->shortcut());
        }
        connect(action, &QAction::triggered, this, [this, macroPtr = macro.get()]() {
            SciEdit *editor = m_viewManager->currentEditor();
            if (editor) {
                macroPtr->replay(editor, 1);
            }
        });
        m_savedMacroActions.append(action);
    }

    unplugActionList(QStringLiteral("saved_macro_list"));
    plugActionList(QStringLiteral("saved_macro_list"), m_savedMacroActions);
}

void MainWindow::toggleFavorite()
{
    const int docId = m_viewManager->currentDocumentId();
    if (docId < 0) {
        return;
    }
    const QString path = m_documentManager->isDocumentUntitled(docId)
        ? QString()
        : m_documentManager->getDocumentFilePath(docId);
    if (path.isEmpty()) {
        QMessageBox::information(this, tr("Toggle Favorite"),
            tr("This document is not a local file and cannot be added to favorites."));
        return;
    }
    m_favoritesStore->toggle(path);
    updateActions();
}

void MainWindow::openManageFavoritesDialog()
{
    FavoritesManageDialog dialog(m_favoritesStore, this);
    connect(&dialog, &FavoritesManageDialog::openRequested, this, &MainWindow::openFiles);
    dialog.exec();
    updateActions();
}

void MainWindow::rebuildFavoriteActions()
{
    qDeleteAll(m_favoriteEntryActions);
    m_favoriteEntryActions.clear();

    const QStringList entries = m_favoritesStore->entries();
    for (const QString &path : entries) {
        auto *action = new QAction(QFileInfo(path).fileName(), this);
        action->setToolTip(path);
        action->setStatusTip(path);
        connect(action, &QAction::triggered, this, [this, path]() {
            openFiles(QStringList{path});
        });
        m_favoriteEntryActions.append(action);
    }

    unplugActionList(u"favorites_list"_s);
    plugActionList(u"favorites_list"_s, m_favoriteEntryActions);

    QMenu *favoritesMenu = m_favoriteToggleAction->popupMenu();
    favoritesMenu->clear();
    for (QAction *action : std::as_const(m_favoriteEntryActions)) {
        favoritesMenu->addAction(action);
    }
    if (!m_favoriteEntryActions.isEmpty()) {
        favoritesMenu->addSeparator();
    }
    if (QAction *manageAction = actionCollection()->action(u"favorites_manage"_s)) {
        favoritesMenu->addAction(manageAction);
    }
}
