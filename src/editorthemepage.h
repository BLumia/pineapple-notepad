#pragma once

#include "ui_editorthemesettings.h"

#include <QColor>
#include <QList>
#include <QString>
#include <QWidget>

class SciEdit;
class KColorButton;
class KMessageWidget;

class EditorThemePage : public QWidget
{
    Q_OBJECT

public:
    explicit EditorThemePage(QWidget *parent = nullptr);

    QString selectedThemeId() const;
    bool hasThemeChanged() const;
    void syncToSaved();

Q_SIGNALS:
    void themeSelectionChanged();
    void themeColorsChanged();

private:
    void setupPreview();
    void setupColorEditor();
    void setupSponsorMessage();
    void populateColorEditor(const QString &themeId);
    void onSaveClicked();
    void onResetClicked();
    void updateSponsorRelatedUi();

    Ui::EditorThemeSettings *ui;
    SciEdit *m_previewEdit = nullptr;
    QString m_savedThemeId;
    KMessageWidget *m_sponsorMessage = nullptr;

    struct ColorRow {
        QString role;
        QColor originalColor;
        QColor currentColor;
        KColorButton *button = nullptr;
    };
    QList<ColorRow> m_colorRows;
};
