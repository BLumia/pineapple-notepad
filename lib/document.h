#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QPointer>
#include <QString>
#include <QVector>

#include <Scintilla.h>

class SciEdit;

struct EditorRef
{
    QPointer<SciEdit> editor;
    sptr_t savedCursorPos = 0;
    sptr_t savedFirstVisibleLine = 0;
    sptr_t savedXOffset = 0;
};

class Document
{
public:
    int id = -1;
    QString title;
    QString filePath;
    QByteArray content;
    QString encoding = QStringLiteral("UTF-8");
    QByteArray codecName = QByteArrayLiteral("UTF-8");
    bool encodingWriteBom = false;
    QString language;
    int eolMode = 0;
    bool eolMixed = false;
    int indentTabWidth = 4;
    bool indentUseTabs = false;
    // Per-document spell check toggle (View menu); new documents start from
    // Sonnet's "checker enabled by default" setting (see DocumentManager).
    bool spellCheckEnabled = false;
    bool contentModified = false;
    bool encodingModified = false;
    // Despite its name, this flag does NOT mean "has no title" or "has no path".
    // It means the document's content has no backing file on disk and lives only
    // in memory, so it must be snapshotted to survive across sessions. This covers
    // both path-less scratch documents AND documents whose filePath points to a
    // file that does not exist on disk (e.g. a non-existent file passed on the
    // command line). It is cleared only once content is actually written to disk
    // (via saveAs()). Note: filePath.isEmpty() is a DISTINCT concept ("the user
    // never assigned a target path") and must not be used interchangeably with
    // this flag — a doc may have a filePath yet still be untitled, and vice versa.
    bool untitled = true;
    bool isLargeFile = false;
    bool isLoading = false;
    bool isSaving = false;
    bool isMonitoringOn = false;
    bool readOnly = false;
    quint64 recentTag = 0;
    QDateTime lastKnownDiskModifiedAt;

    bool loadFromDiskInto(SciEdit *editor);
    bool loadFromDiskInto(SciEdit *editor, const QByteArray &forcedCodecName, bool forcedWriteBom, const QString &forcedDisplayName);
    void attachEditor(SciEdit *editor);
    bool save();
    bool saveAs(const QString &newPath);
    bool reload(bool preserveViewState = true);

    void addEditorReference(SciEdit *editor);
    void removeEditorReference(SciEdit *editor);
    bool hasEditor(SciEdit *editor) const;
    bool hasAnyEditor() const;
    SciEdit *firstEditor() const;
    const QVector<EditorRef> &editorReferences() const;

private:
    QVector<EditorRef> m_editorRefs;
};
