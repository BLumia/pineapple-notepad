#include "findreplacedialog.h"
#include "appsettings.h"
#include "sciedit.h"

#include "ui_findreplacedialog.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>

QString SearchUtil::decodeExtended(const QString &s)
{
    QString out;
    out.reserve(s.size());

    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c != QLatin1Char('\\') || i + 1 >= s.size()) {
            out.append(c);
            continue;
        }

        const QChar n = s.at(i + 1);
        if (n == QLatin1Char('n')) {
            out.append(QLatin1Char('\n'));
            ++i;
        } else if (n == QLatin1Char('r')) {
            out.append(QLatin1Char('\r'));
            ++i;
        } else if (n == QLatin1Char('t')) {
            out.append(QLatin1Char('\t'));
            ++i;
        } else if (n == QLatin1Char('\\')) {
            out.append(QLatin1Char('\\'));
            ++i;
        } else if (n == QLatin1Char('0')) {
            out.append(QChar(0));
            ++i;
        } else if (n == QLatin1Char('x') && i + 3 < s.size()) {
            bool ok = false;
            const QString hex = s.mid(i + 2, 2);
            const int code = hex.toInt(&ok, 16);
            if (ok) {
                out.append(QChar(code));
                i += 3;
            } else {
                out.append(c);
            }
        } else {
            out.append(n);
            ++i;
        }
    }

    return out;
}

FindReplaceDialog::FindReplaceDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FindReplaceDialog)
{
    ui->setupUi(this);
    setModal(false);
    setWindowFlags(windowFlags() | Qt::Tool);

    initCombos();
    initModeRadios();
    initSync();

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        Q_UNUSED(index)
        QWidget *current = ui->tabWidget->currentWidget();
        if (current == ui->findTab) {
            syncFromReplaceTab();
        } else if (current == ui->replaceTab) {
            syncFromFindTab();
        } else {
            syncFromFindTab();
        }
    });

    connect(ui->findNextBtnFind, &QPushButton::clicked, this, [this]() {
        commitHistory();
        emit findRequested(!(qApp->keyboardModifiers() & Qt::ShiftModifier));
    });
    connect(ui->findPrevBtnFind, &QPushButton::clicked, this, [this]() {
        commitHistory();
        emit countRequested();
    });
    connect(ui->findAllInCurrentBtn, &QPushButton::clicked, this, [this]() {
        commitHistory();
        emit findAllInCurrentDocumentRequested();
    });
    connect(ui->findAllInDocsBtn, &QPushButton::clicked, this, [this]() {
        commitHistory();
        emit findAllInAllDocumentsRequested();
    });
    connect(ui->closeBtnFind, &QPushButton::clicked, this, &QDialog::hide);

    connect(ui->findNextBtnReplace, &QPushButton::clicked, this, [this]() {
        commitHistory();
        emit findRequested(true);
    });
    connect(ui->replaceBtn, &QPushButton::clicked, this, [this]() {
        commitHistory();
        emit replaceRequested();
    });
    connect(ui->replaceAllBtn, &QPushButton::clicked, this, [this]() {
        commitHistory();
        emit replaceAllRequested();
    });
    connect(ui->replaceAllInDocsBtn, &QPushButton::clicked, this, [this]() {
        commitHistory();
        emit replaceAllInAllDocumentsRequested();
    });
    connect(ui->closeBtnReplace, &QPushButton::clicked, this, &QDialog::hide);

    connect(ui->findAllBtnFif, &QPushButton::clicked, this, [this]() {
        commitHistory();
        emit findInFilesRequested();
    });
    connect(ui->closeBtnFif, &QPushButton::clicked, this, &QDialog::hide);

    for (int i = 0; i < SciEdit::kMarkStyleCount; ++i) {
        QPixmap pm(12, 12);
        pm.fill(SciEdit::markStyleColor(i));
        ui->markStyleComboMark->addItem(QIcon(pm), tr("Style %1").arg(i + 1));
    }

    connect(ui->markAllBtnMark, &QPushButton::clicked, this, [this]() {
        commitHistory();
        emit markAllRequested();
    });
    connect(ui->clearMarksBtnMark, &QPushButton::clicked, this, [this]() {
        emit clearMarksRequested();
    });
    connect(ui->copyMarkedBtnMark, &QPushButton::clicked, this, [this]() {
        emit copyMarkedTextRequested();
    });
    connect(ui->closeBtnMark, &QPushButton::clicked, this, &QDialog::hide);

    connect(ui->modeRegexRadioMark, &QRadioButton::toggled, this, [this](bool checked) {
        ui->backwardCheckMark->setEnabled(!checked);
    });

    connect(ui->browseDirBtnFif, &QPushButton::clicked, this, [this]() {
        const QString startDir = ui->directoryComboFif->currentText();
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Directory"),
            startDir.isEmpty() ? m_currentDocDirectory : startDir);
        if (!dir.isEmpty()) {
            ui->directoryComboFif->setCurrentText(dir);
        }
    });
    connect(ui->currentDocDirBtnFif, &QPushButton::clicked, this, [this]() {
        if (!m_currentDocDirectory.isEmpty()) {
            ui->directoryComboFif->setCurrentText(m_currentDocDirectory);
        }
    });

    resize(520, sizeHint().height());
}

