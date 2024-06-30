#pragma once

#include <KXmlGuiWindow>

#include "ui_GeneralSettings.h"

class QLabel;
class QsciScintilla;
class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupActions();
    void showSettings();
    void applyLexer(const QString & lexer);

    QsciScintilla * m_editor;
    QLabel * m_cursorPosStatusLabel;
};

// plainly for KConfigDialog
// void setupUi(QWidget *GeneralSettings)
template <typename T>
class SettingsPage : public QWidget
{
public:
    SettingsPage(QWidget *parent)
        : QWidget(parent)
    {
        ui.setupUi(this);
    }

private:
    T ui;
};
