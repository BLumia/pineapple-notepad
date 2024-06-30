#include <QApplication>

#include "mainwindow.h"

#include <KAboutData>
#include <KLocalizedString>
#include <KIconTheme>
#include <KStyleManager>
#include <QSci/qsciglobal.h>

int main(int argc, char *argv[])
{
    KIconTheme::initTheme();

    QApplication a(argc, argv);

    KStyleManager::initStyle();

    KLocalizedString::setApplicationDomain(QByteArrayLiteral("pineapple-notepad"));

    KAboutData aboutData(QStringLiteral("pineapple-notepad"),
                         "Pineapple Notepad（迫真）",
                         QStringLiteral("0.1"),
                         "A Scintilla-based Text Editor",
                         KAboutLicense::Unknown,
                         QString(),
                         QString(),
                         QStringLiteral("https://blumia.net"));
    aboutData.addComponent("QsciScintilla", "A port to Qt6 of Neil Hodgson's Scintilla C++ editor class",
                            QSCINTILLA_VERSION_STR, "https://www.riverbankcomputing.com/software/qscintilla/intro");
    KAboutData::setApplicationData(aboutData);
    a.setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));

    MainWindow mw;
    mw.show();

    return a.exec();
}
