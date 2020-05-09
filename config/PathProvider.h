#pragma once

#include <QString>
#include <QStandardPaths>

namespace devtools
{
class PathProvider
{
    PathProvider();

    friend class GenericPathProvider;
    friend class StandardPathProvider;

public:
    using StandardLocation = QStandardPaths::StandardLocation;
    using LocateOption = QStandardPaths::LocateOption;
    using LocateOptions = QStandardPaths::LocateOptions;

    PathProvider& Instance();

    virtual QString writableLocation(StandardLocation type) = 0;
    virtual QStringList standardLocations(StandardLocation type) = 0;

    virtual QString locate(StandardLocation type, const QString &fileName, LocateOptions options = LocateOption::LocateFile) = 0;
    virtual QStringList locateAll(StandardLocation type, const QString &fileName, LocateOptions options = LocateOption::LocateFile) = 0;
    virtual QString displayName(StandardLocation type) = 0;

    virtual QString findExecutable(const QString &executableName, const QStringList &paths = QStringList()) = 0;

};
} //devtools
