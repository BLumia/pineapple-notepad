#pragma once

#include <QWidget>
#include <QVector>
#include <QStringList>

#include "editorviewmanager.h"

class TabWidget;
class SciEdit;
class DocumentManager;

/*
 * The central widget of a MainWindow, hosting one or more tab groups
 * (TabWidget) and aggregating the EditorViewManager interface over them.
 *
 * For now it hosts a single TabWidget and simply forwards its signals,
 * decoupling MainWindow from QTabWidget specifics. Hosting a second tab
 * group inside a QSplitter (dual view) will be added in later steps.
 */
class EditorArea : public QWidget, public EditorViewManager
{
    Q_OBJECT

public:
    explicit EditorArea(DocumentManager *documentManager, QWidget *parent = nullptr);
    ~EditorArea() override;

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
    QVector<SciEdit *> editorsInSameTabGroup(SciEdit *editor) const override;
    void refreshEditor(SciEdit *editor) override;

    void activateNextTab() override;
    void activatePreviousTab() override;
    void activateTabAtIndex(int index) override;

signals:
    void currentEditorChanged(SciEdit *editor);
    void filesDropped(const QStringList &paths);
    void editorCloseRequested(int docId);
    void contextMenuRequested(SciEdit *editor, int docId);
    void newDocumentRequested();
    void bookmarkMarginContextMenuRequested(SciEdit *editor, const QPoint &globalPos);

private:
    TabWidget *m_tabWidget;
};
