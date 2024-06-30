#pragma once

#include <KActionMenu>
#include <QMenu>

class QsciLexer;
class LexerGroupActionMenu : public KActionMenu
{
    Q_OBJECT
public:
    enum LanguageGroup {
        LANG_GRP_B,
        LANG_GRP_C,
        LANG_GRP_D,
        LANG_GRP_H,
        LANG_GRP_J,
        LANG_GRP_L,
        LANG_GRP_M,
        LANG_GRP_P,
        LANG_GRP_R,
        LANG_GRP_X,
        LANG_GRP_Y,
        LANG_GRP_LAST = LANG_GRP_Y
    };
    LexerGroupActionMenu(const QString &text, enum LanguageGroup grp, QObject *parent)
        : KActionMenu(text, parent)
    {
        m_group = nullptr;
        m_languageGroup = grp;
        connect(menu(), &QMenu::aboutToShow, this, &LexerGroupActionMenu::slotAboutToShow);
        setPopupMode(QToolButton::InstantPopup);
    }

    static QString groupName(enum LanguageGroup grp);
    static QsciLexer * createLexerByLanguage(const QString & languageName);

signals:
    void lexerSelected(const QString lexerName);

private:
    QActionGroup *m_group;
    enum LanguageGroup m_languageGroup;

public:
    void slotAboutToShow();
};