FindReplaceDialog::~FindReplaceDialog()
{
    delete ui;
}

void FindReplaceDialog::initCombos()
{
    ui->findWhatComboFind->setInsertPolicy(QComboBox::NoInsert);
    ui->findWhatComboReplace->setInsertPolicy(QComboBox::NoInsert);
    ui->replaceWithComboReplace->setInsertPolicy(QComboBox::NoInsert);
    ui->findWhatComboFif->setInsertPolicy(QComboBox::NoInsert);
    ui->findWhatComboMark->setInsertPolicy(QComboBox::NoInsert);
    ui->filtersComboFif->setInsertPolicy(QComboBox::NoInsert);
    ui->directoryComboFif->setInsertPolicy(QComboBox::NoInsert);
}

void FindReplaceDialog::initModeRadios()
{
    ui->modeNormalRadioFind->setChecked(true);
    ui->modeNormalRadioReplace->setChecked(true);
    ui->modeNormalRadioFif->setChecked(true);
    ui->modeNormalRadioMark->setChecked(true);
}

void FindReplaceDialog::initSync()
{
    if (ui->findWhatComboFind->lineEdit()) {
        connect(ui->findWhatComboFind->lineEdit(), &QLineEdit::textEdited, this, [this]() { syncFromFindTab(); });
        connect(ui->findWhatComboFind->lineEdit(), &QLineEdit::returnPressed, this, [this]() {
            commitHistory();
            emit findRequested(!(qApp->keyboardModifiers() & Qt::ShiftModifier));
        });
    }
    connect(ui->matchCaseCheckFind, &QCheckBox::toggled, this, [this]() { syncFromFindTab(); });
    connect(ui->wholeWordCheckFind, &QCheckBox::toggled, this, [this]() { syncFromFindTab(); });
    connect(ui->wrapAroundCheckFind, &QCheckBox::toggled, this, [this]() { syncFromFindTab(); });
    connect(ui->inSelectionCheckFind, &QCheckBox::toggled, this, [this]() { syncFromFindTab(); });
    connect(ui->modeNormalRadioFind, &QRadioButton::toggled, this, [this]() { syncFromFindTab(); });
    connect(ui->modeExtendedRadioFind, &QRadioButton::toggled, this, [this]() { syncFromFindTab(); });
    connect(ui->modeRegexRadioFind, &QRadioButton::toggled, this, [this]() { syncFromFindTab(); });

    if (ui->findWhatComboReplace->lineEdit()) {
        connect(ui->findWhatComboReplace->lineEdit(), &QLineEdit::textEdited, this, [this]() { syncFromReplaceTab(); });
        connect(ui->findWhatComboReplace->lineEdit(), &QLineEdit::returnPressed, this, [this]() {
            commitHistory();
            emit findRequested(true);
        });
    }
    if (ui->replaceWithComboReplace->lineEdit()) {
        connect(ui->replaceWithComboReplace->lineEdit(), &QLineEdit::textEdited, this, [this]() { syncFromReplaceTab(); });
    }
    connect(ui->matchCaseCheckReplace, &QCheckBox::toggled, this, [this]() { syncFromReplaceTab(); });
    connect(ui->wholeWordCheckReplace, &QCheckBox::toggled, this, [this]() { syncFromReplaceTab(); });
    connect(ui->wrapAroundCheckReplace, &QCheckBox::toggled, this, [this]() { syncFromReplaceTab(); });
    connect(ui->inSelectionCheckReplace, &QCheckBox::toggled, this, [this]() { syncFromReplaceTab(); });
    connect(ui->modeNormalRadioReplace, &QRadioButton::toggled, this, [this]() { syncFromReplaceTab(); });
    connect(ui->modeExtendedRadioReplace, &QRadioButton::toggled, this, [this]() { syncFromReplaceTab(); });
    connect(ui->modeRegexRadioReplace, &QRadioButton::toggled, this, [this]() { syncFromReplaceTab(); });

    if (ui->findWhatComboFif->lineEdit()) {
        connect(ui->findWhatComboFif->lineEdit(), &QLineEdit::textEdited, this, [this]() { syncFromFifTab(); });
        connect(ui->findWhatComboFif->lineEdit(), &QLineEdit::returnPressed, this, [this]() {
            commitHistory();
            emit findInFilesRequested();
        });
    }
    connect(ui->matchCaseCheckFif, &QCheckBox::toggled, this, [this]() { syncFromFifTab(); });
    connect(ui->wholeWordCheckFif, &QCheckBox::toggled, this, [this]() { syncFromFifTab(); });
    connect(ui->modeNormalRadioFif, &QRadioButton::toggled, this, [this]() { syncFromFifTab(); });
    connect(ui->modeExtendedRadioFif, &QRadioButton::toggled, this, [this]() { syncFromFifTab(); });
    connect(ui->modeRegexRadioFif, &QRadioButton::toggled, this, [this]() { syncFromFifTab(); });

    if (ui->findWhatComboMark->lineEdit()) {
        connect(ui->findWhatComboMark->lineEdit(), &QLineEdit::textEdited, this, [this]() { syncFromMarkTab(); });
        connect(ui->findWhatComboMark->lineEdit(), &QLineEdit::returnPressed, this, [this]() {
            commitHistory();
            emit markAllRequested();
        });
    }
    connect(ui->matchCaseCheckMark, &QCheckBox::toggled, this, [this]() { syncFromMarkTab(); });
    connect(ui->wholeWordCheckMark, &QCheckBox::toggled, this, [this]() { syncFromMarkTab(); });
    connect(ui->wrapAroundCheckMark, &QCheckBox::toggled, this, [this]() { syncFromMarkTab(); });
    connect(ui->inSelectionCheckMark, &QCheckBox::toggled, this, [this]() { syncFromMarkTab(); });
    connect(ui->modeNormalRadioMark, &QRadioButton::toggled, this, [this]() { syncFromMarkTab(); });
    connect(ui->modeExtendedRadioMark, &QRadioButton::toggled, this, [this]() { syncFromMarkTab(); });
    connect(ui->modeRegexRadioMark, &QRadioButton::toggled, this, [this]() { syncFromMarkTab(); });

    syncFromFindTab();
}

