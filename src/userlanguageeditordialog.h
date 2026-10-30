#pragma once

#include <QDialog>
#include <QHash>
#include <QMap>

class QTreeWidgetItem;

namespace Ui {
class UserLanguageEditorDialog;
}

// Editor for user defined languages (UDL): a language built on a base Lexilla
// lexer (one with style mappings known to the application), with user supplied
// keyword lists, lexer properties, comment markers and file associations.
// Persisted via user-languages.json.
class UserLanguageEditorDialog final : public QDialog
{
    Q_OBJECT

public:
    // Create mode.
    explicit UserLanguageEditorDialog(QWidget *parent = nullptr);
    // Edit mode for an existing user defined language id.
    UserLanguageEditorDialog(const QString &editLanguageId, QWidget *parent = nullptr);
    ~UserLanguageEditorDialog() override;

    QString languageId() const;

private:
    enum class EditableKind {
        None,
        KeywordSet, // key: keyword set index (as string)
        Property,   // key: property name
    };

    void setupUiExtra();
    void loadFromExistingEntry();
    void fitTreeColumns();
    void onBaseLexerChanged();
    void rebuildValueTree(const QString &lexerName);
    void onTreeSelectionChanged();
    void storeCurrentValue();
    void showValueForItem(QTreeWidgetItem *item);
    QTreeWidgetItem *findEditableItem(EditableKind kind, const QString &key) const;
    void validateAndAccept();
    void accept() override;

    Ui::UserLanguageEditorDialog *m_ui = nullptr;
    QString m_editLanguageId; // empty in create mode
    QHash<int, QString> m_keywordSets;
    QMap<QString, QString> m_properties;
    EditableKind m_currentKind = EditableKind::None;
    QString m_currentKey;
    bool m_swappingValues = false;
};
