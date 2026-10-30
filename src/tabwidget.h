#pragma once

#include <QTabWidget>
#include <QVector>
#include <QStringList>

#include "editorviewmanager.h"

class SciEdit;
class DocumentManager;

class TabWidget : public QTabWidget, public EditorViewManager
{
    Q_OBJECT

public:
    explicit TabWidget(DocumentManager *documentManager, QWidget *parent = nullptr);
    ~TabWidget();

    // EditorViewManager interface
    SciEdit *currentEditor() const override;
    int currentDocumentId() const override;
    int docIdForEditor(SciEdit *editor) const override;
    SciEdit *editorForDocId(int docId) const override;
    QVector<SciEdit *> editorsForDocId(int docId) const override;
    int count() const override;
    QVector<SciEdit *> editors() const override;
    QVector<int> documentIds() const override;
    QWidget *viewWidget() const override;

    void addEditor(int docId, SciEdit *editor, bool activate = true, NewTabPlacement placement = NewTabPlacement::AppendToEnd) override;
    void removeEditor(SciEdit *editor) override;
    void removeAllEditorsForDoc(int docId) override;
    void switchToEditor(SciEdit *editor) override;
    void refreshEditor(SciEdit *editor) override;
    QVector<SciEdit *> editorsInSameTabGroup(SciEdit *editor) const override;

    // Tab-specific (for SessionManager)
    SciEdit *editorAt(int index) const;
    int documentIdAt(int index) const;
    int findTabByDocumentId(int docId) const;

    void updateTabTitle(int tabIndex);

signals:
    void currentEditorChanged(SciEdit *editor);
    void filesDropped(const QStringList &paths);
    void editorCloseRequested(int docId);
    void contextMenuRequested(SciEdit *editor, int docId);
    void newDocumentRequested();
    void bookmarkMarginContextMenuRequested(SciEdit *editor, const QPoint &globalPos);

private slots:
    void onCurrentChanged(int index);
    void onDocumentModified(int docId, bool modified);
    void onDocumentTitleChanged(int docId, const QString &title);
    void onQuickOpenTriggered();

private:
    DocumentManager *m_documentManager;
    QHash<int, SciEdit*> m_editors;

    int removeTabAt(int index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};