void FindReplaceDialog::syncFromFindTab()
{
    if (m_syncing) {
        return;
    }
    m_syncing = true;
    ui->findWhatComboReplace->setCurrentText(ui->findWhatComboFind->currentText());
    ui->matchCaseCheckReplace->setChecked(ui->matchCaseCheckFind->isChecked());
    ui->wholeWordCheckReplace->setChecked(ui->wholeWordCheckFind->isChecked());
    ui->wrapAroundCheckReplace->setChecked(ui->wrapAroundCheckFind->isChecked());
    ui->inSelectionCheckReplace->setChecked(ui->inSelectionCheckFind->isChecked());
    ui->modeNormalRadioReplace->setChecked(ui->modeNormalRadioFind->isChecked());
    ui->modeExtendedRadioReplace->setChecked(ui->modeExtendedRadioFind->isChecked());
    ui->modeRegexRadioReplace->setChecked(ui->modeRegexRadioFind->isChecked());

    ui->findWhatComboFif->setCurrentText(ui->findWhatComboFind->currentText());
    ui->matchCaseCheckFif->setChecked(ui->matchCaseCheckFind->isChecked());
    ui->wholeWordCheckFif->setChecked(ui->wholeWordCheckFind->isChecked());
    ui->modeNormalRadioFif->setChecked(ui->modeNormalRadioFind->isChecked());
    ui->modeExtendedRadioFif->setChecked(ui->modeExtendedRadioFind->isChecked());
    ui->modeRegexRadioFif->setChecked(ui->modeRegexRadioFind->isChecked());

    ui->findWhatComboMark->setCurrentText(ui->findWhatComboFind->currentText());
    ui->matchCaseCheckMark->setChecked(ui->matchCaseCheckFind->isChecked());
    ui->wholeWordCheckMark->setChecked(ui->wholeWordCheckFind->isChecked());
    ui->wrapAroundCheckMark->setChecked(ui->wrapAroundCheckFind->isChecked());
    ui->inSelectionCheckMark->setChecked(ui->inSelectionCheckFind->isChecked());
    ui->modeNormalRadioMark->setChecked(ui->modeNormalRadioFind->isChecked());
    ui->modeExtendedRadioMark->setChecked(ui->modeExtendedRadioFind->isChecked());
    ui->modeRegexRadioMark->setChecked(ui->modeRegexRadioFind->isChecked());
    m_syncing = false;
}

