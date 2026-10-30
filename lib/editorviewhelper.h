#pragma once

#include <KActionMenu>
#include <QMenu>
#include <QStringList>

class LexerGroupActionMenu : public KActionMenu
{
    Q_OBJECT
public:
    LexerGroupActionMenu(const QString &text, QObject *parent)
        : KActionMenu(text, parent)
    {
        connect(menu(), &QMenu::aboutToShow, this, &LexerGroupActionMenu::slotAboutToShow);
        setPopupMode(QToolButton::InstantPopup);
    }

    // The list of language ids (catalog ids or KSH definition names) offered by
    // this submenu. Display names are resolved through LanguageConfigStore.
    void setLanguageIds(const QStringList &languageIds);

signals:
    void lexerSelected(const QString &languageName);

private:
    QActionGroup *m_group = nullptr;
    QStringList m_languageIds;
    bool m_populated = false;

    void slotAboutToShow();
};

class EncodingGroupActionMenu : public KActionMenu
{
    Q_OBJECT
public:
    EncodingGroupActionMenu(const QString &text, const QStringList &encodingNames, QObject *parent);
    static QList<QStringList> encodingGroups();

signals:
    void encodingSelected(const QString &codecName, bool withBom, const QString &displayName);

private:
    QActionGroup *m_actionGroup;
    QStringList m_encodingNames;
    bool m_menuPopulated;

public:
    void slotAboutToShow();
};
