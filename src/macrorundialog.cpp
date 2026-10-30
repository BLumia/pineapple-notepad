#include "macrorundialog.h"
#include "macro.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVariant>

Q_DECLARE_METATYPE(Macro *)

MacroRunDialog::MacroRunDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Run a Macro Multiple Times"));

    auto *layout = new QVBoxLayout(this);

    auto *formLayout = new QFormLayout();
    m_macroCombo = new QComboBox(this);
    formLayout->addRow(tr("Macro:"), m_macroCombo);
    layout->addLayout(formLayout);

    auto *groupBox = new QGroupBox(tr("Run Mode"), this);
    auto *groupLayout = new QVBoxLayout(groupBox);

    auto *timesLayout = new QHBoxLayout();
    m_runTimesRadio = new QRadioButton(tr("Execute"), this);
    m_runTimesRadio->setChecked(true);
    m_timesSpin = new QSpinBox(this);
    m_timesSpin->setRange(1, 999999999);
    m_timesSpin->setValue(2);
    timesLayout->addWidget(m_runTimesRadio);
    timesLayout->addWidget(m_timesSpin);
    timesLayout->addWidget(new QLabel(tr("times"), this));
    timesLayout->addStretch();
    groupLayout->addLayout(timesLayout);

    m_runToEndRadio = new QRadioButton(tr("Run until end of file"), this);
    groupLayout->addWidget(m_runToEndRadio);

    layout->addWidget(groupBox);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    connect(m_runTimesRadio, &QRadioButton::toggled, m_timesSpin, &QWidget::setEnabled);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void MacroRunDialog::setMacros(Macro *currentMacro, const QVector<Macro *> &savedMacros)
{
    m_macroCombo->clear();

    if (currentMacro && !currentMacro->isEmpty()) {
        const QString name = currentMacro->name().isEmpty()
            ? tr("Current Recorded Macro") : currentMacro->name();
        m_macroCombo->addItem(name, QVariant::fromValue(currentMacro));
    }

    for (Macro *macro : savedMacros) {
        m_macroCombo->addItem(macro->name(), QVariant::fromValue(macro));
    }

    if (m_macroCombo->count() > 0) {
        m_macroCombo->setCurrentIndex(0);
    }
}

Macro *MacroRunDialog::selectedMacro() const
{
    return m_macroCombo->currentData().value<Macro *>();
}

int MacroRunDialog::runCount() const
{
    if (m_runToEndRadio->isChecked()) {
        return -1;
    }
    return m_timesSpin->value();
}
