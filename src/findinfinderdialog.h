#pragma once

#include "findreplacedialog.h"

#include <QDialog>

namespace Ui {
class FindInFinderDialog;
}

// Small prompt dialog for "Find in these search results":
// collects the new search term and options used to re-run a
// search on the files listed in an existing search results panel.
class FindInFinderDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit FindInFinderDialog(QWidget *parent = nullptr);
    ~FindInFinderDialog() override;

    QString findText() const;
    // Extended-mode escape sequences decoded.
    QString findTextForSearch() const;
    bool matchCase() const;
    bool wholeWord() const;
    SearchMode searchMode() const;
    // Only keep matches on lines that already appear in the source results
    // (for "Search only in found lines" option).
    bool matchLineNumber() const;

private:
    Ui::FindInFinderDialog *ui = nullptr;
};
