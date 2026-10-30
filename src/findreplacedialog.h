#pragma once

#include <QComboBox>
#include <QDialog>

namespace Ui {
class FindReplaceDialog;
}

enum class SearchMode {
    Normal,
    Extended,
    Regex,
};

namespace SearchUtil {
// Decodes the \n, \t, ... escape sequences of the Extended search mode.
QString decodeExtended(const QString &s);
}

class FindReplaceDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit FindReplaceDialog(QWidget *parent = nullptr);
    ~FindReplaceDialog() override;

    void openFind();
    void openReplace();
    void openFindInFiles();
    void openMark();

    QString findText() const;
    QString replaceText() const;
    QString findTextForSearch() const;
    QString replaceTextForReplace() const;

    bool matchCase() const;
    bool wholeWord() const;
    bool wrapAround() const;
    bool inSelection() const;
    SearchMode searchMode() const;

    int markStyleIndex() const;
    bool bookmarkLine() const;
    bool purgeMarks() const;
    bool backwardDirection() const;

    QString filters() const;
    QString directory() const;
    bool isRecursive() const;
    bool isInHiddenFolders() const;

    void setFindTextIfNotEmpty(const QString &text);
    void setCurrentDocumentDirectory(const QString &dir);
    void commitHistory();

    void showStatusMessage(const QString &message);
    void clearStatusMessage();

signals:
    void findRequested(bool forward);
    void countRequested();
    void replaceRequested();
    void replaceAllRequested();
    void findAllInCurrentDocumentRequested();
    void findAllInAllDocumentsRequested();
    void replaceAllInAllDocumentsRequested();
    void findInFilesRequested();
    void markAllRequested();
    void clearMarksRequested();
    void copyMarkedTextRequested();
    void dialogClosed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void initCombos();
    void initModeRadios();
    void initSync();
    void syncFromFindTab();
    void syncFromReplaceTab();
    void syncFromFifTab();
    void syncFromMarkTab();
    void openTab(QWidget *tab, QComboBox *focusCombo);

    Ui::FindReplaceDialog *ui = nullptr;
    bool m_syncing = false;
    QString m_currentDocDirectory;
};
