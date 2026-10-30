#include "summarydialog.h"

#include "ui_summarydialog.h"

#include <QDialogButtonBox>

SummaryDialog::SummaryDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SummaryDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

SummaryDialog::~SummaryDialog()
{
    delete ui;
}

void SummaryDialog::setSummaryData(const SummaryData &data)
{
    ui->labelFilePathValue->setText(data.filePath);
    ui->labelCreatedValue->setText(data.created);
    ui->labelModifiedValue->setText(data.modified);
    ui->labelCharactersValue->setText(data.characters);
    ui->labelWordsValue->setText(data.words);
    ui->labelLinesValue->setText(data.lines);
    ui->labelDocumentLengthValue->setText(data.documentLength);
    ui->labelSelectedCharactersValue->setText(data.selectedCharacters);
    ui->labelSelectedBytesValue->setText(data.selectedBytes);
    ui->labelSelectionRangesValue->setText(data.selectionRanges);

    const bool showFileInfo = data.hasFileInfo;
    ui->labelFilePathTitle->setVisible(showFileInfo);
    ui->labelFilePathValue->setVisible(showFileInfo);
    ui->labelCreatedTitle->setVisible(showFileInfo);
    ui->labelCreatedValue->setVisible(showFileInfo);
    ui->labelModifiedTitle->setVisible(showFileInfo);
    ui->labelModifiedValue->setVisible(showFileInfo);
}
