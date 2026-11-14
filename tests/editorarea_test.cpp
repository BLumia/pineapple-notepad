/*
 * Unit tests for the EditorArea tab-group aggregation (SPLITVIEW.md Step 2).
 *
 * EditorArea and TabWidget live in the application target, not in
 * pineapple-notepad-lib, so their sources are compiled into this test
 * target directly.
 */
#include "document.h"
#include "documentmanager.h"
#include "editorarea.h"
#include "sciedit.h"
#include "tabwidget.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

using Ids = QVector<int>;

class EditorAreaTest : public QObject
{
    Q_OBJECT

private:
    DocumentManager *m_documentManager = nullptr;
    EditorArea *m_area = nullptr;

    // Mirrors MainWindow::createEditor() closely enough for the container
    // tests: docId property set and the Scintilla document attached (shared
    // doc pointer for a second editor of the same document).
    SciEdit *createEditorForDoc(int docId)
    {
        auto *editor = new SciEdit(m_area->viewWidget());
        editor->QObject::setProperty("documentId", docId);
        if (Document *doc = m_documentManager->getDocument(docId)) {
            doc->addEditorReference(editor);
        }
        return editor;
    }

    int addDocumentToGroup(int group)
    {
        const int docId = m_documentManager->createNewDocument();
        m_area->addEditorToGroup(group, docId, createEditorForDoc(docId));
        return docId;
    }

private slots:
    void init();
    void cleanup();
    void singleGroupBehavior();
    void subViewLifecycle();
    void aggregatedDocumentIdsAndCount();
    void activeGroupTracking();
    void clonedDocQueries();
    void removalAggregation();
};

void EditorAreaTest::init()
{
    m_documentManager = new DocumentManager;
    m_area = new EditorArea(m_documentManager);
}

void EditorAreaTest::cleanup()
{
    delete m_area;
    m_area = nullptr;
    delete m_documentManager;
    m_documentManager = nullptr;
}

void EditorAreaTest::singleGroupBehavior()
{
    QCOMPARE(m_area->tabGroupCount(), 1);
    QCOMPARE(m_area->subViewVisible(), false);
    QCOMPARE(m_area->activeTabGroup(), 0);
    QCOMPARE(m_area->count(), 0);

    const int docA = addDocumentToGroup(0);
    const int docB = addDocumentToGroup(0);
    QCOMPARE(m_area->count(), 2);
    QCOMPARE(m_area->documentIds(), Ids({docA, docB}));
    QCOMPARE(m_area->currentDocumentId(), docB); // the last activated tab

    m_area->activateNextTab();
    QCOMPARE(m_area->currentDocumentId(), docA); // wraps around
    m_area->activateNextTab();
    QCOMPARE(m_area->currentDocumentId(), docB);

    m_area->activatePreviousTab();
    QCOMPARE(m_area->currentDocumentId(), docA);

    m_area->activateTabAtIndex(1);
    QCOMPARE(m_area->currentDocumentId(), docB);
    m_area->activateTabAtIndex(9); // out of range: no-op
    QCOMPARE(m_area->currentDocumentId(), docB);
}

void EditorAreaTest::subViewLifecycle()
{
    const int docA = addDocumentToGroup(0);
    m_area->ensureSubView();
    QCOMPARE(m_area->tabGroupCount(), 2);
    QCOMPARE(m_area->subViewVisible(), true);

    m_area->ensureSubView(); // idempotent
    QCOMPARE(m_area->tabGroupCount(), 2);

    const int docB = addDocumentToGroup(1);
    QCOMPARE(m_area->currentDocumentId(), docB);
    QCOMPARE(m_area->activeTabGroup(), 1);
    QCOMPARE(m_area->documentIdsInGroup(0), Ids({docA}));
    QCOMPARE(m_area->documentIdsInGroup(1), Ids({docB}));

    // Closing the sub view's only document hides the sub view again and
    // falls back to the primary view.
    m_area->removeAllEditorsForDoc(docB);
    QCOMPARE(m_area->tabGroupCount(), 1);
    QCOMPARE(m_area->subViewVisible(), false);
    QCOMPARE(m_area->activeTabGroup(), 0);
    QCOMPARE(m_area->currentDocumentId(), docA);
}

