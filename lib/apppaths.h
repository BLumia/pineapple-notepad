#pragma once

#include <QDir>
#include <QString>

namespace AppPaths
{
    QDir appDataPath();
    QString appDataSubdirPath(const QString &subdir);
}
