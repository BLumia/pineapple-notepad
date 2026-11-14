#pragma once

#include <QWidget>
#include <QList>
#include <QVector>
#include <QStringList>

#include "editorviewmanager.h"

class QSplitter;
class TabWidget;
class SciEdit;
class DocumentManager;

/*
 * The central widget of a MainWindow, hosting one or two tab groups
 * (TabWidget) and aggregating the EditorViewManager interface over them.
 *
 * Group 0 is the primary (always present) view; group 1 is the secondary
 * view ("other view"), created on demand via ensureSubView() and removed
 * again by removeSubViewIfEmpty() when it holds no more tabs. When both
 * groups exist they are laid out inside a QSplitter.
 *
 * One of the groups is the "active" group: the one whose editor is current.
 * EditorViewManager queries that resolve to a single editor
 * (currentEditor(), activateNextTab(), ...) act on the active group.
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

    // EditorViewManager interface: per tab-group addressing
    int tabGroupCount() const override;
    QVector<int> documentIdsInGroup(int group) const override;
    int activeIndexInGroup(int group) const override;
    SciEdit *currentEditorInGroup(int group) const override;
    SciEdit *editorInGroup(int group, int docId) const override;
    int groupOfEditor(SciEdit *editor) const override;

    // Tab group management
    int activeTabGroup() const;
    void setActiveTabGroup(int group);
    void ensureSubView();
    bool subViewVisible() const;
    void removeSubViewIfEmpty();
    void addEditorToGroup(int group, int docId, SciEdit *editor, bool activate = true,
                          NewTabPlacement placement = NewTabPlacement::AppendToEnd);

signals:
    void currentEditorChanged(SciEdit *editor);
    void filesDropped(const QStringList &paths);
    void editorCloseRequested(int docId);
    void contextMenuRequested(SciEdit *editor, int docId);
    void newDocumentRequested();
    void bookmarkMarginContextMenuRequested(SciEdit *editor, const QPoint &globalPos);
    void activeTabGroupChanged(int group);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    TabWidget *groupAt(int group) const;
    void connectTabWidget(TabWidget *tab);
    void wrapInSplitter();
    void unwrapSplitter();

    DocumentManager *m_documentManager = nullptr;
    QList<TabWidget *> m_tabGroups;
    QSplitter *m_splitter = nullptr;
    int m_activeGroup = 0;
};