void EditorAreaTest::aggregatedDocumentIdsAndCount()
{
    const int docA = addDocumentToGroup(0);
    const int docB = addDocumentToGroup(0);
    m_area->ensureSubView();
    const int docC = addDocumentToGroup(1);

    QCOMPARE(m_area->documentIds(), Ids({docA, docB, docC}));
    QCOMPARE(m_area->documentIdsInGroup(0), Ids({docA, docB}));
    QCOMPARE(m_area->documentIdsInGroup(1), Ids({docC}));
    QCOMPARE(m_area->count(), 3);
    QCOMPARE(m_area->editors().size(), 3);

    // Out-of-range groups report empty/invalid values
    QCOMPARE(m_area->documentIdsInGroup(2), Ids());
    QCOMPARE(m_area->activeIndexInGroup(2), -1);
    QCOMPARE(m_area->currentEditorInGroup(2), nullptr);
    QCOMPARE(m_area->editorInGroup(2, docA), nullptr);
}

void EditorAreaTest::activeGroupTracking()
{
    const int docA = addDocumentToGroup(0);
    m_area->ensureSubView();
    const int docB = addDocumentToGroup(1);
    QCOMPARE(m_area->activeTabGroup(), 1);

    QSignalSpy spy(m_area, &EditorArea::activeTabGroupChanged);

    SciEdit *editorA = m_area->editorInGroup(0, docA);
    QVERIFY(editorA);
    m_area->switchToEditor(editorA);
    QCOMPARE(m_area->activeTabGroup(), 0);
    QCOMPARE(m_area->currentDocumentId(), docA);
    QCOMPARE(spy.size(), 1);

    SciEdit *editorB = m_area->editorInGroup(1, docB);
    QVERIFY(editorB);
    m_area->switchToEditor(editorB);
    QCOMPARE(m_area->activeTabGroup(), 1);
    QCOMPARE(m_area->currentDocumentId(), docB);
    QCOMPARE(spy.size(), 2);

    // setActiveTabGroup with an unchanged/invalid group emits nothing
    m_area->setActiveTabGroup(1);
    m_area->setActiveTabGroup(7);
    QCOMPARE(spy.size(), 2);
}

void EditorAreaTest::clonedDocQueries()
{
    const int docId = addDocumentToGroup(0);
    m_area->ensureSubView();
    SciEdit *cloned = createEditorForDoc(docId);
    m_area->addEditorToGroup(1, docId, cloned);

    // The same document shown in both groups
    QCOMPARE(m_area->editorsForDocId(docId).size(), 2);
    QCOMPARE(m_area->groupOfEditor(cloned), 1);
    QCOMPARE(m_area->groupOfEditor(m_area->editorInGroup(0, docId)), 0);

    // documentIds() reports the cloned document once, count() counts tabs
    QCOMPARE(m_area->documentIds(), Ids({docId}));
    QCOMPARE(m_area->count(), 2);

    // editorForDocId prefers the editor of the active group
    m_area->setActiveTabGroup(0);
    QCOMPARE(m_area->editorForDocId(docId), m_area->editorInGroup(0, docId));
    m_area->setActiveTabGroup(1);
    QCOMPARE(m_area->editorForDocId(docId), cloned);

    // editorsInSameTabGroup routes to the owning group only
    QCOMPARE(m_area->editorsInSameTabGroup(cloned).size(), 1);
    QCOMPARE(m_area->editorsInSameTabGroup(m_area->editorInGroup(0, docId)).size(), 1);
}

void EditorAreaTest::removalAggregation()
{
    const int docA = addDocumentToGroup(0);
    m_area->ensureSubView();
    const int docB = addDocumentToGroup(1);

    // Removing the sub view's only tab hides the sub view again
    SciEdit *editorB = m_area->editorInGroup(1, docB);
    QVERIFY(editorB);
    m_area->removeEditor(editorB);
    QCOMPARE(m_area->tabGroupCount(), 1);
    QCOMPARE(m_area->subViewVisible(), false);
    QCOMPARE(m_area->documentIds(), Ids({docA}));
    QCOMPARE(m_area->count(), 1);

    // Removing an editor that belongs to no group is a no-op
    auto *orphan = new SciEdit;
    m_area->removeEditor(orphan);
    delete orphan;
    QCOMPARE(m_area->count(), 1);

    // removeAllEditorsForDoc removes the tabs of a document from every group
    m_area->ensureSubView();
    m_area->addEditorToGroup(1, docA, createEditorForDoc(docA));
    QCOMPARE(m_area->count(), 2);
    m_area->removeAllEditorsForDoc(docA);
    QCOMPARE(m_area->count(), 0);
    QCOMPARE(m_area->tabGroupCount(), 1);
}

QTEST_MAIN(EditorAreaTest)
#include "editorarea_test.moc"
