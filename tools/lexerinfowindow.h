// SPDX-License-Identifier: MIT
// Lexer Info Tool - GUI version main window.

#ifndef LEXERINFOWINDOW_H
#define LEXERINFOWINDOW_H

#include <QMainWindow>

class QComboBox;
class QStandardItemModel;
class QTreeView;
class QTabWidget;
class QLabel;

class LexerInfoWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit LexerInfoWindow(QWidget* parent = nullptr);

private slots:
    void onLexerSelected(int index);

private:
    void setupUi();
    void populateLexerList();
    void displayLexerMetadata(const std::string& lexerName);

    QComboBox* m_lexerCombo;
    QTabWidget* m_tabWidget;
    QTreeView* m_propView;
    QTreeView* m_kwView;
    QTreeView* m_styleView;
    QStandardItemModel* m_propModel;
    QStandardItemModel* m_kwModel;
    QStandardItemModel* m_styleModel;
    QLabel* m_statusLabel;
};

#endif // LEXERINFOWINDOW_H
