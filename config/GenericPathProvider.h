#pragma once

#include <config/PathProvider.h>

namespace devtools
{
class GenericPathProvider : PathProvider
{
    GenericPathProvider();
    friend class PathProvider;

public:

    QString writableLocation(StandardLocation type) override;
    QStringList standardLocations(StandardLocation type) override;
    QString locate(StandardLocation type, const QString &fileName, LocateOptions options) override;
    QStringList locateAll(StandardLocation type, const QString &fileName, LocateOptions options) override;
    QString displayName(StandardLocation type) override;
    QString findExecutable(const QString &executableName, const QStringList &paths) override;
};

} // namespace devtools
