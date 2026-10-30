#include "favoritesmanagedialog.h"
#include "favoritesstore.h"
#include "ui_favoritesmanagedialog.h"

#include <QFileInfo>
#include <QListWidgetItem>

using namespace Qt::Literals::StringLiterals;

FavoritesManageDialog::FavoritesManageDialog(FavoritesStore *store, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , ui(new Ui::FavoritesManageDialog)
{
    ui->setupUi(this);

    connect(ui->openButton, &QPushButton::clicked, this, &FavoritesManageDialog::openSelected);
    connect(ui->deleteButton, &QPushButton::clicked, this, &FavoritesManageDialog::deleteSelected);
    connect(ui->favoriteListWidget, &QListWidget::itemSelectionChanged,
            this, &FavoritesManageDialog::updateButtons);
    connect(m_store, &FavoritesStore::entriesChanged, this, &FavoritesManageDialog::populateList);

    populateList();
}

FavoritesManageDialog::~FavoritesManageDialog()
{
    delete ui;
}

void FavoritesManageDialog::populateList()
{
    ui->favoriteListWidget->clear();

    const QColor missingTextColor = ui->favoriteListWidget->palette().color(QPalette::Disabled, QPalette::Text);
    const QStringList entries = m_store->entries();
    for (const QString &path : entries) {
        const QFileInfo fi(path);
        auto *item = new QListWidgetItem(fi.fileName());
        item->setToolTip(path);
        item->setData(Qt::UserRole, path);
        if (!fi.exists()) {
            item->setText(fi.fileName() + u" (%1)"_s.arg(tr("missing")));
            item->setForeground(missingTextColor);
        }
        ui->favoriteListWidget->addItem(item);
    }

    updateButtons();
}

void FavoritesManageDialog::openSelected()
{
    QStringList paths;
    const QList<QListWidgetItem *> items = ui->favoriteListWidget->selectedItems();
    for (const QListWidgetItem *item : items) {
        paths.append(item->data(Qt::UserRole).toString());
    }
    if (paths.isEmpty()) {
        return;
    }

    emit openRequested(paths);
    accept();
}

void FavoritesManageDialog::deleteSelected()
{
    QStringList paths;
    const QList<QListWidgetItem *> items = ui->favoriteListWidget->selectedItems();
    for (const QListWidgetItem *item : items) {
        paths.append(item->data(Qt::UserRole).toString());
    }
    if (paths.isEmpty()) {
        return;
    }

    m_store->removeEntries(paths);
}

void FavoritesManageDialog::updateButtons()
{
    const bool hasSelection = !ui->favoriteListWidget->selectedItems().isEmpty();
    ui->openButton->setEnabled(hasSelection);
    ui->deleteButton->setEnabled(hasSelection);
}
