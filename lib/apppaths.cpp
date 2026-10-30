#include "apppaths.h"

#include <QStandardPaths>

namespace AppPaths
{

QDir appDataPath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::home().filePath(QStringLiteral(".pineapple-notepad"));
    }
    QDir().mkpath(base);
    return QDir(base);
}

QString appDataSubdirPath(const QString &subdir)
{
    QDir d = appDataPath();
    d.mkpath(subdir);
    d.cd(subdir);
    return d.absolutePath();
}

}
