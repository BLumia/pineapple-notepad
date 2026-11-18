#pragma once

#include <QWidget>
#include <QList>
#include <QSplitter>
#include <QVector>
#include <QStringList>

#include "editorviewmanager.h"

class QSplitter;
class TabWidget;
class SciEdit;
class DocumentManager;
class QContextMenuEvent;

/*
 * The splitter between the two views. Context menu and double click events
 * on the splitter handle are not consumed by QSplitterHandle and propagate
 * up to the splitter.
 */
class EditorAreaSplitter : public QSplitter
{
    Q_OBJECT

public:
    explicit EditorAreaSplitter(QWidget *parent)
        : QSplitter(Qt::Horizontal, parent)
    {
    }

signals:
    void contextMenuRequested(const QPoint &globalPos);
    void handleDoubleClicked();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};

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

    // Scintilla's caret-blink ("focused") state is per-editor and independent
    // of Qt widget focus. Keep it set on at most one editor at a time; pass
    // nullptr to clear it everywhere.
    void setFocusedEditor(SciEdit *editor);

    // Split layout (only meaningful with two groups). Qt::Horizontal places
    // the views side by side, Qt::Vertical stacks them. "firstGroup" is the
    // tab group shown first (left/top); group indexes are logical and do not
    // change with the layout order.
    Qt::Orientation splitOrientation() const;
    int firstTabGroupInLayout() const;
    void setSplitLayout(Qt::Orientation orientation, int firstGroup);
    // Reset the split to an even distribution.
    void equalizeSplit();
    // Exchange the two views, keeping the current orientation (the sizes
    // follow the widgets).
    void swapSplitViews();
    // Rotate the layout by 90 degrees like the splitter context menu of
    // other editors: clockwise==true rotates right; the rotation decides
    // whether the first view ends up first again (so both the direction and
    // the view order can change).
    void rotateSplit(bool clockwise);

signals:
    void currentEditorChanged(SciEdit *editor);
    void filesDropped(const QStringList &paths);
    void editorCloseRequested(SciEdit *editor, int docId);
    void contextMenuRequested(SciEdit *editor, int docId);
    void newDocumentRequested(int group);
    void bookmarkMarginContextMenuRequested(SciEdit *editor, const QPoint &globalPos);
    void activeTabGroupChanged(int group);
    // A tab was added to a group / is about to be removed from one. Moving a
    // tab between views recreates the editor object, so consumers holding a
    // SciEdit pointer must re-resolve by docId (or reconnect here) on change.
    void editorAdded(int docId, SciEdit *editor, int group);
    void editorAboutToBeRemoved(int docId, SciEdit *editor, int group);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    TabWidget *groupAt(int group) const;
    void connectTabWidget(TabWidget *tab);
    void emitCurrentEditorChanged();
    void wrapInSplitter();
    void unwrapSplitter();
    void collapseSplitIfSideEmpty();
    void showSplitterContextMenu(const QPoint &globalPos);

    DocumentManager *m_documentManager = nullptr;
    QList<TabWidget *> m_tabGroups;
    QSplitter *m_splitter = nullptr;
    int m_activeGroup = 0;
};
