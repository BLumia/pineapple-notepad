#pragma once

#include <QDialog>

namespace Ui {
class GoToDialog;
}

class GoToDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit GoToDialog(QWidget *parent = nullptr);
    ~GoToDialog() override;

    void openWithDefaults(int currentLine, qlonglong currentPos);

signals:
    void goToRequested(bool lineMode, const QString &text);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void updateValueUi();

    Ui::GoToDialog *ui = nullptr;
    int m_currentLine = 1;
    qlonglong m_currentPos = 0;
};
