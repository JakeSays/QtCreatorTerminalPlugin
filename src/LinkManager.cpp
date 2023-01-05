#include "LinkManager.h"

using namespace cxxtools;

LinkManager::LinkManager()
{

}

int LinkManager::AddLink(QString link)
{
    auto loc = _linkMap.find(link);
    if (loc != _linkMap.end())
    {
        return loc.value();
    }

    auto id = _idSequence++;
    _linkMap.insert(link, id);
    _idMap.insert(id, link);

    return id;
}

QString LinkManager::FindLink(int linkId)
{
    auto loc = _idMap.find(linkId);
    if (loc == _idMap.end())
    {
        return QString();
    }

    return loc.value();
}
