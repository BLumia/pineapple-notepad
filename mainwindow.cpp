#include "mainwindow.h"

#include "appsettings.h"

#include <QApplication>
#include <QMenu>

#include <KActionCollection>
#include <KStandardAction>
#include <KActionMenu>
#include <KConfigDialog>
#include <KColorSchemeManager>
#include <KColorSchemeMenu>
#include <QSpinBox>

#include <Qsci/qsciscintilla.h>

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent)
    , m_editor(new QsciScintilla(this))
{
    setCentralWidget(m_editor);

    QFont font(AppSettings::self()->editorFont());
    m_editor->setFont(font);
    m_editor->setMarginsFont(font);

    m_editor->setMarginLineNumbers(1, true);
    m_editor->setMarginWidth(1, 40);
    m_editor->setBraceMatching(QsciScintilla::SloppyBraceMatch);

    m_editor->setTabWidth(AppSettings::self()->tabWidth());
    m_editor->setEolMode(QsciScintilla::EolUnix);

    setupActions();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupActions()
{
    using namespace Qt::Literals::StringLiterals;

    // "File" menu
    KStandardAction::openNew(this, [](){}, actionCollection());
    KStandardAction::open(this, [](){}, actionCollection());
    KStandardAction::save(this, [](){}, actionCollection());
    KStandardAction::close(this, [](){}, actionCollection());
    KStandardAction::quit(qApp, &QApplication::quit, actionCollection());

    // "Edit" menu
    KStandardAction::undo(this, [this](){
        m_editor->undo();
    }, actionCollection());
    KStandardAction::redo(this, [this](){
        m_editor->redo();
    }, actionCollection());
    KStandardAction::cut(this, [this](){
        m_editor->cut();
    }, actionCollection());
    KStandardAction::copy(this, [this](){
        m_editor->copy();
    }, actionCollection());
    KStandardAction::paste(this, [this](){
        m_editor->paste();
    }, actionCollection());

    // "Search" menu
    KStandardAction::find(this, [](){}, actionCollection());
    KStandardAction::findNext(this, [](){}, actionCollection());
    KStandardAction::findPrev(this, [](){}, actionCollection());
    KStandardAction::replace(this, [](){}, actionCollection());
    qDebug() << KStandardAction::name(KStandardAction::Replace);
    
    // "Language" menu
    QAction *lexerNoneAction = new QAction(this);
    lexerNoneAction->setText("None (Normal Text)");
    actionCollection()->addAction(u"lexer_none"_s, lexerNoneAction);
    connect(lexerNoneAction, &QAction::triggered, this, [this](){
        m_editor->setLexer(nullptr);
    });

    // Toolbar actions
    KStandardAction::zoomIn(this, [this](){
        m_editor->zoomIn();
    }, actionCollection());
    KStandardAction::zoomOut(this, [this](){
        m_editor->zoomOut();
    }, actionCollection());

    KStandardAction::preferences(this, &MainWindow::showSettings, actionCollection());

    QAction *toggleWrapModeAction = new QAction(this);
    toggleWrapModeAction->setText("Toggle Wrap Mode");
    toggleWrapModeAction->setIcon(QIcon::fromTheme(u"text-wrap"_s));
    toggleWrapModeAction->setCheckable(true);
    actionCollection()->addAction(u"toggle_wrap_mode"_s, toggleWrapModeAction);
    connect(toggleWrapModeAction, &QAction::triggered, this, [this, toggleWrapModeAction](){
        bool switchToWrapNone = m_editor->wrapMode() == QsciScintilla::WrapWord;
        m_editor->setWrapMode(switchToWrapNone ? QsciScintilla::WrapNone : QsciScintilla::WrapWord);
        toggleWrapModeAction->setChecked(!switchToWrapNone);
    });

    QAction *toggleWhitespaceVisibilityAction = new QAction(this);
    toggleWhitespaceVisibilityAction->setText("Show All Characters");
    toggleWhitespaceVisibilityAction->setCheckable(true);
    // toggleWhitespaceVisibilityAction->setIcon(QIcon::fromTheme(u"text-wrap"_s));
    actionCollection()->addAction(u"toggle_show_all_characters"_s, toggleWhitespaceVisibilityAction);
    connect(toggleWhitespaceVisibilityAction, &QAction::triggered, this, [this, toggleWhitespaceVisibilityAction](){
        bool switchToVisible = m_editor->whitespaceVisibility() == QsciScintilla::WsInvisible;
        m_editor->setWhitespaceVisibility(switchToVisible ? QsciScintilla::WsVisible : QsciScintilla::WsInvisible);
        m_editor->setEolVisibility(switchToVisible);
        toggleWhitespaceVisibilityAction->setChecked(switchToVisible);
    });

    QAction *toggleIndentGuideAction = new QAction(this);
    toggleIndentGuideAction->setText("Toggle Indent Guide");
    toggleIndentGuideAction->setIcon(QIcon::fromTheme(u"show-guides"_s));
    actionCollection()->addAction(u"toggle_indent_guide"_s, toggleIndentGuideAction);
    connect(toggleIndentGuideAction, &QAction::triggered, this, [this](){
        m_editor->setIndentationGuides(!m_editor->indentationGuides());
    });

    // Load themes
    KColorSchemeManager *manager = new KColorSchemeManager(this);
    auto *colorSelectionMenu = KColorSchemeMenu::createMenu(manager, this);
    colorSelectionMenu->menu()->setTitle("&Window Color Scheme");
    actionCollection()->addAction(QStringLiteral("colorscheme_menu"), colorSelectionMenu);

    setupGUI();
}

void MainWindow::showSettings()
{
    if (KConfigDialog::showDialog(QStringLiteral("settings"))) {
        return;
    }
    KConfigDialog *dialog = new KConfigDialog(this, QStringLiteral("settings"), AppSettings::self());
    dialog->setFaceType(KPageDialog::FlatList);
    dialog->addPage(new SettingsPage<Ui::GeneralSettings>(dialog), "Appearance", "preferences-desktop-theme-global");

    connect(dialog, &KConfigDialog::settingsChanged, this, [](const QString &dialogName){
        qDebug() << dialogName << "changed";
    });

    dialog->show();
}
