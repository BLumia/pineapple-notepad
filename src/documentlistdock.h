#pragma once

#include <QDockWidget>

namespace Ui {
class DocumentListDock;
}

class DocumentManager;
class EditorViewManager;

class DocumentListDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit DocumentListDock(QWidget *parent = nullptr);
    ~DocumentListDock() override;

    void setDocumentManager(DocumentManager *dm);
    void setEditorViewManager(EditorViewManager *vm);
    void setCurrentDocId(int docId);

signals:
    void documentActivated(int docId);
    void closeRequested(int docId);
    void contextMenuRequested(int docId, const QPoint &globalPos);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Ui::DocumentListDock *ui;
    DocumentManager *m_dm = nullptr;
};
