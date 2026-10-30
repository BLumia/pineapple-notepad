#include "macromanagedialog.h"
#include "ui_macromanagedialog.h"
#include "macro.h"
#include "macrostep.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>

MacroManageDialog::MacroManageDialog(std::vector<std::unique_ptr<Macro>> &macros, QWidget *parent)
    : QDialog(parent)
    , m_macros(macros)
    , ui(new Ui::MacroManageDialog)
{
    ui->setupUi(this);

    ui->stepTable->horizontalHeader()->setStretchLastSection(true);
    ui->stepTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeMode::ResizeToContents);

    connect(ui->listWidget, &QListWidget::currentRowChanged, this, &MacroManageDialog::onCurrentRowChanged);
    connect(ui->nameEdit, &QLineEdit::textChanged, this, &MacroManageDialog::onNameChanged);
    connect(ui->shortcutEdit, &QKeySequenceEdit::keySequenceChanged, this, &MacroManageDialog::onShortcutChanged);
    connect(ui->copyButton, &QPushButton::clicked, this, &MacroManageDialog::copyCurrentMacro);
    connect(ui->deleteButton, &QPushButton::clicked, this, &MacroManageDialog::deleteCurrentMacro);

    populateMacroList();
    if (!m_macros.empty()) {
        selectMacro(0);
    } else {
        onCurrentRowChanged(-1);
    }
}

MacroManageDialog::~MacroManageDialog()
{
    delete ui;
}

void MacroManageDialog::populateMacroList()
{
    m_updating = true;
    ui->listWidget->clear();
    for (int i = 0; i < static_cast<int>(m_macros.size()); ++i) {
        ui->listWidget->addItem(m_macros[i]->name());
    }
    m_updating = false;
}

void MacroManageDialog::selectMacro(int row)
{
    ui->listWidget->setCurrentRow(row);
}

void MacroManageDialog::onCurrentRowChanged(int row)
{
    const bool valid = row >= 0 && row < static_cast<int>(m_macros.size());

    ui->nameEdit->setEnabled(valid);
    ui->shortcutEdit->setEnabled(valid);
    ui->stepTable->setEnabled(valid);

    if (!valid) {
        m_updating = true;
        ui->nameEdit->clear();
        ui->shortcutEdit->clear();
        ui->stepTable->setRowCount(0);
        m_updating = false;
        return;
    }

    Macro *macro = m_macros[row].get();
    m_updating = true;
    ui->nameEdit->setText(macro->name());
    ui->shortcutEdit->setKeySequence(macro->shortcut());
    m_updating = false;
    updateStepTable(macro);
}

void MacroManageDialog::onNameChanged(const QString &text)
{
    if (m_updating) {
        return;
    }

    const int row = ui->listWidget->currentRow();
    if (row < 0 || row >= static_cast<int>(m_macros.size())) {
        return;
    }

    if (text.trimmed().isEmpty()) {
        ui->nameEdit->setStyleSheet(QStringLiteral("QLineEdit{border: 2px solid red}"));
        return;
    }
    ui->nameEdit->setStyleSheet({});

    m_macros[row]->setName(text);
    m_updating = true;
    ui->listWidget->item(row)->setText(text);
    m_updating = false;
}

void MacroManageDialog::onShortcutChanged(const QKeySequence &shortcut)
{
    if (m_updating) {
        return;
    }

    const int row = ui->listWidget->currentRow();
    if (row < 0 || row >= static_cast<int>(m_macros.size())) {
        return;
    }

    m_macros[row]->setShortcut(shortcut);
}

void MacroManageDialog::copyCurrentMacro()
{
    const int row = ui->listWidget->currentRow();
    if (row < 0 || row >= static_cast<int>(m_macros.size())) {
        return;
    }

    auto copy = std::make_unique<Macro>(*m_macros[row]);
    copy->setName(m_macros[row]->name() + tr(" (Copy)"));
    copy->setShortcut(QKeySequence());

    m_macros.push_back(std::move(copy));
    populateMacroList();
    selectMacro(static_cast<int>(m_macros.size()) - 1);
}

void MacroManageDialog::deleteCurrentMacro()
{
    const int row = ui->listWidget->currentRow();
    if (row < 0 || row >= static_cast<int>(m_macros.size())) {
        return;
    }

    const int ret = QMessageBox::question(this,
        tr("Delete Macro"),
        tr("Are you sure you want to delete \"%1\"?").arg(m_macros[row]->name()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret != QMessageBox::Yes) {
        return;
    }

    m_macros.erase(m_macros.begin() + row);
    populateMacroList();

    if (!m_macros.empty()) {
        const int newRow = qMin(row, static_cast<int>(m_macros.size()) - 1);
        selectMacro(newRow);
    } else {
        onCurrentRowChanged(-1);
    }
}

void MacroManageDialog::updateStepTable(Macro *macro)
{
    ui->stepTable->setRowCount(0);

    if (!macro) {
        return;
    }

    const QVector<MacroStep> &steps = macro->steps();
    ui->stepTable->setRowCount(steps.size());

    for (int i = 0; i < steps.size(); ++i) {
        const MacroStep &step = steps[i];
        auto *nameItem = new QTableWidgetItem(MacroStep::messageName(step.message));
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        ui->stepTable->setItem(i, 0, nameItem);

        QString dataText;
        if (MacroStep::messageHasString(step.message)) {
            dataText = QString::fromUtf8(step.str);
            if (dataText.length() > 80) {
                dataText = dataText.left(80) + QStringLiteral("...");
            }
            dataText.replace(QLatin1Char('\n'), QStringLiteral("\\n"))
                    .replace(QLatin1Char('\r'), QStringLiteral("\\r"))
                    .replace(QLatin1Char('\t'), QStringLiteral("\\t"));
        }
        auto *dataItem = new QTableWidgetItem(dataText);
        dataItem->setFlags(dataItem->flags() & ~Qt::ItemIsEditable);
        ui->stepTable->setItem(i, 1, dataItem);
    }
}
