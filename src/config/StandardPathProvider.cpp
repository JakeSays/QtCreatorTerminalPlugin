#include "StandardPathProvider.h"

using namespace devtools;

StandardPathProvider::StandardPathProvider()
{

}

PathProvider &StandardPathProvider::Instance()
{
    static StandardPathProvider provider;

    return provider;
}


QString devtools::StandardPathProvider::writableLocation(PathProvider::StandardLocation type)
{
    return QStandardPaths::writableLocation(type);
}

QStringList devtools::StandardPathProvider::standardLocations(PathProvider::StandardLocation type)
{
    return QStandardPaths::standardLocations(type);
}

QString devtools::StandardPathProvider::locate(PathProvider::StandardLocation type, const QString &fileName, PathProvider::LocateOptions options)
{
    return QStandardPaths::locate(type, fileName, options);
}

QStringList devtools::StandardPathProvider::locateAll(PathProvider::StandardLocation type, const QString &fileName, PathProvider::LocateOptions options)
{
    return QStandardPaths::locateAll(type, fileName, options);
}

QString devtools::StandardPathProvider::displayName(PathProvider::StandardLocation type)
{
    return QStandardPaths::displayName(type);
}

QString devtools::StandardPathProvider::findExecutable(const QString &executableName, const QStringList &paths)
{
    return QStandardPaths::findExecutable(executableName, paths);
}
