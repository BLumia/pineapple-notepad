#pragma once

#include <QVector>

class SciEdit;
class QWidget;

enum class NewTabPlacement {
    AppendToEnd,
    AfterCurrent,
};

class EditorViewManager
{
public:
    virtual ~EditorViewManager() = default;

    virtual SciEdit *currentEditor() const = 0;
    virtual int currentDocumentId() const = 0;
    virtual int docIdForEditor(SciEdit *editor) const = 0;
    virtual SciEdit *editorForDocId(int docId) const = 0;
    virtual QVector<SciEdit *> editorsForDocId(int docId) const = 0;
    virtual int count() const = 0;
    virtual QVector<SciEdit *> editors() const = 0;
    virtual QVector<int> documentIds() const = 0;
    virtual QWidget *viewWidget() const = 0;

    virtual void addEditor(int docId, SciEdit *editor, bool activate = true, NewTabPlacement placement = NewTabPlacement::AppendToEnd) = 0;
    virtual void removeEditor(SciEdit *editor) = 0;
    virtual void removeAllEditorsForDoc(int docId) = 0;
    virtual void switchToEditor(SciEdit *editor) = 0;
    void switchToDocId(int docId) { if (auto *e = editorForDocId(docId)) switchToEditor(e); }
    virtual QVector<SciEdit *> editorsInSameTabGroup(SciEdit *editor) const = 0;
    virtual void refreshEditor(SciEdit *editor) = 0;
    void refreshEditorsForDoc(int docId) { for (auto *e : editorsForDocId(docId)) refreshEditor(e); }

    // Tab navigation within the tab group of the current editor
    virtual void activateNextTab() = 0;
    virtual void activatePreviousTab() = 0;
    virtual void activateTabAtIndex(int index) = 0;

    // Per tab-group addressing. Implementations hosting a single tab group
    // (TabWidget) report exactly one group with index 0.
    virtual int tabGroupCount() const = 0;
    virtual QVector<int> documentIdsInGroup(int group) const = 0;
    virtual int activeIndexInGroup(int group) const = 0;
    virtual SciEdit *currentEditorInGroup(int group) const = 0;
    virtual SciEdit *editorInGroup(int group, int docId) const = 0;
    virtual int groupOfEditor(SciEdit *editor) const = 0;
};
