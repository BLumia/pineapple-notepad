#include "editorarea.h"

#include "sciedit.h"
#include "tabwidget.h"

#include <QEvent>
#include <QSplitter>
#include <QVBoxLayout>

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

EditorArea::~EditorArea() = default;

TabWidget *EditorArea::groupAt(int group) const
{
    return (group >= 0 && group < m_tabGroups.size()) ? m_tabGroups.at(group) : nullptr;
}

void EditorArea::connectTabWidget(TabWidget *tab)
{
    connect(tab, &TabWidget::filesDropped, this, &EditorArea::filesDropped);
    connect(tab, &TabWidget::editorCloseRequested, this, &EditorArea::editorCloseRequested);
    connect(tab, &TabWidget::contextMenuRequested, this, &EditorArea::contextMenuRequested);
    connect(tab, &TabWidget::newDocumentRequested, this, &EditorArea::newDocumentRequested);
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

void EditorArea::removeSubViewIfEmpty()
{
    if (m_tabGroups.size() < 2) {
        return;
    }
    TabWidget *subView = m_tabGroups.at(1);
    if (subView->count() != 0) {
        return;
    }

    m_tabGroups.removeAt(1);
    subView->deleteLater();
    setActiveTabGroup(0);
    unwrapSplitter();
}

void EditorArea::wrapInSplitter()
{
    if (m_splitter) {
        return;
    }
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(m_tabGroups.at(0));
    layout()->addWidget(m_splitter);
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
        removeSubViewIfEmpty();
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
    removeSubViewIfEmpty();
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

void EditorArea::setFocusedEditor(SciEdit *editor)
{
    for (TabWidget *group : std::as_const(m_tabGroups)) {
        const QVector<SciEdit *> groupEditors = group->editors();
        for (SciEdit *e : groupEditors) {
            e->setFocus(e == editor);
        }
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
