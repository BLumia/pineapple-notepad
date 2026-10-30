#include "rundialog.h"
#include "ui_rundialog.h"

#include "appsettings.h"
#include "sciedit.h"
#include "documentmanager.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>

RunDialog::RunDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RunDialog)
{
    ui->setupUi(this);

    m_commandCombo = ui->commandCombo;
    m_browseBtn = ui->browseBtn;
    m_variableBtn = ui->variableBtn;

    setupConnections();
    loadHistory();

    m_variableMenu = new QMenu(this);
    m_variableMenu->addAction(QStringLiteral("$(FULL_CURRENT_PATH)"));
    m_variableMenu->addAction(QStringLiteral("$(CURRENT_DIRECTORY)"));
    m_variableMenu->addAction(QStringLiteral("$(FILE_NAME)"));
    m_variableMenu->addAction(QStringLiteral("$(NAME_PART)"));
    m_variableMenu->addAction(QStringLiteral("$(EXT_PART)"));
    m_variableMenu->addAction(QStringLiteral("$(CURRENT_WORD)"));
    m_variableMenu->addAction(QStringLiteral("$(APP_DIRECTORY)"));
    m_variableMenu->addAction(QStringLiteral("$(APP_FULL_FILE_PATH)"));
    m_variableMenu->addAction(QStringLiteral("$(CURRENT_LINE)"));
    m_variableMenu->addAction(QStringLiteral("$(CURRENT_COLUMN)"));
    m_variableMenu->addAction(QStringLiteral("$(CURRENT_LINESTR)"));
}

RunDialog::~RunDialog()
{
    delete ui;
}

void RunDialog::setRunContext(SciEdit *editor, DocumentManager *documentManager, int documentId)
{
    m_context.editor = editor;
    m_context.documentManager = documentManager;
    m_context.currentDocumentId = documentId;
}

void RunDialog::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event)
    hide();
}

void RunDialog::setupConnections()
{
    connect(m_browseBtn, &QPushButton::clicked, this, &RunDialog::onBrowseClicked);
    connect(m_variableBtn, &QPushButton::clicked, this, &RunDialog::onVariableMenuRequested);

    auto *buttonBox = ui->buttonBox;
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RunDialog::onRunClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void RunDialog::loadHistory()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("RunDialog"));
    m_history = settings.value(QStringLiteral("history")).toStringList();
    settings.endGroup();

    m_commandCombo->clear();
    m_commandCombo->addItems(m_history);
}

void RunDialog::saveHistory()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("RunDialog"));
    settings.setValue(QStringLiteral("history"), m_history);
    settings.endGroup();
}

void RunDialog::addToHistory(const QString &command)
{
    if (command.isEmpty()) {
        return;
    }

    m_history.removeAll(command);
    m_history.prepend(command);

    while (m_history.size() > MaxHistorySize) {
        m_history.removeLast();
    }

    m_commandCombo->clear();
    m_commandCombo->addItems(m_history);
    m_commandCombo->setCurrentIndex(0);
}

QString RunDialog::expandVariables(const QString &command) const
{
    QString result = command;

    if (m_context.editor && m_context.documentManager) {
        const int docId = m_context.currentDocumentId;
        const QString filePath = m_context.documentManager->getDocumentFilePath(docId);

        if (!filePath.isEmpty()) {
            const QFileInfo fileInfo(filePath);
            result.replace(QStringLiteral("$(FULL_CURRENT_PATH)"), filePath);
            result.replace(QStringLiteral("$(CURRENT_DIRECTORY)"), fileInfo.absolutePath());
            result.replace(QStringLiteral("$(FILE_NAME)"), fileInfo.fileName());
            result.replace(QStringLiteral("$(NAME_PART)"), fileInfo.baseName());
            result.replace(QStringLiteral("$(EXT_PART)"), QStringLiteral(".") + fileInfo.suffix());
        }

        const int selStart = static_cast<int>(m_context.editor->selectionStart());
        const int selEnd = static_cast<int>(m_context.editor->selectionEnd());
        const QByteArray selectedBytes = m_context.editor->get_text_range(selStart, selEnd);
        const QString currentWord = QString::fromUtf8(selectedBytes);
        result.replace(QStringLiteral("$(CURRENT_WORD)"), currentWord);

        const int currentPos = static_cast<int>(m_context.editor->currentPos());
        const int currentLine = static_cast<int>(m_context.editor->lineFromPosition(currentPos)) + 1;
        const int lineStart = static_cast<int>(m_context.editor->positionFromLine(currentLine - 1));
        const int currentColumn = currentPos - lineStart + 1;

        result.replace(QStringLiteral("$(CURRENT_LINE)"), QString::number(currentLine));
        result.replace(QStringLiteral("$(CURRENT_COLUMN)"), QString::number(currentColumn));

        const int lineEnd = static_cast<int>(m_context.editor->lineEndPosition(currentLine - 1));
        const QString currentLineStr = m_context.editor->textRange(lineStart, lineEnd);
        result.replace(QStringLiteral("$(CURRENT_LINESTR)"), currentLineStr);
    }

    result.replace(QStringLiteral("$(APP_DIRECTORY)"), QCoreApplication::applicationDirPath());
    result.replace(QStringLiteral("$(APP_FULL_FILE_PATH)"), QCoreApplication::applicationFilePath());

    return result;
}

void RunDialog::onBrowseClicked()
{
    QFileDialog::Options options;
    if (!AppSettings::useNativeFileDialog()) {
        options |= QFileDialog::DontUseNativeDialog;
    }

    QString filePath = QFileDialog::getOpenFileName(this, tr("Select Program"), QString(), QString(), nullptr, options);
    if (!filePath.isEmpty()) {
        m_commandCombo->setEditText(filePath);
    }
}

void RunDialog::onVariableMenuRequested()
{
    QAction *action = m_variableMenu->exec(m_variableBtn->mapToGlobal(QPoint(0, m_variableBtn->height())));
    if (action) {
        m_commandCombo->setEditText(m_commandCombo->currentText() + action->text());
    }
}

void RunDialog::onRunClicked()
{
    const QString rawCommand = m_commandCombo->currentText().trimmed();
    if (rawCommand.isEmpty()) {
        QMessageBox::warning(this, tr("Run"), tr("Please enter a command to run."));
        return;
    }

    addToHistory(rawCommand);
    saveHistory();

    const QString expandedCommand = expandVariables(rawCommand);

    QString program;
    QStringList arguments;
    QFileInfo programInfo(expandedCommand);

    if (programInfo.exists() && programInfo.isFile()) {
        program = programInfo.absoluteFilePath();
    } else {
        const QStringList parts = QProcess::splitCommand(expandedCommand);
        if (!parts.isEmpty()) {
            program = parts.first();
            arguments = parts.mid(1);
        }
    }

    if (program.isEmpty()) {
        QMessageBox::warning(this, tr("Run"), tr("Invalid command."));
        return;
    }

    QString workingDir;
    if (m_context.editor && m_context.documentManager) {
        const int docId = m_context.currentDocumentId;
        const QString filePath = m_context.documentManager->getDocumentFilePath(docId);
        if (!filePath.isEmpty()) {
            workingDir = QFileInfo(filePath).absolutePath();
        }
    }

    if (workingDir.isEmpty()) {
        workingDir = QDir::currentPath();
    }

    qint64 pid = 0;
    const bool started = QProcess::startDetached(program, arguments, workingDir, &pid);

    if (!started) {
        QMessageBox::critical(this, tr("Run"), tr("Failed to start program:\n%1").arg(expandedCommand));
        return;
    }

    accept();
}
