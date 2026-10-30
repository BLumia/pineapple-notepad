#include <QApplication>

#include "mainwindow.h"

using namespace Qt::StringLiterals;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(u"keygen"_s);
    app.setApplicationDisplayName(u"License Key Generator"_s);

    MainWindow w;
    w.show();

    return app.exec();
}
