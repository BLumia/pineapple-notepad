#include "editorpopupconfigdialog.h"
#include "ui_editorpopupconfigdialog.h"

#include "appsettings.h"
#include "editorpopupconfig.h"
#include "licenseverifier.h"
#include "sponsordialog.h"

#include <KActionCollection>
#include <KActionSelector>
#include <KLocalizedString>
#include <KMessageWidget>
#include <KXMLGUIClient>
#include <KXMLGUIFactory>

#include <QAction>
#include <QDialogButtonBox>
#include <QIcon>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QStringList>

using namespace Qt::StringLiterals;

static QListWidgetItem *makeSeparatorItem(QListWidget *parent)
{
    auto *item = new QListWidgetItem(parent);
    item->setText(ki18nd("kxmlgui6", "--- separator ---").toString());
    item->setData(Qt::UserRole, QString());
    return item;
}

static QListWidgetItem *makeActionItem(QListWidget *parent, QAction *action)
{
    auto *item = new QListWidgetItem(parent);
    item->setText(KLocalizedString::removeAcceleratorMarker(action->iconText()));
    item->setIcon(action->icon());
    item->setData(Qt::UserRole, action->objectName());
    return item;
}

EditorPopupConfigDialog::EditorPopupConfigDialog(KXMLGUIFactory *factory, KXMLGUIClient *client, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EditorPopupConfigDialog)
    , m_collection(client ? client->actionCollection() : nullptr)
    , m_factory(factory)
    , m_client(client)
{
    ui->setupUi(this);

    setupSponsorMessage();

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui->resetButton, &QPushButton::clicked, this, [this]() {
        EditorPopupConfig::clearCustomization();
        reload();
    });

    reload();
}

EditorPopupConfigDialog::~EditorPopupConfigDialog()
{
    delete ui;
}

QStringList EditorPopupConfigDialog::currentItems() const
{
    if (EditorPopupConfig::isCustomized()) {
        return EditorPopupConfig::items();
    }
    return defaultItemsFromContainer(m_factory, m_client);
}

void EditorPopupConfigDialog::reload()
{
    const QStringList selectedNames = currentItems();
    const QSet<QString> selectedSet(selectedNames.begin(), selectedNames.end());

    QListWidget *selected = ui->selector->selectedListWidget();
    selected->clear();
    for (const QString &name : selectedNames) {
        if (name.isEmpty()) {
            makeSeparatorItem(selected);
        } else if (m_collection) {
            if (QAction *a = m_collection->action(name)) {
                makeActionItem(selected, a);
            } else {
                auto *item = new QListWidgetItem(selected);
                item->setText(name);
                item->setData(Qt::UserRole, name);
            }
        }
    }

    QListWidget *available = ui->selector->availableListWidget();
    available->clear();
    if (m_collection) {
        const auto actions = m_collection->actions();
        for (QAction *a : actions) {
            const QString name = a->objectName();
            if (name.isEmpty() || selectedSet.contains(name)) {
                continue;
            }
            makeActionItem(available, a);
        }
    }
    makeSeparatorItem(available);
    available->sortItems();

    ui->selector->setButtonsEnabled();
}

QStringList EditorPopupConfigDialog::collectSelected() const
{
    QStringList items;
    QListWidget *selected = ui->selector->selectedListWidget();
    for (int i = 0; i < selected->count(); ++i) {
        items.append(selected->item(i)->data(Qt::UserRole).toString());
    }
    return items;
}

QStringList EditorPopupConfigDialog::defaultItemsFromContainer(KXMLGUIFactory *factory, KXMLGUIClient *client)
{
    QStringList items;
    if (!factory || !client) {
        return items;
    }
    auto *menu = qobject_cast<QMenu *>(factory->container(u"editor_popup"_s, client));
    if (!menu) {
        return items;
    }
    const auto actions = menu->actions();
    for (QAction *a : actions) {
        if (a->isSeparator()) {
            items.append(QString());
        } else {
            const QString name = a->objectName();
            if (!name.isEmpty()) {
                items.append(name);
            }
        }
    }
    return items;
}

void EditorPopupConfigDialog::setupSponsorMessage()
{
#ifdef SPONSOR_EDITION
    // Sponsor Edition: all features always unlocked, no gate UI needed.
    // m_sponsorMessage stays nullptr; OK button keeps its default (enabled).
#else
    m_sponsorMessage = new KMessageWidget(this);
    m_sponsorMessage->setMessageType(KMessageWidget::Information);
    m_sponsorMessage->setWordWrap(true);
    m_sponsorMessage->setCloseButtonVisible(false);
    m_sponsorMessage->setIcon(QIcon::fromTheme(QStringLiteral("love")));
    m_sponsorMessage->setText(tr("This feature is currently only available to sponsors. "
                                 "Please consider becoming a sponsor."));
    auto *iAmSponsorAction = new QAction(tr("I'm a Sponsor"), m_sponsorMessage);
    m_sponsorMessage->addAction(iAmSponsorAction);
    connect(iAmSponsorAction, &QAction::triggered, this, [this]() {
        SponsorDialog dialog(this);
        dialog.showLicenseTab();
        dialog.exec();
    });
    ui->verticalLayout->insertWidget(0, m_sponsorMessage);

    connect(AppSettings::self(), &AppSettings::licenseContentChanged,
            this, &EditorPopupConfigDialog::updateSponsorRelatedUi);
    updateSponsorRelatedUi();
#endif
}

void EditorPopupConfigDialog::updateSponsorRelatedUi()
{
#ifdef SPONSOR_EDITION
    // No-op in Sponsor Edition.
#else
    if (!m_sponsorMessage)
        return;

    const QString licText = AppSettings::licenseContent();
    const bool isValid = !licText.isEmpty() && LicenseVerifier::verifyContent(licText);
    m_sponsorMessage->setVisible(!isValid);
    if (QPushButton *ok = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        ok->setEnabled(isValid);
    }
#endif
}

void EditorPopupConfigDialog::accept()
{
    const QStringList collected = collectSelected();
    const QStringList defaultItems = defaultItemsFromContainer(m_factory, m_client);
    if (collected == defaultItems) {
        // Matches the ui.rc default: don't leave a customization trail, so the
        // popup keeps using the factory-built container (and its features).
        EditorPopupConfig::clearCustomization();
    } else {
        EditorPopupConfig::setItems(collected);
    }
    QDialog::accept();
}
