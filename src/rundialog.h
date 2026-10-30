#pragma once

#include <QDialog>

class QComboBox;
class QPushButton;
class QMenu;
class SciEdit;
class DocumentManager;

namespace Ui {
class RunDialog;
}

struct RunContext {
    SciEdit *editor = nullptr;
    DocumentManager *documentManager = nullptr;
    int currentDocumentId = -1;
};

class RunDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit RunDialog(QWidget *parent = nullptr);
    ~RunDialog() override;

    void setRunContext(SciEdit *editor, DocumentManager *documentManager, int documentId);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onBrowseClicked();
    void onVariableMenuRequested();
    void onRunClicked();

private:
    void setupConnections();
    void loadHistory();
    void saveHistory();
    void addToHistory(const QString &command);
    QString expandVariables(const QString &command) const;

    QComboBox *m_commandCombo = nullptr;
    QPushButton *m_browseBtn = nullptr;
    QPushButton *m_variableBtn = nullptr;
    QMenu *m_variableMenu = nullptr;
    Ui::RunDialog *ui = nullptr;

    RunContext m_context;
    QStringList m_history;
    static constexpr int MaxHistorySize = 20;
};
