#include "tabwidget.h"
#include "sciedit.h"
#include "documentmanager.h"

#include <QMenu>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QIcon>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QTabBar>
#include <QToolButton>

Q_LOGGING_CATEGORY(lcTabWidget, "pnotepad.tabwidget")

TabWidget::TabWidget(DocumentManager *documentManager, QWidget *parent)
    : QTabWidget(parent)
    , m_documentManager(documentManager)
{
    setTabsClosable(true);
    setMovable(true);
    setUsesScrollButtons(true);

    auto * quickopen = new QToolButton(this);
    quickopen->setIcon(QIcon::fromTheme("quickopen"));
    quickopen->setAutoRaise(true);
    quickopen->setToolTip(tr("Quick Open"));
    setCornerWidget(quickopen);
    quickopen->setVisible(false);

    connect(quickopen, &QToolButton::clicked, this, &TabWidget::onQuickOpenTriggered);
    connect(this, &QTabWidget::currentChanged, this, &TabWidget::onCurrentChanged);
    connect(this, &QTabWidget::tabCloseRequested, this, [this](int index) {
        const int docId = documentIdAt(index);
        if (docId >= 0) {
            emit editorCloseRequested(docId);
        }
    });

    connect(m_documentManager, &DocumentManager::documentModified,
            this, &TabWidget::onDocumentModified);
    connect(m_documentManager, &DocumentManager::documentTitleChanged,
            this, &TabWidget::onDocumentTitleChanged);

    if (QTabBar *bar = tabBar()) {
        connect(bar, &QTabBar::tabMoved, this, [this](int from, int to) {
            m_documentManager->moveDocument(from, to);
        });
        bar->installEventFilter(this);
        if (QWidget *barParent = bar->parentWidget()) {
            barParent->installEventFilter(this);
        }
    }
}

TabWidget::~TabWidget()
{
}

int TabWidget::removeTabAt(int index)
{
    const int docId = documentIdAt(index);
    m_editors.remove(docId);

    QWidget *w = widget(index);
    removeTab(index);
    if (w) {
        w->deleteLater();
    }

    return docId;
}

SciEdit *TabWidget::currentEditor() const
{
    return qobject_cast<SciEdit *>(currentWidget());
}

SciEdit *TabWidget::editorAt(int index) const
{
    return qobject_cast<SciEdit *>(widget(index));
}

SciEdit *TabWidget::editorForDocId(int docId) const
{
    return m_editors.value(docId, nullptr);
}

int TabWidget::currentDocumentId() const
{
    return docIdForEditor(currentEditor());
}

int TabWidget::documentIdAt(int index) const
{
    return docIdForEditor(editorAt(index));
}

int TabWidget::findTabByDocumentId(int docId) const
{
    for (int i = 0; i < count(); ++i) {
        if (documentIdAt(i) == docId) {
            return i;
        }
    }

    return -1;
}

QVector<int> TabWidget::documentIds() const
{
    QVector<int> result;
    for (int i = 0; i < count(); ++i) {
        const int docId = documentIdAt(i);
        if (docId >= 0) {
            result.append(docId);
        }
    }
    return result;
}

QVector<SciEdit *> TabWidget::editors() const
{
    QVector<SciEdit *> result;
    for (int i = 0; i < count(); ++i) {
        if (SciEdit *editor = editorAt(i)) {
            result.append(editor);
        }
    }
    return result;
}

int TabWidget::docIdForEditor(SciEdit *editor) const
{
    if (!editor) {
        return -1;
    }

    const QVariant docIdProperty = editor->QObject::property("documentId");
    if (docIdProperty.isValid()) {
        return docIdProperty.toInt();
    }

    for (auto it = m_editors.begin(); it != m_editors.end(); ++it) {
        if (it.value() == editor) {
            return it.key();
        }
    }

    return -1;
}

int TabWidget::count() const
{
    return QTabWidget::count();
}

QWidget *TabWidget::viewWidget() const
{
    return const_cast<TabWidget*>(this);
}

QVector<SciEdit *> TabWidget::editorsForDocId(int docId) const
{
    SciEdit *editor = editorForDocId(docId);
    if (editor) {
        return {editor};
    }
    return {};
}

