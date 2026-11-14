#include "editorarea.h"

#include "sciedit.h"
#include "tabwidget.h"

#include <QVBoxLayout>

EditorArea::EditorArea(DocumentManager *documentManager, QWidget *parent)
    : QWidget(parent)
    , m_tabWidget(new TabWidget(documentManager, this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabWidget);

    connect(m_tabWidget, &TabWidget::currentEditorChanged, this, &EditorArea::currentEditorChanged);
    connect(m_tabWidget, &TabWidget::filesDropped, this, &EditorArea::filesDropped);
    connect(m_tabWidget, &TabWidget::editorCloseRequested, this, &EditorArea::editorCloseRequested);
    connect(m_tabWidget, &TabWidget::contextMenuRequested, this, &EditorArea::contextMenuRequested);
    connect(m_tabWidget, &TabWidget::newDocumentRequested, this, &EditorArea::newDocumentRequested);
    connect(m_tabWidget, &TabWidget::bookmarkMarginContextMenuRequested, this, &EditorArea::bookmarkMarginContextMenuRequested);
}

EditorArea::~EditorArea() = default;

// EditorViewManager interface (currently delegated to the single tab group)

SciEdit *EditorArea::currentEditor() const
{
    return m_tabWidget->currentEditor();
}

int EditorArea::currentDocumentId() const
{
    return m_tabWidget->currentDocumentId();
}

int EditorArea::docIdForEditor(SciEdit *editor) const
{
    return m_tabWidget->docIdForEditor(editor);
}

SciEdit *EditorArea::editorForDocId(int docId) const
{
    return m_tabWidget->editorForDocId(docId);
}

QVector<SciEdit *> EditorArea::editorsForDocId(int docId) const
{
    return m_tabWidget->editorsForDocId(docId);
}

int EditorArea::count() const
{
    return m_tabWidget->count();
}

QVector<SciEdit *> EditorArea::editors() const
{
    return m_tabWidget->editors();
}

QVector<int> EditorArea::documentIds() const
{
    return m_tabWidget->documentIds();
}

QWidget *EditorArea::viewWidget() const
{
    return m_tabWidget;
}

void EditorArea::addEditor(int docId, SciEdit *editor, bool activate, NewTabPlacement placement)
{
    m_tabWidget->addEditor(docId, editor, activate, placement);
}

void EditorArea::removeEditor(SciEdit *editor)
{
    m_tabWidget->removeEditor(editor);
}

void EditorArea::removeAllEditorsForDoc(int docId)
{
    m_tabWidget->removeAllEditorsForDoc(docId);
}

void EditorArea::switchToEditor(SciEdit *editor)
{
    m_tabWidget->switchToEditor(editor);
}

QVector<SciEdit *> EditorArea::editorsInSameTabGroup(SciEdit *editor) const
{
    return m_tabWidget->editorsInSameTabGroup(editor);
}

void EditorArea::refreshEditor(SciEdit *editor)
{
    m_tabWidget->refreshEditor(editor);
}

void EditorArea::activateNextTab()
{
    m_tabWidget->activateNextTab();
}

void EditorArea::activatePreviousTab()
{
    m_tabWidget->activatePreviousTab();
}

void EditorArea::activateTabAtIndex(int index)
{
    m_tabWidget->activateTabAtIndex(index);
}
