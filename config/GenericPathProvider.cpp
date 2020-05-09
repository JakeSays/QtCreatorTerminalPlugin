#include "GenericPathProvider.h"

using namespace devtools;

GenericPathProvider::GenericPathProvider()
{

}


QString devtools::GenericPathProvider::writableLocation(PathProvider::StandardLocation type)
{
    Q_UNUSED(type)
    return QString();
}

QStringList devtools::GenericPathProvider::standardLocations(PathProvider::StandardLocation type)
{
    Q_UNUSED(type)
    return QStringList();
}

QString devtools::GenericPathProvider::locate(PathProvider::StandardLocation type, const QString &fileName, PathProvider::LocateOptions options)
{
    Q_UNUSED(type)
    Q_UNUSED(fileName)
    Q_UNUSED(options)
    return QString();
}

QStringList devtools::GenericPathProvider::locateAll(PathProvider::StandardLocation type, const QString &fileName, PathProvider::LocateOptions options)
{
    Q_UNUSED(type)
    Q_UNUSED(fileName)
    Q_UNUSED(options)
    return QStringList();
}

QString devtools::GenericPathProvider::displayName(PathProvider::StandardLocation type)
{
    Q_UNUSED(type)
    return QString();
}

QString devtools::GenericPathProvider::findExecutable(const QString &executableName, const QStringList &paths)
{
    Q_UNUSED(executableName)
    Q_UNUSED(paths)
    return QString();
}