void FindReplaceDialog::syncFromReplaceTab()
{
    if (m_syncing) {
        return;
    }
    m_syncing = true;
    ui->findWhatComboFind->setCurrentText(ui->findWhatComboReplace->currentText());
    ui->matchCaseCheckFind->setChecked(ui->matchCaseCheckReplace->isChecked());
    ui->wholeWordCheckFind->setChecked(ui->wholeWordCheckReplace->isChecked());
    ui->wrapAroundCheckFind->setChecked(ui->wrapAroundCheckReplace->isChecked());
    ui->inSelectionCheckFind->setChecked(ui->inSelectionCheckReplace->isChecked());
    ui->modeNormalRadioFind->setChecked(ui->modeNormalRadioReplace->isChecked());
    ui->modeExtendedRadioFind->setChecked(ui->modeExtendedRadioReplace->isChecked());
    ui->modeRegexRadioFind->setChecked(ui->modeRegexRadioReplace->isChecked());

    ui->findWhatComboFif->setCurrentText(ui->findWhatComboReplace->currentText());
    ui->matchCaseCheckFif->setChecked(ui->matchCaseCheckReplace->isChecked());
    ui->wholeWordCheckFif->setChecked(ui->wholeWordCheckReplace->isChecked());
    ui->modeNormalRadioFif->setChecked(ui->modeNormalRadioReplace->isChecked());
    ui->modeExtendedRadioFif->setChecked(ui->modeExtendedRadioReplace->isChecked());
    ui->modeRegexRadioFif->setChecked(ui->modeRegexRadioReplace->isChecked());

    ui->findWhatComboMark->setCurrentText(ui->findWhatComboReplace->currentText());
    ui->matchCaseCheckMark->setChecked(ui->matchCaseCheckReplace->isChecked());
    ui->wholeWordCheckMark->setChecked(ui->wholeWordCheckReplace->isChecked());
    ui->inSelectionCheckMark->setChecked(ui->inSelectionCheckReplace->isChecked());
    ui->modeNormalRadioMark->setChecked(ui->modeNormalRadioReplace->isChecked());
    ui->modeExtendedRadioMark->setChecked(ui->modeExtendedRadioReplace->isChecked());
    ui->modeRegexRadioMark->setChecked(ui->modeRegexRadioReplace->isChecked());
    m_syncing = false;
}

