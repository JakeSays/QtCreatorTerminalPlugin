#include "PathProvider.h"
#include "StandardPathProvider.h"
#include "GenericPathProvider.h"

using namespace devtools;

namespace
{
bool RunningOnKde()
{
    return qgetenv("XDG_CURRENT_DESKTOP").toLower() == "kde";
}

}

PathProvider::PathProvider()
{

}

PathProvider &PathProvider::Instance()
{
    static PathProvider* provider =
    RunningOnKde()
        ? (PathProvider*) new StandardPathProvider()
        : (PathProvider*) new GenericPathProvider();

    return *provider;
}
