#include <QApplication>

#include "appsettings.h"
#include "dependency_versions.h"
#include "documentmanager.h"
#include "mainwindow.h"
#include "singleapplicationmanager.h"
#include "sponsordialog.h"

#ifdef Q_OS_MACOS
#include "fileopeneventhandler.h"
#endif // Q_OS_MACOS

#include <KAboutData>
#include <KLocalizedString>
#include <KIconTheme>
#include <KSharedConfig>
#include <KStyleManager>
#include <KSignalHandler>
#include <KConfigGroup>
#include <QCommandLineParser>
#include <QStyleHints>
#include <QTranslator>
#include <QLoggingCategory>
#include <csignal>

Q_LOGGING_CATEGORY(lcMain, "pnotepad.main")

using namespace Qt::StringLiterals;

// Return a QLocale that uses `LANGUAGE` environment variable's first value for language
// `LANGUAGE` might be empty or have multiple values splitted by `:`. Fallback languages
// are not supported for now.
// This is becase KXmlGUI kswitchlanguagedialog_p.cpp initializeLanguages() will
// set LANGUAGE env and then try reset QLocale by invoking QSystemLocale dtor.
// On Linux, it seems like QLocale::system() will uses the updated value after that.
// Sadly this hack won't work on Windows and macOS (maybe because it won't read LANGUAGE
// environment variable at the first place?), thus we need to parse the env and feed them
// to QTranslator manually.
// FIXME: maybe should switch to gettext-based i18n so we won't rely on these hacks...
QLocale getLocale()
{
#ifndef Q_OS_LINUX
    const auto languages = qEnvironmentVariable("LANGUAGE").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (!languages.isEmpty()) {
        return QLocale(languages.first());
    }
#endif
    return QLocale::system();
}