void FindReplaceDialog::syncFromFifTab()
{
    if (m_syncing) {
        return;
    }
    m_syncing = true;
    ui->findWhatComboFind->setCurrentText(ui->findWhatComboFif->currentText());
    ui->matchCaseCheckFind->setChecked(ui->matchCaseCheckFif->isChecked());
    ui->wholeWordCheckFind->setChecked(ui->wholeWordCheckFif->isChecked());
    ui->modeNormalRadioFind->setChecked(ui->modeNormalRadioFif->isChecked());
    ui->modeExtendedRadioFind->setChecked(ui->modeExtendedRadioFif->isChecked());
    ui->modeRegexRadioFind->setChecked(ui->modeRegexRadioFif->isChecked());

    ui->findWhatComboReplace->setCurrentText(ui->findWhatComboFif->currentText());
    ui->matchCaseCheckReplace->setChecked(ui->matchCaseCheckFif->isChecked());
    ui->wholeWordCheckReplace->setChecked(ui->wholeWordCheckFif->isChecked());
    ui->modeNormalRadioReplace->setChecked(ui->modeNormalRadioFif->isChecked());
    ui->modeExtendedRadioReplace->setChecked(ui->modeExtendedRadioFif->isChecked());
    ui->modeRegexRadioReplace->setChecked(ui->modeRegexRadioFif->isChecked());

    ui->findWhatComboMark->setCurrentText(ui->findWhatComboFif->currentText());
    ui->matchCaseCheckMark->setChecked(ui->matchCaseCheckFif->isChecked());
    ui->wholeWordCheckMark->setChecked(ui->wholeWordCheckFif->isChecked());
    ui->modeNormalRadioMark->setChecked(ui->modeNormalRadioFif->isChecked());
    ui->modeExtendedRadioMark->setChecked(ui->modeExtendedRadioFif->isChecked());
    ui->modeRegexRadioMark->setChecked(ui->modeRegexRadioFif->isChecked());
    m_syncing = false;
}

void FindReplaceDialog::syncFromMarkTab()
{
    if (m_syncing) {
        return;
    }
    m_syncing = true;
    ui->findWhatComboFind->setCurrentText(ui->findWhatComboMark->currentText());
    ui->matchCaseCheckFind->setChecked(ui->matchCaseCheckMark->isChecked());
    ui->wholeWordCheckFind->setChecked(ui->wholeWordCheckMark->isChecked());
    ui->wrapAroundCheckFind->setChecked(ui->wrapAroundCheckMark->isChecked());
    ui->inSelectionCheckFind->setChecked(ui->inSelectionCheckMark->isChecked());
    ui->modeNormalRadioFind->setChecked(ui->modeNormalRadioMark->isChecked());
    ui->modeExtendedRadioFind->setChecked(ui->modeExtendedRadioMark->isChecked());
    ui->modeRegexRadioFind->setChecked(ui->modeRegexRadioMark->isChecked());

    ui->findWhatComboReplace->setCurrentText(ui->findWhatComboMark->currentText());
    ui->matchCaseCheckReplace->setChecked(ui->matchCaseCheckMark->isChecked());
    ui->wholeWordCheckReplace->setChecked(ui->wholeWordCheckMark->isChecked());
    ui->inSelectionCheckReplace->setChecked(ui->inSelectionCheckMark->isChecked());
    ui->modeNormalRadioReplace->setChecked(ui->modeNormalRadioMark->isChecked());
    ui->modeExtendedRadioReplace->setChecked(ui->modeExtendedRadioMark->isChecked());
    ui->modeRegexRadioReplace->setChecked(ui->modeRegexRadioMark->isChecked());

    ui->findWhatComboFif->setCurrentText(ui->findWhatComboMark->currentText());
    ui->matchCaseCheckFif->setChecked(ui->matchCaseCheckMark->isChecked());
    ui->wholeWordCheckFif->setChecked(ui->wholeWordCheckMark->isChecked());
    ui->modeNormalRadioFif->setChecked(ui->modeNormalRadioMark->isChecked());
    ui->modeExtendedRadioFif->setChecked(ui->modeExtendedRadioMark->isChecked());
    ui->modeRegexRadioFif->setChecked(ui->modeRegexRadioMark->isChecked());
    m_syncing = false;
}

void FindReplaceDialog::openTab(QWidget *tab, QComboBox *focusCombo)
{
    ui->tabWidget->setCurrentWidget(tab);
    show();
    raise();
    activateWindow();
    focusCombo->setFocus();
    if (focusCombo->lineEdit()) {
        focusCombo->lineEdit()->selectAll();
    }
}

void FindReplaceDialog::openFind()
{
    openTab(ui->findTab, ui->findWhatComboFind);
}

