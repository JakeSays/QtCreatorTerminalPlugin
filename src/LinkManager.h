#pragma once

#include <QHash>

namespace cxxtools
{
class LinkManager
{
    QHash<int, QString> _idMap;
    QHash<QString, int> _linkMap;

    int _idSequence = 1;

public:
    LinkManager();

    int AddLink(QString link);
    QString FindLink(int linkId);
};
} //cxxtools
