#include "editorarea.h"

#include "sciedit.h"
#include "tabwidget.h"

#include <QContextMenuEvent>
#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QSplitter>
#include <QVBoxLayout>

void EditorAreaSplitter::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextMenuRequested(event->globalPos());
    event->accept();
}

void EditorAreaSplitter::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit handleDoubleClicked();
        event->accept();
        return;
    }
    QSplitter::mouseDoubleClickEvent(event);
}

EditorArea::EditorArea(DocumentManager *documentManager, QWidget *parent)
    : QWidget(parent)
    , m_documentManager(documentManager)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *primary = new TabWidget(m_documentManager, this);
    layout->addWidget(primary);
    m_tabGroups.append(primary);
    connectTabWidget(primary);
}

EditorArea::~EditorArea()
{
    // The override cursor is application-global; never leave it behind.
    restoreTabDragCursor();
}

TabWidget *EditorArea::groupAt(int group) const
{
    return (group >= 0 && group < m_tabGroups.size()) ? m_tabGroups.at(group) : nullptr;
}

void EditorArea::connectTabWidget(TabWidget *tab)
{
    connect(tab, &TabWidget::filesDropped, this, &EditorArea::filesDropped);
    connect(tab, &TabWidget::editorCloseRequested, this, &EditorArea::editorCloseRequested);
    connect(tab, &TabWidget::contextMenuRequested, this, &EditorArea::contextMenuRequested);
    connect(tab, &TabWidget::newDocumentRequested, this, [this, tab]() {
        emit newDocumentRequested(m_tabGroups.indexOf(tab));
    });
    connect(tab, &TabWidget::tabDraggedOut, this, [this, tab](int tabIndex, const QPoint &globalPos) {
        emit tabDraggedOut(m_tabGroups.indexOf(tab), tabIndex, globalPos);
    });
    // Drag feedback: while the pressed tab hovers outside its tab bar, show
    // a drop hint cursor depending on what is under the mouse.
    connect(tab, &TabWidget::tabDragMoved, this,
            [this](int, const QPoint &globalPos, bool outsideTabBar) {
        if (outsideTabBar) {
            updateTabDragCursor(globalPos);
        } else {
            restoreTabDragCursor();
        }
    });
    connect(tab, &TabWidget::tabDragFinished, this, &EditorArea::restoreTabDragCursor);
    connect(tab, &TabWidget::bookmarkMarginContextMenuRequested, this, &EditorArea::bookmarkMarginContextMenuRequested);
    // Switching to a tab of a group also makes that group the active one.
    // currentEditorChanged is emitted from the aggregated view (the active
    // group's current editor) so that background events of a group (e.g.
    // closing its last tab) do not announce a stale editor.
    connect(tab, &TabWidget::currentEditorChanged, this, [this, tab](SciEdit *) {
        const int group = m_tabGroups.indexOf(tab);
        if (group != m_activeGroup) {
            setActiveTabGroup(group);
        } else {
            emitCurrentEditorChanged();
        }
    });
}

void EditorArea::emitCurrentEditorChanged()
{
    emit currentEditorChanged(currentEditor());
}

int EditorArea::activeTabGroup() const
{
    return m_activeGroup;
}

void EditorArea::setActiveTabGroup(int group)
{
    if (group < 0 || group >= m_tabGroups.size() || group == m_activeGroup) {
        return;
    }
    m_activeGroup = group;
    emit activeTabGroupChanged(group);
    // The aggregated "current editor" changes with the active group.
    emitCurrentEditorChanged();
}

bool EditorArea::subViewVisible() const
{
    return m_tabGroups.size() > 1;
}

void EditorArea::ensureSubView()
{
    if (m_tabGroups.size() > 1) {
        return;
    }

    wrapInSplitter();

    auto *subView = new TabWidget(m_documentManager, this);
    m_splitter->addWidget(subView);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_tabGroups.append(subView);
    connectTabWidget(subView);

    // Give both views the same size. Equal stretch factors only distribute
    // extra space on later resizes; the initial split follows the widgets'
    // current sizes (the primary view joins the splitter with the full width).
    int total = m_splitter->width();
    if (total <= 0) {
        total = m_splitter->sizeHint().width();
    }
    if (total > 0) {
        m_splitter->setSizes({total / 2, total - total / 2});
    }
}

