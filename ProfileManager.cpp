/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "ProfileManager.h"

#include "TerminalDebug.h"

// Qt
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QString>

// KDE
#include <config/ksharedconfig.h>
#include <config/kconfig.h>
#include <config/kconfiggroup.h>
#include <KLocalizedString>
#include <KMessageBox>

// Konsole
#include "ProfileReader.h"
//#include "ProfileWriter.h"

using namespace terminal;

static bool profileIndexLessThan(const Profile::Ptr& p1, const Profile::Ptr& p2)
{
    return p1->menuIndexAsInt() < p2->menuIndexAsInt();
}

static bool profileNameLessThan(const Profile::Ptr& p1, const Profile::Ptr& p2)
{
    return QString::localeAwareCompare(p1->name(), p2->name()) < 0;
}

static bool stringLessThan(const QString& p1, const QString& p2)
{
    return QString::localeAwareCompare(p1, p2) < 0;
}

static void sortByIndexProfileList(QList<Profile::Ptr>& list)
{
    std::stable_sort(list.begin(), list.end(), profileIndexLessThan);
}

static void sortByNameProfileList(QList<Profile::Ptr>& list)
{
    std::stable_sort(list.begin(), list.end(), profileNameLessThan);
}

static QString GetDefaultProfilePath(const QString& profileBaseName)
{
//    auto profileName = profileBaseName;
//    profileName.replace(".profile", ".qtcreator.profile");

//    if (profileName.isEmpty())
//    {
//        return QString();
//    }

    return profileBaseName;
}

ProfileManager::ProfileManager()
    : _profiles(QSet<Profile::Ptr>())
    , _favorites(QSet<Profile::Ptr>())
    , _defaultProfile(nullptr)
    , _fallbackProfile(nullptr)
    , _loadedAllProfiles(false)
{
    //load fallback profile
    _fallbackProfile = Profile::Ptr(new Profile());
    _fallbackProfile->useFallback();
    addProfile(_fallbackProfile);

    // lookup the default profile specified in <App>rc
    // for stand-alone Konsole, appConfig is just konsolerc
    // for konsolepart, appConfig might be yakuakerc, dolphinrc, katerc...
    KSharedConfigPtr appConfig = KSharedConfig::openConfig();
    KConfigGroup group = appConfig->group("Desktop Entry");
    QString defaultProfileFileName = group.readEntry("DefaultProfile", "");

    // if the hosting application of konsolepart does not specify its own
    // default profile, use the default profile of stand-alone Konsole.
    if (defaultProfileFileName.isEmpty()) {
        KSharedConfigPtr konsoleConfig = KSharedConfig::openConfig(QStringLiteral("konsolerc"));
        group = konsoleConfig->group("Desktop Entry");
        defaultProfileFileName = group.readEntry("DefaultProfile", "");
    }

    auto profilePath = GetDefaultProfilePath(defaultProfileFileName);

    _defaultProfile = _fallbackProfile;
    if (!profilePath.isEmpty())
    {
        // load the default profile
        Profile::Ptr profile = loadProfile(profilePath);
        if (profile)
        {
            _defaultProfile = profile;
        }
    }
    else
    {
        // load the default profile from a qrc resource
    }

    Q_ASSERT(_profiles.count() > 0);
    Q_ASSERT(_defaultProfile);
}

ProfileManager::~ProfileManager() = default;

Q_GLOBAL_STATIC(ProfileManager, theProfileManager)
ProfileManager* ProfileManager::instance()
{
    return theProfileManager;
}

Profile::Ptr ProfileManager::loadProfile(const QString& shortPath)
{
    // the fallback profile has a 'special' path name, "FALLBACK/"
    if (shortPath == _fallbackProfile->path()) {
        return _fallbackProfile;
    }

    QString path = shortPath;

    // add a suggested suffix and relative prefix if missing
    QFileInfo fileInfo(path);

    if (fileInfo.isDir()) {
        return Profile::Ptr();
    }

    if (fileInfo.suffix() != QLatin1String("profile")) {
        path.append(QLatin1String(".profile"));
    }

//    if (fileInfo.path().isEmpty() || fileInfo.path() == QLatin1Char('.')) {
//        path.prepend(QLatin1String("konsole") + QDir::separator());
//    }

//    // if the file is not an absolute path, look it up
//    if (!fileInfo.isAbsolute()) {
//        path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, path);
//    }

    // if the file is not found, return immediately
    if (path.isEmpty()) {
        return Profile::Ptr();
    }

    // check that we have not already loaded this profile
    for(const auto& profile : _profiles) {
        if (profile->path() == path) {
            return profile;
        }
    }

    // guard to prevent problems if a profile specifies itself as its parent
    // or if there is recursion in the "inheritance" chain
    // (eg. two profiles, A and B, specifying each other as their parents)
    static QStack<QString> recursionGuard;
    PopStackOnExit<QString> popGuardOnExit(recursionGuard);

    if (recursionGuard.contains(path)) {
        qCDebug(TerminalDebug) << "Ignoring attempt to load profile recursively from" << path;
        return _fallbackProfile;
    } else {
        recursionGuard.push(path);
    }

    // load the profile
    ProfileReader reader;

    Profile::Ptr newProfile = Profile::Ptr(new Profile(fallbackProfile()));
    newProfile->setProperty(Profile::Path, path);

    QString parentProfilePath;
    bool result = reader.readProfile(path, newProfile, parentProfilePath);

    if (!parentProfilePath.isEmpty()) {
        Profile::Ptr parentProfile = loadProfile(parentProfilePath);
        newProfile->setParent(parentProfile);
    }

    if (!result) {
        qCDebug(TerminalDebug) << "Could not load profile from " << path;
        return Profile::Ptr();
    } else if (newProfile->name().isEmpty()) {
        qCWarning(TerminalDebug) << path << " does not have a valid name, ignoring.";
        return Profile::Ptr();
    } else {
        addProfile(newProfile);
        return newProfile;
    }
}
QStringList ProfileManager::availableProfilePaths() const
{
    ProfileReader reader;

    QStringList paths;
    paths += reader.findProfiles();

    std::stable_sort(paths.begin(), paths.end(), stringLessThan);

    return paths;
}

