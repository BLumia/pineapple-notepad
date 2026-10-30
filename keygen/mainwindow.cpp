#include "mainwindow.h"

#include "keygentab.h"
#include "licensetab.h"
#include "verifytab.h"

#include <QTabWidget>

using namespace Qt::StringLiterals;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(u"License Key Generator"_s);
    resize(640, 480);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(new KeygenTab(this),  tr("&Generate Public Key"));
    tabs->addTab(new LicenseTab(this), tr("&Generate License"));
    tabs->addTab(new VerifyTab(this),  tr("&Verify License"));

    setCentralWidget(tabs);
}
