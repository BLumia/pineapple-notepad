#include "editorviewhelper.h"
#include "languageconfigstore.h"

#include <QActionGroup>
#include <QMenu>
#include <QStringConverter>

#include <KActionMenu>
#include <KCharsets>

using namespace Qt::StringLiterals;

void LexerGroupActionMenu::setLanguageIds(const QStringList &languageIds)
{
    m_languageIds = languageIds;
}

void LexerGroupActionMenu::slotAboutToShow()
{
    if (m_populated) {
        return;
    }
    m_populated = true;

    if (!m_group) {
        m_group = new QActionGroup(menu());
    }

    for (const QString &id : std::as_const(m_languageIds)) {
        QAction *a = menu()->addAction(LanguageConfigStore::displayName(id), this, [this, id](){
            emit lexerSelected(id);
        });
        a->setActionGroup(m_group);
    }
}

static bool isCodecSupportingBom(const QString &codecName)
{
    static const QStringList bomCodecs = {
        QStringLiteral("UTF-8"),
        QStringLiteral("UTF-16"),
        QStringLiteral("UTF-16BE"),
        QStringLiteral("UTF-16LE"),
        QStringLiteral("UTF-32"),
        QStringLiteral("UTF-32BE"),
        QStringLiteral("UTF-32LE")
    };
    return bomCodecs.contains(codecName, Qt::CaseInsensitive);
}

EncodingGroupActionMenu::EncodingGroupActionMenu(const QString &text, const QStringList &encodingNames, QObject *parent)
    : KActionMenu(text, parent)
    , m_actionGroup(nullptr)
    , m_encodingNames(encodingNames)
    , m_menuPopulated(false)
{
    connect(menu(), &QMenu::aboutToShow, this, &EncodingGroupActionMenu::slotAboutToShow);
    setPopupMode(QToolButton::InstantPopup);
}

QList<QStringList> EncodingGroupActionMenu::encodingGroups()
{
    return KCharsets::charsets()->encodingsByScript();
}

void EncodingGroupActionMenu::slotAboutToShow()
{
    if (m_menuPopulated) {
        return;
    }
    m_menuPopulated = true;

    if (!m_actionGroup) {
        m_actionGroup = new QActionGroup(menu());
    }

    KCharsets *charsets = KCharsets::charsets();

    for (const QString &encodingName : m_encodingNames) {
        const QString codecName = charsets->encodingForName(encodingName);
        const QString description = charsets->descriptionForEncoding(codecName);
        const QString displayText = description.isEmpty() ? encodingName : description;

        if (isCodecSupportingBom(codecName)) {
            QAction *a = menu()->addAction(displayText, this, [this, codecName, displayText](){
                emit encodingSelected(codecName, false, displayText);
            });
            a->setActionGroup(m_actionGroup);

            const QString bomText = displayText + u" (BOM)"_s;
            QAction *aBom = menu()->addAction(bomText, this, [this, codecName, bomText](){
                emit encodingSelected(codecName, true, bomText);
            });
            aBom->setActionGroup(m_actionGroup);
        } else {
            QAction *a = menu()->addAction(displayText, this, [this, codecName, displayText](){
                emit encodingSelected(codecName, false, displayText);
            });
            a->setActionGroup(m_actionGroup);
        }
    }
}
