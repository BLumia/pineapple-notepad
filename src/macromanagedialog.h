#pragma once

#include <QDialog>
#include <memory>
#include <vector>

class QListWidget;
class QLineEdit;
class QKeySequenceEdit;
class QTableWidget;
class Macro;

namespace Ui {
class MacroManageDialog;
}

class MacroManageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MacroManageDialog(std::vector<std::unique_ptr<Macro>> &macros, QWidget *parent = nullptr);
    ~MacroManageDialog();

private:
    void onCurrentRowChanged(int row);
    void onNameChanged(const QString &text);
    void onShortcutChanged(const QKeySequence &shortcut);
    void copyCurrentMacro();
    void deleteCurrentMacro();
    void updateStepTable(Macro *macro);
    void populateMacroList();
    void selectMacro(int row);

    std::vector<std::unique_ptr<Macro>> &m_macros;
    Ui::MacroManageDialog *ui = nullptr;
    bool m_updating = false;
};
