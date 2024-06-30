#include "editorviewhelper.h"

#include <QActionGroup>
#include <QApplication>
#include <QMenu>

#include <KActionMenu>

#include <Qsci/qsciscintilla.h>
#include <Qsci/qscilexer.h>
#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexerbatch.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexercmake.h>
#include <Qsci/qscilexercsharp.h>
#include <Qsci/qscilexercss.h>
#include <Qsci/qscilexerd.h>
#include <Qsci/qscilexerdiff.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qscilexerjava.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexerlua.h>
#include <Qsci/qscilexermakefile.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexerpascal.h>
#include <Qsci/qscilexerperl.h>
#include <Qsci/qscilexerpo.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerruby.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qscilexeryaml.h>

const QStringList languages_group[] = {
    QStringList{
        "Bash", "Batch"
    },
    QStringList{
        "C", "C++", "C#", "CMake", "CSS"
    },
    QStringList{
        "D", "Diff"
    },
    QStringList{
        "HTML"
    },
    QStringList{
        "Java", "JavaScript", "JSON"
    },
    QStringList{
        "Lua"
    },
    QStringList{
        "Makefile", "Markdown"
    },
    QStringList{
        "Pascal", "Perl", "PO", "Python"
    },
    QStringList{
        "Ruby"
    },
    QStringList{
        "XML"
    },
    QStringList{
        "YAML"
    },
};

QString LexerGroupActionMenu::groupName(LanguageGroup grp)
{
    switch (grp) {
    case LANG_GRP_B:
        return QLatin1String("B");
    case LANG_GRP_C:
        return QLatin1String("C");
    case LANG_GRP_D:
        return QLatin1String("D");
    case LANG_GRP_H:
        return QLatin1String("H");
    case LANG_GRP_J:
        return QLatin1String("J");
    case LANG_GRP_L:
        return QLatin1String("L");
    case LANG_GRP_M:
        return QLatin1String("M");
    case LANG_GRP_P:
        return QLatin1String("P");
    case LANG_GRP_R:
        return QLatin1String("R");
    case LANG_GRP_X:
        return QLatin1String("X");
    case LANG_GRP_Y:
        return QLatin1String("Y");
    }

    Q_UNREACHABLE_RETURN("");
}

QsciLexer * LexerGroupActionMenu::createLexerByLanguage(const QString &languageName)
{
    QsciLexer * ret = nullptr;
    if (languageName == "Bash") {
        ret = new QsciLexerBash();
    } else if (languageName == "Batch") {
        ret = new QsciLexerBatch();
    } else if (languageName == "C" || languageName == "C++") {
        ret = new QsciLexerCPP();
    } else if (languageName == "C#") {
        ret = new QsciLexerCSharp();
    } else if (languageName == "CMake") {
        ret = new QsciLexerCMake();
    } else if (languageName == "CSS") {
        ret = new QsciLexerCSS();
    } else if (languageName == "D") {
        ret = new QsciLexerD();
    } else if (languageName == "Diff") {
        ret = new QsciLexerDiff();
    } else if (languageName == "HTML") {
        ret = new QsciLexerHTML();
    } else if (languageName == "Java") {
        ret = new QsciLexerJava();
    } else if (languageName == "JavaScript") {
        ret = new QsciLexerJavaScript();
    } else if (languageName == "JSON") {
        ret = new QsciLexerJSON();
    } else if (languageName == "Lua") {
        ret = new QsciLexerLua();
    } else if (languageName == "Makefile") {
        ret = new QsciLexerMakefile();
    } else if (languageName == "Markdown") {
        ret = new QsciLexerMarkdown();
    } else if (languageName == "Pascal") {
        ret = new QsciLexerPascal();
    } else if (languageName == "Perl") {
        ret = new QsciLexerPerl();
    } else if (languageName == "PO") {
        ret = new QsciLexerPO();
    } else if (languageName == "Python") {
        ret = new QsciLexerPython();
    } else if (languageName == "Ruby") {
        ret = new QsciLexerRuby();
    } else if (languageName == "XML") {
        ret = new QsciLexerXML();
    } else if (languageName == "YAML") {
        ret = new QsciLexerYAML();
    }
    return ret;
}

void LexerGroupActionMenu::slotAboutToShow()
{
    if (!m_group) {
        m_group = new QActionGroup(menu());
    } else {
        return;
    }

    for (const QString & name : languages_group[m_languageGroup]) {
        QAction *a = menu()->addAction(name, this, [this, name](){
            emit lexerSelected(name);
        });
        a->setActionGroup(m_group);
    }
}
