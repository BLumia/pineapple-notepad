#pragma once

#include <QDialog>

class KActionCollection;
class KMessageWidget;
class KXMLGUIClient;
class KXMLGUIFactory;

namespace Ui {
class EditorPopupConfigDialog;
}

class EditorPopupConfigDialog final : public QDialog
{
    Q_OBJECT
public:
    EditorPopupConfigDialog(KXMLGUIFactory *factory, KXMLGUIClient *client, QWidget *parent = nullptr);
    ~EditorPopupConfigDialog() override;

    void accept() override;

private:
    void reload();
    void setupSponsorMessage();
    void updateSponsorRelatedUi();
    QStringList collectSelected() const;
    QStringList currentItems() const;
    static QStringList defaultItemsFromContainer(KXMLGUIFactory *factory, KXMLGUIClient *client);

    Ui::EditorPopupConfigDialog *ui = nullptr;
    KActionCollection *m_collection = nullptr;
    KXMLGUIFactory *m_factory = nullptr;
    KXMLGUIClient *m_client = nullptr;
    KMessageWidget *m_sponsorMessage = nullptr;
};
