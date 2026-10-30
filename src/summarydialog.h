#pragma once

#include <QDialog>

namespace Ui {
class SummaryDialog;
}

struct SummaryData {
    QString filePath;
    QString created;
    QString modified;
    QString characters;
    QString words;
    QString lines;
    QString documentLength;
    QString selectedCharacters;
    QString selectedBytes;
    QString selectionRanges;
    bool hasFileInfo = false;
};

class SummaryDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SummaryDialog(QWidget *parent = nullptr);
    ~SummaryDialog() override;

    void setSummaryData(const SummaryData &data);

private:
    Ui::SummaryDialog *ui = nullptr;
};
