#include "gotodialog.h"

#include "ui_gotodialog.h"

#include <QCloseEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>

GoToDialog::GoToDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::GoToDialog)
{
    ui->setupUi(this);
    setModal(false);
    setWindowFlags(windowFlags() | Qt::Tool);

    connect(ui->lineRadio, &QRadioButton::toggled, this, [this](bool) {
        updateValueUi();
    });
    connect(ui->offsetRadio, &QRadioButton::toggled, this, [this](bool) {
        updateValueUi();
    });

    connect(ui->goBtn, &QPushButton::clicked, this, [this]() {
        emit goToRequested(ui->lineRadio->isChecked(), ui->valueEdit->text());
    });
    connect(ui->closeBtn, &QPushButton::clicked, this, &QDialog::hide);

    updateValueUi();
    resize(sizeHint().width(), sizeHint().height());
}

GoToDialog::~GoToDialog()
{
    delete ui;
}

void GoToDialog::openWithDefaults(int currentLine, qlonglong currentPos)
{
    m_currentLine = qMax(1, currentLine);
    m_currentPos = qMax<qlonglong>(0, currentPos);
    updateValueUi();

    show();
    raise();
    activateWindow();
    ui->valueEdit->setFocus();
    ui->valueEdit->selectAll();
}

void GoToDialog::closeEvent(QCloseEvent *event)
{
    hide();
    if (event) {
        event->ignore();
    }
}

void GoToDialog::updateValueUi()
{
    if (ui->lineRadio->isChecked()) {
        ui->valueEdit->setText(QString::number(m_currentLine));
        ui->valueEdit->setPlaceholderText(QStringLiteral("1"));
    } else {
        ui->valueEdit->setText(QString::number(m_currentPos));
        ui->valueEdit->setPlaceholderText(QStringLiteral("0"));
    }
    ui->valueEdit->selectAll();
}
