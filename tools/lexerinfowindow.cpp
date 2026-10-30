// SPDX-License-Identifier: MIT
// Lexer Info Tool - GUI version main window.

#include "lexerinfowindow.h"
#include "lexer_query.h"

#include <QComboBox>
#include <QCompleter>
#include <QHeaderView>
#include <QLabel>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTreeView>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

LexerInfoWindow::LexerInfoWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    populateLexerList();
}

void LexerInfoWindow::setupUi() {
    setWindowTitle(QStringLiteral("Lexilla Lexer Info"));
    resize(900, 640);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);

    // Lexer selector
    m_lexerCombo = new QComboBox(central);
    m_lexerCombo->setEditable(true);
    m_lexerCombo->setInsertPolicy(QComboBox::NoInsert);
    auto* completer = new QCompleter(m_lexerCombo);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_lexerCombo->setCompleter(completer);
    layout->addWidget(m_lexerCombo);

    // Tab widget with 3 tree views
    m_tabWidget = new QTabWidget(central);

    // Styles tab
    m_styleModel = new QStandardItemModel(this);
    m_styleModel->setHorizontalHeaderLabels({tr("ID"), tr("Name"), tr("Tags"), tr("Description")});
    m_styleView = new QTreeView();
    m_styleView->setModel(m_styleModel);
    m_styleView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_styleView->setAlternatingRowColors(true);
    m_styleView->setRootIsDecorated(false);
    m_styleView->setItemsExpandable(false);
    m_styleView->header()->setStretchLastSection(true);
    m_tabWidget->addTab(m_styleView, tr("Styles"));

    // Properties tab
    m_propModel = new QStandardItemModel(this);
    m_propModel->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Description"), tr("Default")});
    m_propView = new QTreeView();
    m_propView->setModel(m_propModel);
    m_propView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_propView->setAlternatingRowColors(true);
    m_propView->setRootIsDecorated(false);
    m_propView->setItemsExpandable(false);
    m_propView->header()->setStretchLastSection(true);
    m_tabWidget->addTab(m_propView, tr("Properties"));

    // Keywords tab
    m_kwModel = new QStandardItemModel(this);
    m_kwModel->setHorizontalHeaderLabels({tr("Index"), tr("Description")});
    m_kwView = new QTreeView();
    m_kwView->setModel(m_kwModel);
    m_kwView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_kwView->setAlternatingRowColors(true);
    m_kwView->setRootIsDecorated(false);
    m_kwView->setItemsExpandable(false);
    m_kwView->header()->setStretchLastSection(true);
    m_tabWidget->addTab(m_kwView, tr("Keywords"));

    layout->addWidget(m_tabWidget);

    // Status label
    m_statusLabel = new QLabel(central);
    layout->addWidget(m_statusLabel);

    setCentralWidget(central);

    connect(m_lexerCombo, &QComboBox::currentIndexChanged, this, &LexerInfoWindow::onLexerSelected);
}

void LexerInfoWindow::populateLexerList() {
    m_lexerCombo->blockSignals(true);
    auto names = LexerQuery::enumerateLexerNames();
    for (const auto& name : names) {
        m_lexerCombo->addItem(QString::fromStdString(name));
    }
    m_lexerCombo->blockSignals(false);

    if (!names.empty()) {
        m_lexerCombo->setCurrentIndex(0);
        onLexerSelected(0);
    }
}

void LexerInfoWindow::onLexerSelected(int index) {
    if (index < 0)
        return;
    std::string name = m_lexerCombo->itemText(index).toStdString();
    displayLexerMetadata(name);
}

static QString truncated(const std::string& s, int maxlen = 512) {
    if (static_cast<int>(s.size()) <= maxlen)
        return QString::fromStdString(s);
    return QString::fromStdString(s).left(maxlen) + QStringLiteral("...");
}

void LexerInfoWindow::displayLexerMetadata(const std::string& lexerName) {
    auto meta = LexerQuery::queryLexerMetadata(lexerName);

    // Properties
    m_propModel->removeRows(0, m_propModel->rowCount());
    for (const auto& p : meta.properties) {
        auto* nameItem = new QStandardItem(QString::fromStdString(p.name));
        nameItem->setEditable(false);
        auto* typeItem = new QStandardItem(QString::fromStdString(p.typeName));
        typeItem->setEditable(false);
        auto* descItem = new QStandardItem(truncated(p.description));
        descItem->setEditable(false);
        descItem->setToolTip(QString::fromStdString(p.description));
        auto* defItem = new QStandardItem(QString::fromStdString(p.defaultValue));
        defItem->setEditable(false);
        m_propModel->appendRow({nameItem, typeItem, descItem, defItem});
    }
    for (int c = 0; c < m_propModel->columnCount() - 1; ++c)
        m_propView->resizeColumnToContents(c);

    // Keywords
    m_kwModel->removeRows(0, m_kwModel->rowCount());
    for (const auto& kw : meta.keywordLists) {
        auto* idxItem = new QStandardItem(QString::number(kw.index));
        idxItem->setEditable(false);
        auto* descItem = new QStandardItem(QString::fromStdString(kw.description));
        descItem->setEditable(false);
        m_kwModel->appendRow({idxItem, descItem});
    }
    for (int c = 0; c < m_kwModel->columnCount() - 1; ++c)
        m_kwView->resizeColumnToContents(c);

    // Styles
    m_styleModel->removeRows(0, m_styleModel->rowCount());
    for (const auto& s : meta.styles) {
        auto* idItem = new QStandardItem(QString::number(s.id));
        idItem->setEditable(false);
        auto* nameItem = new QStandardItem(s.name.empty() ? tr("(unnamed)") : QString::fromStdString(s.name));
        nameItem->setEditable(false);
        auto* tagsItem = new QStandardItem(QString::fromStdString(s.tags));
        tagsItem->setEditable(false);
        auto* descItem = new QStandardItem(truncated(s.description));
        descItem->setEditable(false);
        descItem->setToolTip(QString::fromStdString(s.description));
        m_styleModel->appendRow({idItem, nameItem, tagsItem, descItem});
    }
    for (int c = 0; c < m_styleModel->columnCount() - 1; ++c)
        m_styleView->resizeColumnToContents(c);

    // Status
    m_statusLabel->setText(tr("Lexer: %1 | Type: %2 | Identifier: %3 | Properties: %4 | Keywords: %5 | Styles: %6/%7")
        .arg(QString::fromStdString(meta.name))
        .arg(meta.usesFallbackStyles ? tr("old-style") : tr("new-style"))
        .arg(meta.identifier)
        .arg(meta.properties.size())
        .arg(meta.keywordLists.size())
        .arg(meta.styles.size())
        .arg(meta.totalStyleSlots));
}