void FindReplaceDialog::openReplace()
{
    openTab(ui->replaceTab, ui->findWhatComboReplace);
}

void FindReplaceDialog::openFindInFiles()
{
    openTab(ui->findInFilesTab, ui->findWhatComboFif);
    // "Fill directory field with current document's directory" option:
    // Prefill the directory field when the tab is opened.
    if (AppSettings::fillDirFieldFromDocument() && !m_currentDocDirectory.isEmpty()) {
        ui->directoryComboFif->setCurrentText(m_currentDocDirectory);
    }
}

void FindReplaceDialog::openMark()
{
    openTab(ui->markTab, ui->findWhatComboMark);
}

QString FindReplaceDialog::findText() const
{
    if (ui->tabWidget->currentWidget() == ui->replaceTab) {
        return ui->findWhatComboReplace->currentText();
    }
    if (ui->tabWidget->currentWidget() == ui->findInFilesTab) {
        return ui->findWhatComboFif->currentText();
    }
    if (ui->tabWidget->currentWidget() == ui->markTab) {
        return ui->findWhatComboMark->currentText();
    }
    return ui->findWhatComboFind->currentText();
}

QString FindReplaceDialog::replaceText() const
{
    return ui->replaceWithComboReplace->currentText();
}

QString FindReplaceDialog::findTextForSearch() const
{
    const QString raw = findText();
    if (searchMode() == SearchMode::Extended) {
        return SearchUtil::decodeExtended(raw);
    }
    return raw;
}

QString FindReplaceDialog::replaceTextForReplace() const
{
    const QString raw = replaceText();
    if (searchMode() == SearchMode::Extended) {
        return SearchUtil::decodeExtended(raw);
    }
    return raw;
}

bool FindReplaceDialog::matchCase() const
{
    if (ui->tabWidget->currentWidget() == ui->replaceTab) {
        return ui->matchCaseCheckReplace->isChecked();
    }
    if (ui->tabWidget->currentWidget() == ui->findInFilesTab) {
        return ui->matchCaseCheckFif->isChecked();
    }
    if (ui->tabWidget->currentWidget() == ui->markTab) {
        return ui->matchCaseCheckMark->isChecked();
    }
    return ui->matchCaseCheckFind->isChecked();
}

bool FindReplaceDialog::wholeWord() const
{
    if (ui->tabWidget->currentWidget() == ui->replaceTab) {
        return ui->wholeWordCheckReplace->isChecked();
    }
    if (ui->tabWidget->currentWidget() == ui->findInFilesTab) {
        return ui->wholeWordCheckFif->isChecked();
    }
    if (ui->tabWidget->currentWidget() == ui->markTab) {
        return ui->wholeWordCheckMark->isChecked();
    }
    return ui->wholeWordCheckFind->isChecked();
}

bool FindReplaceDialog::wrapAround() const
{
    if (ui->tabWidget->currentWidget() == ui->replaceTab) {
        return ui->wrapAroundCheckReplace->isChecked();
    }
    if (ui->tabWidget->currentWidget() == ui->markTab) {
        return ui->wrapAroundCheckMark->isChecked();
    }
    return ui->wrapAroundCheckFind->isChecked();
}

bool FindReplaceDialog::inSelection() const
{
    if (ui->tabWidget->currentWidget() == ui->replaceTab) {
        return ui->inSelectionCheckReplace->isChecked();
    }
    if (ui->tabWidget->currentWidget() == ui->markTab) {
        return ui->inSelectionCheckMark->isChecked();
    }
    return ui->inSelectionCheckFind->isChecked();
}

