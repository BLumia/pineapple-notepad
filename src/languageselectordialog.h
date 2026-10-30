#pragma once

#include <QDialog>
#include <QSortFilterProxyModel>

class QStandardItemModel;
class QPushButton;

namespace Ui {
class LanguageSelectorDialog;
}

enum class LanguageEntrySource {
    Catalog = 0, // built-in json or user-languages.json (Lexilla engine)
    Ksh = 1,     // KSyntaxHighlighting definition
};

class LanguageSelectorFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit LanguageSelectorFilterProxyModel(QObject *parent = nullptr);

    void setFilterText(const QString &text);
    void setCustomOnly(bool customOnly);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_filterText;
    bool m_customOnly = false;
};

class LanguageSelectorDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit LanguageSelectorDialog(bool defaultCustomOnly, QWidget *parent = nullptr);
    ~LanguageSelectorDialog() override;

    QString selectedLanguageId() const;

signals:
    // Emitted whenever the "Language" menu entry list changed (pin toggled or
    // a user defined language was added/edited/deleted).
    void languageMenuChanged();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void loadEntries();
    void onSelectionChanged();
    void updatePinButton();
    void togglePinForSelection();
    bool isEntryPinned(const QString &encodedEntry) const;
    void setEntryPinned(const QString &encodedEntry, bool pinned);
    void accept() override;

    Ui::LanguageSelectorDialog *m_ui = nullptr;

    QStandardItemModel *m_model = nullptr;
    LanguageSelectorFilterProxyModel *m_proxyModel = nullptr;
    QPushButton *m_pinButton = nullptr;

    QString m_selectedEntry; // encoded menu entry of the current selection
    bool m_defaultCustomOnly = false;
};
