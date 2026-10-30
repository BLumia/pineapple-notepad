// SPDX-License-Identifier: MIT
// Lexer Info Tool - GUI version entry point.

#include <QApplication>
#include "lexerinfowindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Lexer Info"));
    LexerInfoWindow w;
    w.show();
    return app.exec();
}
