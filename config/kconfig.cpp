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

#include "kconfig.h"
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


KConfig::KConfig(const QString &file, OpenFlags mode)
    : d_ptr(new KConfigPrivate(mode))
{
    d_ptr->changeFileName(file); // set the local file name

    // read initial information off disk
    reparseConfiguration();
}

KConfig::KConfig(const QString &file, const QString &backend)
    : d_ptr(new KConfigPrivate(SimpleConfig))
{
    d_ptr->mBackend = KConfigBackend::create(file, backend);
    d_ptr->bDynamicBackend = false;
    d_ptr->changeFileName(file); // set the local file name

    // read initial information off disk
    reparseConfiguration();
}

KConfig::KConfig(KConfigPrivate &d)
    : d_ptr(&d)
{
}

KConfig::~KConfig()
{
    Q_D(KConfig);
    if (d->bDirty && (d->mBackend && d->mBackend->ref.load() == 1)) {
        sync();
    }
    delete d;
}

QStringList KConfig::groupList() const
{
    Q_D(const KConfig);
    QSet<QString> groups;

    for (KEntryMap::ConstIterator entryMapIt(d->entryMap.constBegin()); entryMapIt != d->entryMap.constEnd(); ++entryMapIt) {
        const KEntryKey &key = entryMapIt.key();
        const QByteArray group = key.mGroup;
        if (key.mKey.isNull() && !group.isEmpty() && group != "<default>" && group != "$Version") {
            const QString groupname = QString::fromUtf8(group);
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

QStringList KConfig::keyList(const QString &aGroup) const
{
    Q_D(const KConfig);
    const QByteArray theGroup(aGroup.isEmpty() ? "<default>" : aGroup.toUtf8());
    return d->keyListImpl(theGroup);
}

QMap<QString, QString> KConfig::entryMap(const QString &aGroup) const
{
    Q_D(const KConfig);
    QMap<QString, QString> theMap;
    const QByteArray theGroup(aGroup.isEmpty() ? "<default>" : aGroup.toUtf8());

    const KEntryMapConstIterator theEnd = d->entryMap.constEnd();
    KEntryMapConstIterator it = d->entryMap.findEntry(theGroup, nullptr, nullptr);
    if (it != theEnd) {
        ++it; // advance past the special group entry marker

        for (; it != theEnd && it.key().mGroup == theGroup; ++it) {
            // leave the default values and deleted entries out
            if (!it->bDeleted && !it.key().bDefault) {
                const QString key = QString::fromUtf8(it.key().mKey.constData());
                // the localized entry should come first, so don't overwrite it
                // with the non-localized entry
                if (!theMap.contains(key)) {
                    if (it->bExpand) {
                        theMap.insert(key, KConfigPrivate::expandString(QString::fromUtf8(it->mValue.constData())));
                    } else {
                        theMap.insert(key, QString::fromUtf8(it->mValue.constData()));
                    }
                }
            }
        }
    }

    return theMap;
}

bool KConfig::sync()
{
    Q_D(KConfig);

    if (isImmutable() || name().isEmpty()) {
        // can't write to an immutable or anonymous file.
        return false;
    }

    QHash<QString, QByteArrayList> notifyGroupsLocal;
    QHash<QString, QByteArrayList> notifyGroupsGlobal;

    if (d->bDirty && d->mBackend) {
        const QByteArray utf8Locale(locale().toUtf8());

        // Create the containing dir, maybe it wasn't there
        d->mBackend->createEnclosing();

        // lock the local file
        if (d->configState == ReadWrite && !d->lockLocal()) {
            qCWarning(KCONFIG_CORE_LOG) << "couldn't lock local file";
            return false;
        }

        // Rewrite global/local config only if there is a dirty entry in it.
        bool writeLocals = false;

        for (auto it = d->entryMap.constBegin(); it != d->entryMap.constEnd(); ++it) {
            auto e = it.value();
            if (e.bDirty) {
                writeLocals = true;
                if (e.bNotify) {
                    notifyGroupsLocal[QString::fromUtf8(it.key().mGroup)] << it.key().mKey;
                }
            }
        }

        d->bDirty = false; // will revert to true if a config write fails

        if (writeLocals) {
            if (!d->mBackend->writeConfig(utf8Locale, d->entryMap, KConfigBackend::WriteOptions())) {
                d->bDirty = true;
            }
        }
        if (d->mBackend->isLocked()) {
            d->mBackend->unlock();
        }
    }

    if (!notifyGroupsLocal.isEmpty()) {
        d->notifyClients(notifyGroupsLocal, QLatin1Char('/') + name());
    }
    if (!notifyGroupsGlobal.isEmpty()) {
        d->notifyClients(notifyGroupsGlobal, QStringLiteral("/kdeglobals"));
    }

    return !d->bDirty;
}

void KConfig::markAsClean()
{
    Q_D(KConfig);
    d->bDirty = false;

    // clear any dirty flags that entries might have set
    const KEntryMapIterator theEnd = d->entryMap.end();
    for (KEntryMapIterator it = d->entryMap.begin(); it != theEnd; ++it) {
        it->bDirty = false;
        it->bNotify = false;
    }
}

bool KConfig::isDirty() const
{
    Q_D(const KConfig);
    return d->bDirty;
}

//void KConfig::checkUpdate(const QString &id, const QString &updateFile)
//{
//    const KConfigGroup cg(this, "$Version");
//    const QString cfg_id = updateFile + QLatin1Char(':') + id;
//    const QStringList ids = cg.readEntry("update_info", QStringList());
//    if (!ids.contains(cfg_id)) {
//        QProcess::execute(QStringLiteral(KCONF_UPDATE_INSTALL_LOCATION), QStringList { QStringLiteral("--check"), updateFile });
//        reparseConfiguration();
//    }
//}

KConfig *KConfig::copyTo(const QString &file, KConfig *config) const
{
    Q_D(const KConfig);
    if (!config) {
        config = new KConfig(QString(), SimpleConfig);
    }
    config->d_func()->changeFileName(file);
    config->d_func()->entryMap = d->entryMap;
    config->d_func()->bFileImmutable = false;

    const KEntryMapIterator theEnd = config->d_func()->entryMap.end();
    for (KEntryMapIterator it = config->d_func()->entryMap.begin(); it != theEnd; ++it) {
        it->bDirty = true;
    }
    config->d_ptr->bDirty = true;

    return config;
}

QString KConfig::name() const
{
    Q_D(const KConfig);
    return d->fileName;
}


KConfig::OpenFlags KConfig::openFlags() const
{
    Q_D(const KConfig);
    return d->openFlags;
}

struct KConfigStaticData
{
    QString globalMainConfigName;
    // Keep a copy so we can use it in global dtors, after qApp is gone
    QStringList appArgs;
};
Q_GLOBAL_STATIC(KConfigStaticData, globalData)

void KConfig::setMainConfigName(const QString &str)
{
    globalData()->globalMainConfigName = str;
}

QString KConfig::mainConfigName()
{
    KConfigStaticData* data = globalData();
    if (data->appArgs.isEmpty())
        data->appArgs = QCoreApplication::arguments();

    // --config on the command line overrides everything else
    const QStringList args = data->appArgs;
    for (int i = 1; i < args.count(); ++i) {
        if (args.at(i) == QLatin1String("--config") && i < args.count() - 1) {
            return args.at(i + 1);
        }
    }
    const QString globalName = data->globalMainConfigName;
    if (!globalName.isEmpty()) {
        return globalName;
    }

    QString appName = QCoreApplication::applicationName();
    return appName + QLatin1String("rc");
}

void KConfig::reparseConfiguration()
{
    Q_D(KConfig);
    if (d->fileName.isEmpty()) {
        return;
    }

    // Don't lose pending changes
    if (!d->isReadOnly() && d->bDirty) {
        sync();
    }

    d->entryMap.clear();

    d->bFileImmutable = false;

    d->parseConfigFiles();
}

KConfig::AccessMode KConfig::accessMode() const
{
    Q_D(const KConfig);
    return d->configState;
}

void KConfig::addConfigSources(const QStringList &files)
{
    Q_D(KConfig);
    for (const QString &file : files) {
        d->extraFiles.push(file);
    }

    if (!files.isEmpty()) {
        reparseConfiguration();
    }
}

QStringList KConfig::additionalConfigSources() const
{
    Q_D(const KConfig);
    return d->extraFiles.toList();
}

QString KConfig::locale() const
{
    Q_D(const KConfig);
    return d->locale;
}


bool KConfig::setLocale(const QString &locale)
{
    Q_D(KConfig);
    if (d->setLocale(locale)) {
        reparseConfiguration();
        return true;
    }
    return false;
}

void KConfig::setReadDefaults(bool b)
{
    Q_D(KConfig);
    d->bReadDefaults = b;
}

bool KConfig::readDefaults() const
{
    Q_D(const KConfig);
    return d->bReadDefaults;
}

bool KConfig::isImmutable() const
{
    Q_D(const KConfig);
    return d->bFileImmutable;
}

bool KConfig::isGroupImmutableImpl(const QByteArray &aGroup) const
{
    Q_D(const KConfig);
    return isImmutable() || d->entryMap.getEntryOption(aGroup, nullptr, nullptr, KEntryMap::EntryImmutable);
}

KConfigGroup KConfig::groupImpl(const QByteArray &group)
{
    return KConfigGroup(this, group.constData());
}

const KConfigGroup KConfig::groupImpl(const QByteArray &group) const
{
    return KConfigGroup(this, group.constData());
}

KEntryMap::EntryOptions convertToOptions(KConfig::WriteConfigFlags flags)
{
    KEntryMap::EntryOptions options = nullptr;

    if (flags & KConfig::Persistent) {
        options |= KEntryMap::EntryDirty;
    }
    if (flags & KConfig::Global) {
        options |= KEntryMap::EntryGlobal;
    }
    if (flags & KConfig::Localized) {
        options |= KEntryMap::EntryLocalized;
    }
    if (flags.testFlag(KConfig::Notify)) {
        options |= KEntryMap::EntryNotify;
    }
    return options;
}

void KConfig::deleteGroupImpl(const QByteArray &aGroup, WriteConfigFlags flags)
{
    Q_D(KConfig);
    KEntryMap::EntryOptions options = convertToOptions(flags) | KEntryMap::EntryDeleted;

    const QSet<QByteArray> groups = d->allSubGroups(aGroup);
    for (const QByteArray &group : groups) {
        const QStringList keys = d->keyListImpl(group);
        for (const QString &_key : keys) {
            const QByteArray &key = _key.toUtf8();
            if (d->canWriteEntry(group, key.constData())) {
                d->entryMap.setEntry(group, key, QByteArray(), options);
                d->bDirty = true;
            }
        }
    }
}

bool KConfig::isConfigWritable(bool /*warnUser*/)
{
    Q_D(KConfig);
    bool allWritable = (d->mBackend ? d->mBackend->isWritable() : false);

//    if (warnUser && !allWritable) {
//        QString errorMsg;
//        if (d->mBackend) { // TODO how can be it be null? Set errorMsg appropriately
//            errorMsg = d->mBackend->nonWritableErrorMessage();
//        }

//        // Note: We don't ask the user if we should not ask this question again because we can't save the answer.
//        errorMsg += QCoreApplication::translate("KConfig", "Please contact your system administrator.");
//        QString cmdToExec = QStandardPaths::findExecutable(QStringLiteral("kdialog"));
//        if (!cmdToExec.isEmpty()) {
//            QProcess::execute(cmdToExec, QStringList()
//                              << QStringLiteral("--title") << QCoreApplication::applicationName()
//                              << QStringLiteral("--msgbox") << errorMsg);
//        }
//    }

    d->configState = allWritable ?  ReadWrite : ReadOnly; // update the read/write status

    return allWritable;
}

bool KConfig::hasGroupImpl(const QByteArray &aGroup) const
{
    Q_D(const KConfig);

    // No need to look for the actual group entry anymore, or for subgroups:
    // a group exists if it contains any non-deleted entry.

    return d->hasNonDeletedEntries(aGroup);
}

void KConfig::virtual_hook(int /*id*/, void * /*data*/)
{
    /* nothing */
}
