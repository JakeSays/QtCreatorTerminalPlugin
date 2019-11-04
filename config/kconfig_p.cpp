/*
   This file is part of the KDE libraries
   Copyright (c) 2006, 2007 Thomas Braxton <kde.braxton@gmail.com>
   Copyright (c) 1999 Preston Brown <pbrown@kde.org>
   Copyright (c) 1997-1999 Matthias Kalle Dalheimer <kalle@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kconfig_p.h"

#include "config-kconfig.h"
#include "kconfig_core_log_settings.h"

#include <cstdlib>
#include <fcntl.h>

#include "kconfigbackend_p.h"
#include "kconfiggroup.h"

#include <qcoreapplication.h>
#include <qprocess.h>
#include <qbytearray.h>
#include <qfile.h>
#include <qlocale.h>
#include <qdir.h>
#include <QProcess>
#include <QSet>
#include <QBasicMutex>
#include <QMutexLocker>


bool KConfigPrivate::mappingsRegistered = false;

#ifndef Q_OS_WIN
static const Qt::CaseSensitivity sPathCaseSensitivity = Qt::CaseSensitive;
#else
static const Qt::CaseSensitivity sPathCaseSensitivity = Qt::CaseInsensitive;
#endif

KConfigPrivate::KConfigPrivate(KConfig::OpenFlags flags)
    : openFlags(flags),
      mBackend(nullptr),
      bDynamicBackend(true),
      bDirty(false),
      bReadDefaults(false),
      bFileImmutable(false),
      bForceGlobal(false),
      bSuppressGlobal(false),
      configState(KConfigBase::NoAccess)
{
    setLocale(QLocale().name());
}

bool KConfigPrivate::lockLocal()
{
    if (mBackend) {
        return mBackend->lock();
    }
    // anonymous object - pretend we locked it
    return true;
}

void KConfigPrivate::copyGroup(const QByteArray &source, const QByteArray &destination,
                               KConfigGroup *otherGroup, KConfigBase::WriteConfigFlags flags) const
{
    KEntryMap &otherMap = otherGroup->config()->d_ptr->entryMap;
    const int len = source.length();
    const bool sameName = (destination == source);

    // we keep this bool outside the foreach loop so that if
    // the group is empty, we don't end up marking the other config
    // as dirty erroneously
    bool dirtied = false;

    for (KEntryMap::ConstIterator entryMapIt(entryMap.constBegin()); entryMapIt != entryMap.constEnd(); ++entryMapIt) {
        const QByteArray &group = entryMapIt.key().mGroup;

        if (!group.startsWith(source)) { // nothing to do
            continue;
        }

        // don't copy groups that start with the same prefix, but are not sub-groups
        if (group.length() > len && group[len] != '\x1d') {
            continue;
        }

        KEntryKey newKey = entryMapIt.key();

        if (flags & KConfigBase::Localized) {
            newKey.bLocal = true;
        }

        if (!sameName) {
            newKey.mGroup.replace(0, len, destination);
        }

        KEntry entry = entryMap[ entryMapIt.key() ];
        dirtied = entry.bDirty = flags & KConfigBase::Persistent;

        if (flags & KConfigBase::Global) {
            entry.bGlobal = true;
        }

        otherMap[newKey] = entry;
    }

    if (dirtied) {
        otherGroup->config()->d_ptr->bDirty = true;
    }
}

QString KConfigPrivate::expandString(const QString &value)
{
    QString aValue = value;

    // check for environment variables and make necessary translations
    int nDollarPos = aValue.indexOf(QLatin1Char('$'));
    while (nDollarPos != -1 && nDollarPos + 1 < aValue.length()) {
        // there is at least one $
        if (aValue[nDollarPos + 1] != QLatin1Char('$')) {
            int nEndPos = nDollarPos + 1;
            // the next character is not $
            QStringRef aVarName;
            if (aValue[nEndPos] == QLatin1Char('{')) {
                while ((nEndPos <= aValue.length()) && (aValue[nEndPos] != QLatin1Char('}'))) {
                    nEndPos++;
                }
                nEndPos++;
                aVarName = aValue.midRef(nDollarPos + 2, nEndPos - nDollarPos - 3);
            } else {
                while (nEndPos <= aValue.length() &&
                        (aValue[nEndPos].isNumber() ||
                         aValue[nEndPos].isLetter() ||
                         aValue[nEndPos] == QLatin1Char('_'))) {
                    nEndPos++;
                }
                aVarName = aValue.midRef(nDollarPos + 1, nEndPos - nDollarPos - 1);
            }
            QString env;
            if (!aVarName.isEmpty()) {
#ifdef Q_OS_WIN
                if (aVarName == QLatin1String("HOME")) {
                    env = QDir::homePath();
                } else
#endif
                {
                    QByteArray pEnv = qgetenv(aVarName.toLatin1().constData());
                    if (!pEnv.isEmpty()) {
                        env = QString::fromLocal8Bit(pEnv.constData());
                    } else {
                        if (aVarName == QLatin1String("QT_DATA_HOME")) {
                            env = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
                        } else if (aVarName == QLatin1String("QT_CONFIG_HOME")) {
                            env = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
                        } else if (aVarName == QLatin1String("QT_CACHE_HOME")) {
                            env = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
                        }
                    }
                }
                aValue.replace(nDollarPos, nEndPos - nDollarPos, env);
                nDollarPos += env.length();
            } else {
                aValue.remove(nDollarPos, nEndPos - nDollarPos);
            }
        } else {
            // remove one of the dollar signs
            aValue.remove(nDollarPos, 1);
            nDollarPos++;
        }
        nDollarPos = aValue.indexOf(QLatin1Char('$'), nDollarPos);
    }

    return aValue;
}


QStringList KConfigPrivate::groupList(const QByteArray &group) const
{
    QByteArray theGroup = group + '\x1d';
    QSet<QString> groups;

    for (KEntryMap::ConstIterator entryMapIt(entryMap.constBegin()); entryMapIt != entryMap.constEnd(); ++entryMapIt) {
        const KEntryKey &key = entryMapIt.key();
        if (key.mKey.isNull() && key.mGroup.startsWith(theGroup)) {
            const QString groupname = QString::fromUtf8(key.mGroup.mid(theGroup.length()));
            groups << groupname.left(groupname.indexOf(QLatin1Char('\x1d')));
        }
    }

    return groups.toList();
}

static bool isGroupOrSubGroupMatch(const QByteArray &potentialGroup, const QByteArray &group)
{
    if (!potentialGroup.startsWith(group)) {
      return false;
    }
    return potentialGroup.length() == group.length() || potentialGroup[group.length()] == '\x1d';
}

// List all sub groups, including subsubgroups
QSet<QByteArray> KConfigPrivate::allSubGroups(const QByteArray &parentGroup) const
{
    QSet<QByteArray> groups;

    for (KEntryMap::const_iterator entryMapIt = entryMap.begin(); entryMapIt != entryMap.end(); ++entryMapIt) {
        const KEntryKey &key = entryMapIt.key();
        if (key.mKey.isNull() && isGroupOrSubGroupMatch(key.mGroup, parentGroup)) {
            groups << key.mGroup;
        }
    }
    return groups;
}

bool KConfigPrivate::hasNonDeletedEntries(const QByteArray &group) const
{
    for (KEntryMap::const_iterator it = entryMap.begin(); it != entryMap.end(); ++it) {
        const KEntryKey &key = it.key();
        // Check for any non-deleted entry
        if (isGroupOrSubGroupMatch(key.mGroup, group) && !key.mKey.isNull() && !it->bDeleted) {
            return true;
        }
    }
    return false;
}

QStringList KConfigPrivate::keyListImpl(const QByteArray &theGroup) const
{
    QStringList keys;

    const KEntryMapConstIterator theEnd = entryMap.constEnd();
    KEntryMapConstIterator it = entryMap.findEntry(theGroup);
    if (it != theEnd) {
        ++it; // advance past the special group entry marker

        QSet<QString> tmp;
        for (; it != theEnd && it.key().mGroup == theGroup; ++it) {
            const KEntryKey &key = it.key();
            if (!key.mKey.isNull() && !it->bDeleted) {
                tmp << QString::fromUtf8(key.mKey);
            }
        }
        keys = tmp.toList();
    }

    return keys;
}

void KConfigPrivate::changeFileName(const QString &name)
{
    fileName = name;

    QString file;
    if (name.isEmpty()) {
        // anonymous config
        openFlags = KConfig::SimpleConfig;
        return;
    } else if (QDir::isAbsolutePath(fileName)) {
        fileName = QFileInfo(fileName).canonicalFilePath();
        if (fileName.isEmpty())
        {
            // file doesn't exist (yet)
            fileName = name;
        }
        file = fileName;
    }
    else
    {
        fileName = QDir::currentPath() + "/" + name;

        auto canonicalFile = QFileInfo(fileName).canonicalFilePath();
        if (canonicalFile.isEmpty())
        {
            fileName = name;
        }
    }

    Q_ASSERT(!file.isEmpty());

    if (bDynamicBackend || !mBackend) { // allow dynamic changing of backend
        mBackend = KConfigBackend::create(file);
    } else {
        mBackend->setFilePath(file);
    }

    configState = mBackend->accessMode();
}

void KConfigPrivate::parseConfigFiles()
{
    // can only read the file if there is a backend and a file name
    if (mBackend && !fileName.isEmpty()) {

        bFileImmutable = false;

        QList<QString> files;
        files << mBackend->filePath();

        if (!isSimple()) {
            files = extraFiles.toList() + files;
        }


        const QByteArray utf8Locale = locale.toUtf8();
        for (const QString &file : qAsConst(files)) {
            if (file.compare(mBackend->filePath(), sPathCaseSensitivity) == 0) {
                switch (mBackend->parseConfig(utf8Locale, entryMap, KConfigBackend::ParseExpansions)) {
                case KConfigBackend::ParseOk:
                    break;
                case KConfigBackend::ParseImmutable:
                    bFileImmutable = true;
                    break;
                case KConfigBackend::ParseOpenError:
                    configState = KConfigBase::NoAccess;
                    break;
                }
            } else {
                QExplicitlySharedDataPointer<KConfigBackend> backend = KConfigBackend::create(file);
                bFileImmutable = (backend->parseConfig(utf8Locale, entryMap,
                                                       KConfigBackend::ParseDefaults | KConfigBackend::ParseExpansions)
                                  == KConfigBackend::ParseImmutable);
            }

            if (bFileImmutable) {
                break;
            }
        }
    }
}

bool KConfigPrivate::setLocale(const QString &aLocale)
{
    if (aLocale != locale) {
        locale = aLocale;
        return true;
    }
    return false;
}

KEntryMap::EntryOptions convertToOptions(KConfig::WriteConfigFlags flags);

bool KConfigPrivate::canWriteEntry(const QByteArray &group, const char *key, bool isDefault) const
{
    if (bFileImmutable ||
            entryMap.getEntryOption(group, key, KEntryMap::SearchLocalized, KEntryMap::EntryImmutable)) {
        return isDefault;
    }
    return true;
}

void KConfigPrivate::putData(const QByteArray &group, const char *key,
                             const QByteArray &value, KConfigBase::WriteConfigFlags flags, bool expand)
{
    KEntryMap::EntryOptions options = convertToOptions(flags);

    if (bForceGlobal) {
        options |= KEntryMap::EntryGlobal;
    }
    if (expand) {
        options |= KEntryMap::EntryExpansion;
    }

    if (value.isNull()) { // deleting entry
        options |= KEntryMap::EntryDeleted;
    }

    bool dirtied = entryMap.setEntry(group, key, value, options);
    if (dirtied && (flags & KConfigBase::Persistent)) {
        bDirty = true;
    }
}

void KConfigPrivate::revertEntry(const QByteArray &group, const char *key, KConfigBase::WriteConfigFlags flags)
{
    KEntryMap::EntryOptions options = convertToOptions(flags);

    bool dirtied = entryMap.revertEntry(group, key, options);
    if (dirtied) {
        bDirty = true;
    }
}

QByteArray KConfigPrivate::lookupData(const QByteArray &group, const char *key,
                                      KEntryMap::SearchFlags flags) const
{
    if (bReadDefaults) {
        flags |= KEntryMap::SearchDefaults;
    }
    const KEntryMapConstIterator it = entryMap.findEntry(group, key, flags);
    if (it == entryMap.constEnd()) {
        return QByteArray();
    }
    return it->mValue;
}

QString KConfigPrivate::lookupData(const QByteArray &group, const char *key,
                                   KEntryMap::SearchFlags flags, bool *expand) const
{
    if (bReadDefaults) {
        flags |= KEntryMap::SearchDefaults;
    }
    return entryMap.getEntry(group, key, QString(), flags, expand);
}