void EditorArea::collapseSplitIfSideEmpty()
{
    if (m_tabGroups.size() < 2) {
        return;
    }
    TabWidget *primary = m_tabGroups.at(0);
    TabWidget *secondary = m_tabGroups.at(1);
    const bool primaryEmpty = (primary->count() == 0);
    const bool secondaryEmpty = (secondary->count() == 0);

    if (secondaryEmpty) {
        // The secondary view became empty (or nothing is left at all):
        // drop it and keep the canonical primary view.
        m_tabGroups.removeAt(1);
        secondary->deleteLater();
        setActiveTabGroup(0);
        unwrapSplitter();
        return;
    }

    if (primaryEmpty) {
        // The primary side became empty (e.g. its tabs were closed while it
        // was shown as the second side after a swap): promote the secondary
        // view to the primary slot so the split collapses to a single view.
        m_tabGroups[0] = secondary;
        m_tabGroups.removeAt(1);
        setActiveTabGroup(0);
        // Reparents the promoted view out of the splitter; deleting the
        // splitter also deletes the emptied old primary view.
        unwrapSplitter();
        // The surviving current editor may not have been announced (e.g.
        // when the emptied side was the active one).
        emitCurrentEditorChanged();
    }
}

void EditorArea::wrapInSplitter()
{
    if (m_splitter) {
        return;
    }
    auto *splitter = new EditorAreaSplitter(this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(m_tabGroups.at(0));
    connect(splitter, &EditorAreaSplitter::contextMenuRequested,
            this, &EditorArea::showSplitterContextMenu);
    connect(splitter, &EditorAreaSplitter::handleDoubleClicked,
            this, &EditorArea::equalizeSplit);
    layout()->addWidget(splitter);
    m_splitter = splitter;
}

void EditorArea::unwrapSplitter()
{
    if (!m_splitter) {
        return;
    }
    QSplitter *splitter = m_splitter;
    m_splitter = nullptr;
    TabWidget *primary = m_tabGroups.value(0);
    primary->setParent(this);
    layout()->addWidget(primary);
    splitter->deleteLater();
}

Qt::Orientation EditorArea::splitOrientation() const
{
    return m_splitter ? m_splitter->orientation() : Qt::Horizontal;
}

int EditorArea::firstTabGroupInLayout() const
{
    if (!m_splitter || m_tabGroups.size() < 2) {
        return 0;
    }
    return (m_splitter->widget(0) == m_tabGroups.at(1)) ? 1 : 0;
}

void EditorArea::setSplitLayout(Qt::Orientation orientation, int firstGroup)
{
    if (m_tabGroups.size() < 2 || !m_splitter) {
        return;
    }
    if (firstGroup != 0 && firstGroup != 1) {
        return;
    }

    const int currentFirst = firstTabGroupInLayout();
    const QList<int> oldSizes = m_splitter->sizes();
    const bool hasSizes = (oldSizes.size() == 2 && (oldSizes.at(0) > 0 || oldSizes.at(1) > 0));

    m_splitter->setOrientation(orientation);
    if (firstGroup != currentFirst) {
        // The sizes belong to the widgets, so they swap along with them.
        m_splitter->insertWidget(0, m_splitter->widget(1));
        if (hasSizes) {
            m_splitter->setSizes({oldSizes.at(1), oldSizes.at(0)});
        }
    } else if (hasSizes) {
        m_splitter->setSizes(oldSizes);
    }
}

void EditorArea::swapSplitViews()
{
    if (m_tabGroups.size() < 2 || !m_splitter) {
        return;
    }
    setSplitLayout(m_splitter->orientation(), 1 - firstTabGroupInLayout());
}

void EditorArea::rotateSplit(bool clockwise)
{
    if (m_tabGroups.size() < 2 || !m_splitter) {
        return;
    }
    const bool sideBySide = (m_splitter->orientation() == Qt::Horizontal);
    const Qt::Orientation target = sideBySide ? Qt::Vertical : Qt::Horizontal;
    const int first = firstTabGroupInLayout();
    // Rotating clockwise keeps the leading view leading when starting side
    // by side (left becomes top), and swaps it when starting stacked (top
    // becomes right); counter-clockwise behaves the other way round.
    const bool swapOrder = sideBySide ? !clockwise : clockwise;
    setSplitLayout(target, swapOrder ? 1 - first : first);
}

void EditorArea::showSplitterContextMenu(const QPoint &globalPos)
{
    if (m_tabGroups.size() < 2) {
        return;
    }
    QMenu menu;
    QAction *rotateLeft = menu.addAction(tr("Rotate to Left"));
    QAction *rotateRight = menu.addAction(tr("Rotate to Right"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == rotateLeft) {
        rotateSplit(false);
    } else if (chosen == rotateRight) {
        rotateSplit(true);
    }
}

void EditorArea::equalizeSplit()
{
    if (!m_splitter || m_splitter->count() != 2) {
        return;
    }
    const bool sideBySide = (m_splitter->orientation() == Qt::Horizontal);
    int total = sideBySide ? m_splitter->width() : m_splitter->height();
    if (total <= 0) {
        const QSize hint = m_splitter->sizeHint();
        total = sideBySide ? hint.width() : hint.height();
    }
    if (total > 0) {
        m_splitter->setSizes({total / 2, total - total / 2});
    }
}

// EditorViewManager interface

SciEdit *EditorArea::currentEditor() const
{
    if (TabWidget *group = groupAt(m_activeGroup)) {
        return group->currentEditor();
    }
    return nullptr;
}

int EditorArea::currentDocumentId() const
{
    if (TabWidget *group = groupAt(m_activeGroup)) {
        return group->currentDocumentId();
    }
    return -1;
}

int EditorArea::docIdForEditor(SciEdit *editor) const
{
    if (TabWidget *group = groupAt(groupOfEditor(editor))) {
        return group->docIdForEditor(editor);
    }
    return -1;
}

SciEdit *EditorArea::editorForDocId(int docId) const
{
    // Prefer an editor in the active group, fall back to any group.
    if (TabWidget *group = groupAt(m_activeGroup)) {
        if (SciEdit *editor = group->editorForDocId(docId)) {
            return editor;
        }
    }
    for (TabWidget *group : std::as_const(m_tabGroups)) {
        if (SciEdit *editor = group->editorForDocId(docId)) {
            return editor;
        }
    }
    return nullptr;
}

QVector<SciEdit *> EditorArea::editorsForDocId(int docId) const
{
    QVector<SciEdit *> result;
    for (TabWidget *group : std::as_const(m_tabGroups)) {
        if (SciEdit *editor = group->editorForDocId(docId)) {
            result.append(editor);
        }
    }
    return result;
}

int EditorArea::count() const
{
    int total = 0;
    for (TabWidget *group : std::as_const(m_tabGroups)) {
        total += group->count();
    }
    return total;
}

QVector<SciEdit *> EditorArea::editors() const
{
    QVector<SciEdit *> result;
    for (TabWidget *group : std::as_const(m_tabGroups)) {
        result += group->editors();
    }
    return result;
}

QVector<int> EditorArea::documentIds() const
{
    // Group 0 order first, then group 1; a document cloned into both groups
    // is reported once (at its first occurrence).
    QVector<int> result;
    for (TabWidget *group : std::as_const(m_tabGroups)) {
        const QVector<int> docIds = group->documentIds();
        for (int docId : docIds) {
            if (docId >= 0 && !result.contains(docId)) {
                result.append(docId);
            }
        }
    }
    return result;
}

QWidget *EditorArea::viewWidget() const
{
    // New editors are created with this widget as parent; the receiving
    // tab group reparents them when the tab is added.
    return m_tabGroups.value(0);
}

void EditorArea::addEditor(int docId, SciEdit *editor, bool activate, NewTabPlacement placement)
{
    addEditorToGroup(m_activeGroup, docId, editor, activate, placement);
}

void EditorArea::addEditorToGroup(int group, int docId, SciEdit *editor, bool activate, NewTabPlacement placement)
{
    TabWidget *tabGroup = groupAt(group);
    if (!tabGroup) {
        tabGroup = groupAt(m_activeGroup);
    }
    Q_ASSERT(tabGroup);
    if (!tabGroup) {
        return;
    }

    tabGroup->addEditor(docId, editor, activate, placement);
    editor->installEventFilter(this);
    emit editorAdded(docId, editor, m_tabGroups.indexOf(tabGroup));
}

void EditorArea::removeEditor(SciEdit *editor)
{
    const int group = groupOfEditor(editor);
    if (TabWidget *tabGroup = groupAt(group)) {
        emit editorAboutToBeRemoved(tabGroup->docIdForEditor(editor), editor, group);
        tabGroup->removeEditor(editor);
        collapseSplitIfSideEmpty();
    }
}

void EditorArea::removeAllEditorsForDoc(int docId)
{
    for (int i = 0; i < m_tabGroups.size(); ++i) {
        if (SciEdit *editor = m_tabGroups.at(i)->editorForDocId(docId)) {
            emit editorAboutToBeRemoved(docId, editor, i);
        }
    }
    for (TabWidget *group : std::as_const(m_tabGroups)) {
        group->removeAllEditorsForDoc(docId);
    }
    collapseSplitIfSideEmpty();
}

void EditorArea::switchToEditor(SciEdit *editor)
{
    const int group = groupOfEditor(editor);
    if (TabWidget *tabGroup = groupAt(group)) {
        setActiveTabGroup(group);
        tabGroup->switchToEditor(editor);
    }
}

QVector<SciEdit *> EditorArea::editorsInSameTabGroup(SciEdit *editor) const
{
    if (TabWidget *group = groupAt(groupOfEditor(editor))) {
        return group->editorsInSameTabGroup(editor);
    }
    return {};
}

void EditorArea::refreshEditor(SciEdit *editor)
{
    if (TabWidget *group = groupAt(groupOfEditor(editor))) {
        group->refreshEditor(editor);
    }
}

void EditorArea::activateNextTab()
{
    if (TabWidget *group = groupAt(m_activeGroup)) {
        group->activateNextTab();
    }
}

void EditorArea::activatePreviousTab()
{
    if (TabWidget *group = groupAt(m_activeGroup)) {
        group->activatePreviousTab();
    }
}

void EditorArea::activateTabAtIndex(int index)
{
    if (TabWidget *group = groupAt(m_activeGroup)) {
        group->activateTabAtIndex(index);
    }
}

// EditorViewManager interface: per tab-group addressing

int EditorArea::tabGroupCount() const
{
    return m_tabGroups.size();
}

QVector<int> EditorArea::documentIdsInGroup(int group) const
{
    if (TabWidget *tabGroup = groupAt(group)) {
        return tabGroup->documentIds();
    }
    return {};
}

int EditorArea::activeIndexInGroup(int group) const
{
    if (TabWidget *tabGroup = groupAt(group)) {
        return tabGroup->currentIndex();
    }
    return -1;
}

SciEdit *EditorArea::currentEditorInGroup(int group) const
{
    if (TabWidget *tabGroup = groupAt(group)) {
        return tabGroup->currentEditor();
    }
    return nullptr;
}

SciEdit *EditorArea::editorInGroup(int group, int docId) const
{
    if (TabWidget *tabGroup = groupAt(group)) {
        return tabGroup->editorForDocId(docId);
    }
    return nullptr;
}

int EditorArea::groupOfEditor(SciEdit *editor) const
{
    for (int i = 0; i < m_tabGroups.size(); ++i) {
        if (m_tabGroups.at(i)->groupOfEditor(editor) == 0) {
            return i;
        }
    }
    return -1;
}

int EditorArea::groupForWidget(const QWidget *widget) const
{
    if (!widget) {
        return -1;
    }
    for (int i = 0; i < m_tabGroups.size(); ++i) {
        if (m_tabGroups.at(i) == widget || m_tabGroups.at(i)->isAncestorOf(widget)) {
            return i;
        }
    }
    return -1;
}

TabWidget *EditorArea::tabGroupAt(int group) const
{
    return groupAt(group);
}

void EditorArea::setFocusedEditor(SciEdit *editor)
{
    for (TabWidget *group : std::as_const(m_tabGroups)) {
        const QVector<SciEdit *> groupEditors = group->editors();
        for (SciEdit *e : groupEditors) {
            e->setFocus(e == editor);
        }
    }
}

void EditorArea::updateTabDragCursor(const QPoint &globalPos)
{
    // The drop target resolution needs a real (visible, unobscured) window,
    // which is always the case for an actual drag within this application.
    const bool copy = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier);
    const Qt::CursorShape shape = tabDragCursorShape(QApplication::widgetAt(globalPos), copy);

    if (m_tabDragCursorActive) {
        QApplication::changeOverrideCursor(QCursor(shape));
    } else {
        QApplication::setOverrideCursor(QCursor(shape));
        m_tabDragCursorActive = true;
    }
}

Qt::CursorShape EditorArea::tabDragCursorShape(const QWidget *dropTarget, bool copyModifier) const
{
    // A view (of this window) accepts the drop: moving the tab, or cloning
    // it with Ctrl held. Anywhere else — dock widgets, other panels, outside
    // the window — currently refuses it.
    if (groupForWidget(dropTarget) >= 0) {
        return copyModifier ? Qt::DragCopyCursor : Qt::DragMoveCursor;
    }
    return Qt::ForbiddenCursor;
}

void EditorArea::restoreTabDragCursor()
{
    if (m_tabDragCursorActive) {
        QApplication::restoreOverrideCursor();
        m_tabDragCursorActive = false;
    }
}

bool EditorArea::eventFilter(QObject *watched, QEvent *event)
{
    // An editor gaining focus activates its tab group (clicking into an
    // already-current tab does not emit currentEditorChanged).
    if (event->type() == QEvent::FocusIn) {
        if (auto *editor = qobject_cast<SciEdit *>(watched)) {
            const int group = groupOfEditor(editor);
            if (group >= 0) {
                setActiveTabGroup(group);
                // The freshly focused editor is the only one that may keep
                // the caret blinking (switching the current editor or group
                // programmatically never sends focus events to the others).
                setFocusedEditor(editor);
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
