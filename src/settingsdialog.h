#pragma once

#include <KConfigDialog>

class EditorThemePage;

class SettingsDialog : public KConfigDialog
{
    Q_OBJECT

public:
    SettingsDialog(QWidget *parent, const QString &name, KCoreConfigSkeleton *config);

    void setEditorThemePage(EditorThemePage *page);

protected:
    QSize sizeHint() const override;
    bool hasChanged() override;
    void updateSettings() override;
    void updateWidgets() override;
    void closeEvent(QCloseEvent *event) override;

private:
    bool hasUnsavedChanges() const;
    EditorThemePage *m_editorThemePage = nullptr;
};
