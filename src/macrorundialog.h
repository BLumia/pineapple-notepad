#pragma once

#include <QDialog>
#include <QVector>

class QComboBox;
class QRadioButton;
class QSpinBox;
class Macro;

class MacroRunDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MacroRunDialog(QWidget *parent = nullptr);

    void setMacros(Macro *currentMacro, const QVector<Macro *> &savedMacros);

    Macro *selectedMacro() const;
    int runCount() const;

private:
    QComboBox *m_macroCombo = nullptr;
    QRadioButton *m_runTimesRadio = nullptr;
    QRadioButton *m_runToEndRadio = nullptr;
    QSpinBox *m_timesSpin = nullptr;
};