int main(int argc, char *argv[])
{
    // Set application name BEFORE KIconTheme::initTheme to ensure KColorSchemeManager
    // uses the correct application name when it gets initialized
    QApplication::setApplicationName(u"pineapple-notepad"_s);
#ifdef SPONSOR_EDITION
    QApplication::setApplicationVersion(QStringLiteral(PINEAPPLE_NOTEPAD_VERSION_STR) % u"-sponsor-edition"_s);
#else
    QApplication::setApplicationVersion(QStringLiteral(PINEAPPLE_NOTEPAD_VERSION_STR));
#endif

    KIconTheme::initTheme();

    QApplication a(argc, argv);

    KLocalizedString::setApplicationDomain(QByteArrayLiteral("pineapple-notepad"));

    QTranslator translator;
    if (translator.load(getLocale(), "pineapple-notepad", "_", ":/i18n")) {
        a.installTranslator(&translator);
    }

    KStyleManager::initStyle();

    KSignalHandler::self()->watchSignal(SIGINT);
    KSignalHandler::self()->watchSignal(SIGTERM);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, &a, [&a](int s) {
        qCDebug(lcMain) << "KSignalHandler received signal" << s;
        if (s == SIGINT || s == SIGTERM) {
            a.quit();
        }
    });

    KAboutData aboutData(u"pineapple-notepad"_s,
                         QCoreApplication::translate("main", "Pineapple Notepad"),
                         a.applicationVersion(),
                         QCoreApplication::translate("main", "An advanced, easy to use, cross-platform notepad application."),
                         KAboutLicense::Unknown,
                         QStringLiteral("Copyright &copy; 2026 Gary 'BLumia' Wang"),
                         QString(),
                         QStringLiteral("https://link.blumia.net/pineapple-notepad"));
    aboutData.addAuthor("Gary Wang", "Author, current maintainer",
                        "pineapple@blumia.net", "https://www.blumia.net/");
    aboutData.setBugAddress("https://github.com/BLumia/pineapple-notepad");
    aboutData.addComponent("Scintilla", "A free source code editing component.",
                           PnDeps::kScintillaVersion, "https://scintilla.org/");
    aboutData.addComponent("Lexilla", "A library of lexers that can be used with Scintilla.",
                           PnDeps::kLexillaVersion, "https://www.scintilla.org/Lexilla.html");
    aboutData.addComponent("Monocypher", "Boring crypto that simply works.",
                           PnDeps::kMonocypherVersion, "https://monocypher.org/");
    aboutData.addCredit("BlackMaple", QCoreApplication::translate("main", "Sponsor"));
    aboutData.addCredit("Chris Xiong", QCoreApplication::translate("main", "Sponsor"), "chris@pineapple.cat");
    aboutData.addCredit("CodingCat", QCoreApplication::translate("main", "Sponsor"));
    aboutData.addCredit("doyaGu", QCoreApplication::translate("main", "Sponsor"));
    aboutData.addCredit("Greg Dunn", QCoreApplication::translate("main", "Sponsor"));
    aboutData.addCredit("1i9h7_b1u3", QCoreApplication::translate("main", "Sponsor"));
    aboutData.addCredit("justforlxz", QCoreApplication::translate("main", "Sponsor"));
    aboutData.addCredit("Lttqzz2", QCoreApplication::translate("main", "Sponsor"));
    aboutData.addCredit("panpanpdj", QCoreApplication::translate("main", "Sponsor"));
    aboutData.addCredit("pzm9012", QCoreApplication::translate("main", "Sponsor"));
    aboutData.addCredit("田浪", QCoreApplication::translate("main", "Sponsor"));
    aboutData.setLicenseTextFile(":/LICENSE");
    KAboutData::setApplicationData(aboutData);
    a.setWindowIcon(QIcon(":/icons/icons/pineapple-notepad.svg"));

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("Files to open"), QStringLiteral("[files...]"));
    parser.process(a);
    aboutData.processCommandLine(&parser);

    SingleApplicationManager sam("_pineapple_notepad_owo_");
    if (sam.checkSingleInstance(QVariant::fromValue(parser.positionalArguments()))) {
        return 0;
    } else {
        sam.createSingleInstance();
    }

    {
        KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("Startup"));
        const QString dismissedVersion = group.readEntry(QStringLiteral("NaggingDialogDismissedVersion"), QString());
        const QString currentVersion = QStringLiteral(PINEAPPLE_NOTEPAD_VERSION_STR);
        // Skip the startup reminder if the user has dismissed it for this
        // version, or if they've previously imported a valid license
        // (isActivated is a cached hint; we intentionally do NOT re-verify
        // the signature here - per design, startup must not do activation
        // checks).
        if (Q_UNLIKELY(dismissedVersion != currentVersion) && !AppSettings::isActivated()) {
            if (SponsorDialog::showStartupReminder()) {
                group.writeEntry(QStringLiteral("NaggingDialogDismissedVersion"), currentVersion);
                group.sync();
            }
        }
    }

    if (AppSettings::editorTheme() == QStringLiteral("auto")) {
        const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
        const QString resolved = (scheme == Qt::ColorScheme::Dark)
            ? QStringLiteral("Pineapple Notepad Dark")
            : QStringLiteral("Pineapple Notepad Light");
        AppSettings::setEditorTheme(resolved);
        AppSettings::self()->save();
    }

    DocumentManager *docMgr = new DocumentManager(&a);

    MainWindow *mw = new MainWindow(docMgr);
    if (!mw->restorePreviousSession()) {
        mw->newFile();
    }

    if (!parser.positionalArguments().isEmpty()) {
        mw->openFiles(parser.positionalArguments());
    }

    mw->show();

#ifdef Q_OS_MACOS
    FileOpenEventHandler * fileOpenEventHandler = new FileOpenEventHandler(&a);
    a.installEventFilter(fileOpenEventHandler);
    a.connect(fileOpenEventHandler, &FileOpenEventHandler::fileOpen, [&mw](const QUrl & url){
        if (mw->isHidden()) {
            mw->showNormal();
        } else {
            mw->activateWindow();
        }
        mw->openFiles({url.toLocalFile()});
    });
#endif // Q_OS_MACOS

    QObject::connect(&sam, &SingleApplicationManager::dataReached, [mw](const QVariant &data){
        if (data.canConvert<QStringList>()) {
            mw->openFiles(data.toStringList());
            mw->activateWindow();
            mw->raise();
        }
    });

    return a.exec();
}