void TabWidget::addEditor(int docId, SciEdit *editor, bool activate, NewTabPlacement placement)
{
    Q_ASSERT(editor != nullptr);
    Q_ASSERT(docId >= 0);

    m_editors[docId] = editor;

    connect(editor, &SciEdit::filesDropped, this, &TabWidget::filesDropped);
    connect(editor, &SciEdit::bookmarkMarginContextMenuRequested, this,
            [this, editor](const QPoint &globalPos) {
        emit bookmarkMarginContextMenuRequested(editor, globalPos);
    });

    QString title = m_documentManager->getDocumentTitle(docId);
    int tabIndex;
    if (placement == NewTabPlacement::AfterCurrent) {
        const int cur = currentIndex();
        tabIndex = insertTab(cur < 0 ? count() : cur + 1, editor, title);
    } else {
        tabIndex = addTab(editor, title);
    }
    updateTabTitle(tabIndex);

    if (activate) {
        setCurrentIndex(tabIndex);
        emit currentEditorChanged(editor);
    }
}

void TabWidget::removeEditor(SciEdit *editor)
{
    if (!editor) return;
    const int index = indexOf(editor);
    if (index >= 0) {
        removeTabAt(index);
    }
}

void TabWidget::removeAllEditorsForDoc(int docId)
{
    const int index = findTabByDocumentId(docId);
    if (index >= 0) {
        removeTabAt(index);
    }
}

void TabWidget::switchToEditor(SciEdit *editor)
{
    if (!editor) return;
    const int index = indexOf(editor);
    if (index >= 0) {
        setCurrentIndex(index);
    }
}

void TabWidget::refreshEditor(SciEdit *editor)
{
    if (!editor) return;
    const int docId = docIdForEditor(editor);
    if (docId >= 0) {
        const int index = findTabByDocumentId(docId);
        if (index >= 0) {
            updateTabTitle(index);
        }
    }
}

QVector<SciEdit *> TabWidget::editorsInSameTabGroup(SciEdit *editor) const
{
    Q_UNUSED(editor);
    return editors();
}

void TabWidget::onCurrentChanged(int index)
{
    qCDebug(lcTabWidget) << "Current tab changed to index:" << index;
    SciEdit *editor = editorAt(index);
    emit currentEditorChanged(editor);
}

void TabWidget::onDocumentModified(int docId, bool modified)
{
    Q_UNUSED(modified)
    refreshEditorsForDoc(docId);
}

void TabWidget::onDocumentTitleChanged(int docId, const QString &title)
{
    Q_UNUSED(title)
    refreshEditorsForDoc(docId);
}

void TabWidget::onQuickOpenTriggered()
{
    QMessageBox::information(this, "Quick Open", "This feature is not implemented yet...");
}

void TabWidget::updateTabTitle(int tabIndex)
{
    const int docId = documentIdAt(tabIndex);
    if (docId == -1) {
        return;
    }
    const QModelIndex idx = m_documentManager->indexForDocId(docId);
    if (!idx.isValid()) {
        return;
    }
    setTabText(tabIndex, idx.data(Qt::DisplayRole).toString());
    setTabToolTip(tabIndex, idx.data(Qt::ToolTipRole).toString());
    setTabIcon(tabIndex, idx.data(Qt::DecorationRole).value<QIcon>());
}

bool TabWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == tabBar() && event->type() == QEvent::ContextMenu) {
        auto *contextMenuEvent = static_cast<QContextMenuEvent *>(event);
        const int tabIndex = tabBar()->tabAt(contextMenuEvent->pos());
        if (tabIndex < 0) {
            return false;
        }

        const int docId = documentIdAt(tabIndex);
        SciEdit *editor = editorAt(tabIndex);
        emit contextMenuRequested(editor, docId);
        return true;
    }

    if (watched == tabBar()->parentWidget() && event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            emit newDocumentRequested();
            return true;
        }
    }

    if (watched == tabBar() && event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::MiddleButton) {
            const int tabIndex = tabBar()->tabAt(mouseEvent->position().toPoint());
            if (tabIndex >= 0) {
                const int docId = documentIdAt(tabIndex);
                if (docId >= 0) {
                    emit editorCloseRequested(docId);
                }
                return true;
            }
        }
    }

    return QTabWidget::eventFilter(watched, event);
}
