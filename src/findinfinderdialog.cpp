#include "findinfinderdialog.h"

#include "ui_findinfinderdialog.h"

using namespace Qt::StringLiterals;

namespace {

// Last used term of this dialog within the session.
QString s_lastFindText;

} // namespace

FindInFinderDialog::FindInFinderDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FindInFinderDialog)
{
    ui->setupUi(this);
    ui->findWhatCombo->setCurrentText(s_lastFindText);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        s_lastFindText = ui->findWhatCombo->currentText();
    });

    // "Whole word" has no meaning for regex patterns.
    connect(ui->modeRegexRadio, &QRadioButton::toggled, this, [this](bool checked) {
        ui->wholeWordCheck->setEnabled(!checked);
    });
}

FindInFinderDialog::~FindInFinderDialog()
{
    delete ui;
}

QString FindInFinderDialog::findText() const
{
    return ui->findWhatCombo->currentText();
}

QString FindInFinderDialog::findTextForSearch() const
{
    const QString raw = findText();
    if (searchMode() == SearchMode::Extended) {
        return SearchUtil::decodeExtended(raw);
    }
    return raw;
}

bool FindInFinderDialog::matchCase() const
{
    return ui->matchCaseCheck->isChecked();
}

bool FindInFinderDialog::wholeWord() const
{
    return ui->wholeWordCheck->isEnabled() && ui->wholeWordCheck->isChecked();
}

SearchMode FindInFinderDialog::searchMode() const
{
    if (ui->modeRegexRadio->isChecked()) {
        return SearchMode::Regex;
    }
    if (ui->modeExtendedRadio->isChecked()) {
        return SearchMode::Extended;
    }
    return SearchMode::Normal;
}

bool FindInFinderDialog::matchLineNumber() const
{
    return ui->matchLineNumberCheck->isChecked();
}
