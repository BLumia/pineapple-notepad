#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

#include "document.h"

class DocumentManager : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        DocIdRole = Qt::UserRole + 1,
        DocPtrRole,
        RecentTagRole,
    };
    Q_ENUM(Roles)

    explicit DocumentManager(QObject *parent = nullptr);
    ~DocumentManager();

    // Path utilities
    static QString normalizeFilePath(const QString &path);

    // Lexer utilities
    static void applyLexerToEditor(SciEdit *editor, const QString &language, const DocumentManager *docManager, bool isLargeFile = false, bool colouriseNow = true);

    // Document lookup
    int findDocumentByFilePath(const QString &filePath) const;
    // Finds the untitled document showing the given title (e.g. "Untitled-3");
    // used to resolve references coming from the search results panel, where
    // untitled documents are listed by title.
    int findUntitledDocumentByTitle(const QString &title) const;

    // Document management
    int createNewDocument();
    int openDocument(const QString &filePath);
    bool saveDocument(int docId);
    bool saveDocumentAs(int docId, const QString &filePath);
    void closeDocument(int docId);

    // Document properties
    QString getDocumentTitle(int docId) const;
    QString getDocumentFilePath(int docId) const;
    QString getDocumentEncoding(int docId) const;
    QString getDocumentLanguage(int docId) const;
    QString resolveLexerForLanguage(const QString &language) const;
    int getDocumentEolMode(int docId) const;
    bool isDocumentEolMixed(int docId) const;
    int getDocumentIndentTabWidth(int docId) const;
    bool getDocumentIndentUseTabs(int docId) const;
    bool getDocumentSpellCheckEnabled(int docId) const;
    bool isDocumentModified(int docId) const;
    bool isDocumentUntitled(int docId) const;
    bool isDocumentMonitored(int docId) const;
    bool isDocumentReadOnly(int docId) const;

    Document *getDocument(int docId);
    const Document *getDocument(int docId) const;

    // Document modification
    void setDocumentModified(int docId, bool modified);
    void setDocumentEncodingModified(int docId, bool modified);
    void setDocumentEncodingDetails(int docId, const QString &displayName, const QByteArray &codecName, bool writeBom);
    void setDocumentLanguage(int docId, const QString &language);
    void setDocumentEolMode(int docId, int eolMode, bool mixed = false);
    void setDocumentIndentTabWidth(int docId, int width);
    void setDocumentIndentUseTabs(int docId, bool useTabs);
    // Toggles spell checking for the document and applies the new state to
    // every attached editor.
    void setDocumentSpellCheckEnabled(int docId, bool enabled);
    void setDocumentFilePath(int docId, const QString &filePath);
    void setDocumentReadOnly(int docId, bool readOnly);
    void touchDocument(int docId);

    // Get all document information
    QList<int> getAllDocumentIds() const;
    int getDocumentCount() const;

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Model helpers
    int rowForDocId(int docId) const;
    QModelIndex indexForDocId(int docId) const;
    int docIdAt(int row) const;
    void setCurrentDocId(int docId);
    void moveDocument(int from, int to);

signals:
    void documentCreated(int docId);
    void documentOpened(int docId, const QString &filePath);
    void documentSaved(int docId, const QString &filePath);
    void documentClosed(int docId);
    void documentModified(int docId, bool modified);
    void documentTitleChanged(int docId, const QString &title);
    void documentReadOnlyChanged(int docId, bool readOnly);

private:
    QHash<int, Document> m_documents;
    QList<int> m_docIdOrder;
    int m_nextDocumentId;
    int m_currentDocId = -1;
    static int s_untitledCounter;
    static quint64 s_recentTagCounter;

    QString generateUntitledName();
    QString detectLanguageFromExtension(const QString &filePath);
    QIcon iconForDoc(const Document &doc) const;
    void emitDataChangedForRoles(int docId, const QVector<int> &roles);
};