QStringList ProfileManager::availableProfileNames() const
{
    QStringList names;

    for(auto profile : ProfileManager::instance()->allProfiles())
    {
        if (!profile->isHidden()) {
            names.push_back(profile->name());
        }
    }

    std::stable_sort(names.begin(), names.end(), stringLessThan);

    return names;
}

void ProfileManager::loadAllProfiles()
{
    if (_loadedAllProfiles) {
        return;
    }

    const QStringList& paths = availableProfilePaths();
    for(const auto& path : paths) {
        loadProfile(path);
    }

    _loadedAllProfiles = true;
}

void ProfileManager::sortProfiles(QList<Profile::Ptr>& list)
{
    QList<Profile::Ptr> lackingIndices;
    QList<Profile::Ptr> havingIndices;

    for (const auto & i : list) {
        // dis-regard the fallback profile
        if (i->path() == _fallbackProfile->path()) {
            continue;
        }

        if (i->menuIndexAsInt() == 0) {
            lackingIndices.append(i);
        } else {
            havingIndices.append(i);
        }
    }

    // sort by index
    sortByIndexProfileList(havingIndices);

    // sort alphabetically those w/o an index
    sortByNameProfileList(lackingIndices);

    // Put those with indices in sequential order w/o any gaps
    int i = 0;
    for (i = 0; i < havingIndices.size(); ++i) {
        Profile::Ptr tempProfile = havingIndices.at(i);
        tempProfile->setProperty(Profile::MenuIndex, QString::number(i + 1));
        havingIndices.replace(i, tempProfile);
    }
    // Put those w/o indices in sequential order
    for (int j = 0; j < lackingIndices.size(); ++j) {
        Profile::Ptr tempProfile = lackingIndices.at(j);
        tempProfile->setProperty(Profile::MenuIndex, QString::number(j + 1 + i));
        lackingIndices.replace(j, tempProfile);
    }

    // combine the 2 list: first those who had indices
    list.clear();
    list.append(havingIndices);
    list.append(lackingIndices);
}

void ProfileManager::saveSettings()
{
    // save default profile
    saveDefaultProfile();

    // ensure default/favorites/shortcuts settings are synced into disk
    KSharedConfigPtr appConfig = KSharedConfig::openConfig();
    appConfig->sync();
}

QList<Profile::Ptr> ProfileManager::allProfiles()
{
    loadAllProfiles();

    return _profiles.toList();
}

QList<Profile::Ptr> ProfileManager::loadedProfiles() const
{
    return _profiles.toList();
}

Profile::Ptr ProfileManager::defaultProfile() const
{
    return _defaultProfile;
}
Profile::Ptr ProfileManager::fallbackProfile() const
{
    return _fallbackProfile;
}

QString ProfileManager::saveProfile(const Profile::Ptr &profile)
{
    return QString();
//    ProfileWriter writer;

//    QString newPath = writer.getPath(profile);

//    if (!writer.writeProfile(newPath, profile)) {
//        KMessageBox::sorry(nullptr,
//                           i18n("Konsole does not have permission to save this profile to %1", newPath));
//    }

//    return newPath;
}

