#pragma once

#include <QStringList>
#include <QString>
#include <QFile>

#include "ColorScheme.h"

namespace terminal
{
enum class ColorSchemeLoaderType
{
    Unknown,
    File,
    Resource
};

class ColorSchemeLoader
{
public:
    virtual ColorSchemeLoaderType Type() = 0;
    virtual bool IsReadOnly() = 0;
    virtual QStringList ListSchemes() = 0;
    virtual ColorScheme LoadScheme(const QString& schemeName) = 0;
    virtual void SaveScheme(ColorScheme *scheme) = 0;

protected:
    ColorScheme* LoadScheme(const QString& schemeName, QFile* input);
};
} // namespace terminal