SearchMode FindReplaceDialog::searchMode() const
{
    const bool isReplace = (ui->tabWidget->currentWidget() == ui->replaceTab);
    const bool isFif = (ui->tabWidget->currentWidget() == ui->findInFilesTab);
    const bool isMark = (ui->tabWidget->currentWidget() == ui->markTab);
    if (isReplace) {
        if (ui->modeNormalRadioReplace->isChecked()) return SearchMode::Normal;
        if (ui->modeExtendedRadioReplace->isChecked()) return SearchMode::Extended;
        if (ui->modeRegexRadioReplace->isChecked()) return SearchMode::Regex;
    } else if (isFif) {
        if (ui->modeNormalRadioFif->isChecked()) return SearchMode::Normal;
        if (ui->modeExtendedRadioFif->isChecked()) return SearchMode::Extended;
        if (ui->modeRegexRadioFif->isChecked()) return SearchMode::Regex;
    } else if (isMark) {
        if (ui->modeNormalRadioMark->isChecked()) return SearchMode::Normal;
        if (ui->modeExtendedRadioMark->isChecked()) return SearchMode::Extended;
        if (ui->modeRegexRadioMark->isChecked()) return SearchMode::Regex;
    } else {
        if (ui->modeNormalRadioFind->isChecked()) return SearchMode::Normal;
        if (ui->modeExtendedRadioFind->isChecked()) return SearchMode::Extended;
        if (ui->modeRegexRadioFind->isChecked()) return SearchMode::Regex;
    }
    return SearchMode::Normal;
}

int FindReplaceDialog::markStyleIndex() const
{
    return ui->markStyleComboMark->currentIndex();
}

bool FindReplaceDialog::bookmarkLine() const
{
    return ui->bookmarkLineCheckMark->isChecked();
}

bool FindReplaceDialog::purgeMarks() const
{
    return ui->purgeCheckMark->isChecked();
}

bool FindReplaceDialog::backwardDirection() const
{
    return ui->backwardCheckMark->isChecked() && ui->backwardCheckMark->isEnabled();
}

void FindReplaceDialog::setFindTextIfNotEmpty(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    ui->findWhatComboFind->setCurrentText(text);
    ui->findWhatComboReplace->setCurrentText(text);
    ui->findWhatComboFif->setCurrentText(text);
    ui->findWhatComboMark->setCurrentText(text);
    if (ui->tabWidget->currentWidget() == ui->replaceTab) {
        if (ui->findWhatComboReplace->lineEdit()) {
            ui->findWhatComboReplace->lineEdit()->selectAll();
        }
    } else if (ui->tabWidget->currentWidget() == ui->findInFilesTab) {
        if (ui->findWhatComboFif->lineEdit()) {
            ui->findWhatComboFif->lineEdit()->selectAll();
        }
    } else if (ui->tabWidget->currentWidget() == ui->markTab) {
        if (ui->findWhatComboMark->lineEdit()) {
            ui->findWhatComboMark->lineEdit()->selectAll();
        }
    } else {
        if (ui->findWhatComboFind->lineEdit()) {
            ui->findWhatComboFind->lineEdit()->selectAll();
        }
    }
}

void FindReplaceDialog::commitHistory()
{
    const auto commit = [](QComboBox *combo) {
        const QString v = combo->currentText();
        if (v.isEmpty()) {
            return;
        }
        const int idx = combo->findText(v);
        if (idx >= 0) {
            combo->removeItem(idx);
        }
        combo->insertItem(0, v);
        combo->setCurrentIndex(0);
        while (combo->count() > 20) {
            combo->removeItem(combo->count() - 1);
        }
    };

    commit(ui->findWhatComboFind);
    commit(ui->findWhatComboReplace);
    commit(ui->replaceWithComboReplace);
    commit(ui->findWhatComboFif);
    commit(ui->findWhatComboMark);
    commit(ui->filtersComboFif);
    commit(ui->directoryComboFif);
}

QString FindReplaceDialog::filters() const
{
    return ui->filtersComboFif->currentText();
}

QString FindReplaceDialog::directory() const
{
    return ui->directoryComboFif->currentText();
}

bool FindReplaceDialog::isRecursive() const
{
    return ui->recursiveCheckFif->isChecked();
}

bool FindReplaceDialog::isInHiddenFolders() const
{
    return ui->hiddenCheckFif->isChecked();
}

void FindReplaceDialog::setCurrentDocumentDirectory(const QString &dir)
{
    m_currentDocDirectory = dir;
}

void FindReplaceDialog::showStatusMessage(const QString &message)
{
    ui->statusBar->showMessage(message);
}

void FindReplaceDialog::clearStatusMessage()
{
    ui->statusBar->clearMessage();
}

void FindReplaceDialog::closeEvent(QCloseEvent *event)
{
    emit dialogClosed();
    hide();
    event->ignore();
}
