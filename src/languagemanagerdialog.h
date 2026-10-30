#pragma once

#include <QDialog>

class QPushButton;
class QTreeWidgetItem;

namespace Ui {
class LanguageManagerDialog;
}

// Management UI for catalog languages with a Lexilla engine: built-in
// languages (optionally overridden via user-languages.json), and user defined
// languages. Provides create/edit/override, reset (remove an override) and
// delete (remove a UDL). Pure selection is handled by LanguageSelectorDialog.
class LanguageManagerDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit LanguageManagerDialog(QWidget *parent = nullptr);
    ~LanguageManagerDialog() override;

signals:
    // Emitted after any modification (create/edit/reset/delete) that changed
    // the merged catalog, so callers can rebuild the "Language" menu.
    void languagesChanged();

private:
    void loadLanguages();
    void applyFilter(const QString &text);
    void updateButtons();
    void onNew();
    void onEdit();
    void onReset();
    void onDelete();

    Ui::LanguageManagerDialog *m_ui = nullptr;
    QString m_currentId;
    QPushButton *m_newButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
};