void ProfileManager::changeProfile(Profile::Ptr profile,
                                   QHash<Profile::Property, QVariant> propertyMap, bool persistent)
{
    Q_ASSERT(profile);

    const QString origPath = profile->path();

    // never save a profile with empty name into disk!
    persistent = persistent && !profile->name().isEmpty();

    Profile::Ptr newProfile;

    // If we are asked to store the fallback profile (which has an
    // invalid path by design), we reset the path to an empty string
    // which will make the profile writer automatically generate a
    // proper path.
    if (persistent && profile->path() == _fallbackProfile->path()) {

        // Generate a new name, so it is obvious what is actually built-in
        // in the profile manager
        QList<Profile::Ptr> existingProfiles = allProfiles();
        QStringList existingProfileNames;
        for (auto existingProfile : existingProfiles) {
            existingProfileNames.append(existingProfile->name());
        }

        int nameSuffix = 1;
        QString newName;
        QString newTranslatedName;
        do {
            newName = QLatin1String("Profile ") + QString::number(nameSuffix);
            newTranslatedName = i18nc("The default name of a profile", "Profile #%1", nameSuffix);
            // TODO: remove the # above and below - too many issues
            newTranslatedName.remove(QLatin1Char('#'));
            nameSuffix++;
        } while (existingProfileNames.contains(newName));

        newProfile = Profile::Ptr(new Profile(ProfileManager::instance()->fallbackProfile()));
        newProfile->clone(profile, true);
        newProfile->setProperty(Profile::UntranslatedName, newName);
        newProfile->setProperty(Profile::Name, newTranslatedName);
        newProfile->setProperty(Profile::MenuIndex, QStringLiteral("0"));
        newProfile->setHidden(false);

        addProfile(newProfile);
        setDefaultProfile(newProfile);

    } else {
        newProfile = profile;
    }

    // insert the changes into the existing Profile instance
    QListIterator<Profile::Property> iter(propertyMap.keys());
    while (iter.hasNext()) {
        const Profile::Property property = iter.next();
        newProfile->setProperty(property, propertyMap[property]);
    }

    // when changing a group, iterate through the profiles
    // in the group and call changeProfile() on each of them
    //
    // this is so that each profile in the group, the profile is
    // applied, a change notification is emitted and the profile
    // is saved to disk
    ProfileGroup::Ptr group = newProfile->asGroup();
    if (group) {
        for(const auto& groupProfile : group->profiles()) {
            changeProfile(groupProfile, propertyMap, persistent);
        }
        return;
    }

    // save changes to disk, unless the profile is hidden, in which case
    // it has no file on disk
    if (persistent && !newProfile->isHidden()) {
        newProfile->setProperty(Profile::Path, saveProfile(newProfile));
        // if the profile was renamed, after saving the new profile
        // delete the old/redundant profile.
        // only do this if origPath is not empty, because it's empty
        // when creating a new profile, this works around a bug where
        // the newly created profile appears twice in the ProfileSettings
        // dialog
        if (!origPath.isEmpty() && (newProfile->path() != origPath)) {
            // this is needed to include the old profile too
            _loadedAllProfiles = false;
           const QList<Profile::Ptr> availableProfiles = ProfileManager::instance()->allProfiles();
            for (auto oldProfile : availableProfiles)
            {
                if (oldProfile->path() == origPath) {
                    // assign the same shortcut of the old profile to
                    // the newly renamed profile
                    deleteProfile(oldProfile);
                }
            }
        }
    }

    // notify the world about the change
    emit profileChanged(newProfile);
}

void ProfileManager::addProfile(const Profile::Ptr &profile)
{
    if (_profiles.isEmpty()) {
        _defaultProfile = profile;
    }

    _profiles.insert(profile);

    emit profileAdded(profile);
}

bool ProfileManager::deleteProfile(Profile::Ptr profile)
{
    bool wasDefault = (profile == defaultProfile());

    if (profile) {
        // try to delete the config file
        if (profile->isPropertySet(Profile::Path) && QFile::exists(profile->path())) {
            if (!QFile::remove(profile->path())) {
                qCDebug(TerminalDebug) << "Could not delete profile: " << profile->path()
                           << "The file is most likely in a directory which is read-only.";

                return false;
            }
        }

        // remove from favorites, profile list, shortcut list etc.
        _profiles.remove(profile);

        // mark the profile as hidden so that it does not show up in the
        // Manage Profiles dialog and is not saved to disk
        profile->setHidden(true);
    }

    // If we just deleted the default profile, replace it with the first
    // profile in the list.
    if (wasDefault) {
        const QList<Profile::Ptr> existingProfiles = allProfiles();
        setDefaultProfile(existingProfiles.at(0));
    }

    emit profileRemoved(profile);

    return true;
}

void ProfileManager::setDefaultProfile(const Profile::Ptr &profile)
{
    Q_ASSERT(_profiles.contains(profile));

    _defaultProfile = profile;
}

void ProfileManager::saveDefaultProfile()
{
    QString path = _defaultProfile->path();
    ProfileWriter writer;

    if (path.isEmpty()) {
        path = writer.getPath(_defaultProfile);
    }

    QFileInfo fileInfo(path);

    KSharedConfigPtr appConfig = KSharedConfig::openConfig();
    KConfigGroup group = appConfig->group("Desktop Entry");
    group.writeEntry("DefaultProfile", fileInfo.fileName());
}


QString ProfileManager::normalizePath(const QString& path) const {
    return path;
//    QFileInfo fileInfo(path);
//    const QString location = QStandardPaths::locate(
//            QStandardPaths::GenericDataLocation, QStringLiteral("konsole/") + fileInfo.fileName());
//    return (!fileInfo.isAbsolute()) || location.isEmpty() ? path : fileInfo.fileName();
}

