#pragma once

#include <QDialog>
#include <QStringList>

class FavoritesStore;

namespace Ui {
class FavoritesManageDialog;
}

class FavoritesManageDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit FavoritesManageDialog(FavoritesStore *store, QWidget *parent = nullptr);
    ~FavoritesManageDialog() override;

Q_SIGNALS:
    void openRequested(const QStringList &paths);

private:
    void populateList();
    void openSelected();
    void deleteSelected();
    void updateButtons();

    FavoritesStore *m_store = nullptr;
    Ui::FavoritesManageDialog *ui = nullptr;
};
