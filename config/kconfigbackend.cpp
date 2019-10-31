/*
   This file is part of the KDE libraries
   Copyright (c) 2006 Thomas Braxton <brax108@cox.net>
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

#include "kconfigbackend_p.h"

#include <QDateTime>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QDebug>

#include "kconfigini_p.h"
#include "kconfigdata.h"

typedef QExplicitlySharedDataPointer<KConfigBackend> BackendPtr;

class KConfigBackendPrivate
{
public:
    QString localFileName;

    static QString whatSystem(const QString & /*fileName*/)
    {
        return QStringLiteral("INI");
    }
};

void KConfigBackend::registerMappings(const KEntryMap & /*entryMap*/)
{
}

BackendPtr KConfigBackend::create(const QString &file, const QString &sys)
{
    //qDebug() << "creating a backend for file" << file << "with system" << sys;
    KConfigBackend *backend = nullptr;

#if 0 // TODO port to Qt5 plugin loading
    const QString system = (sys.isEmpty() ? KConfigBackendPrivate::whatSystem(file) : sys);
    if (system.compare(QLatin1String("INI"), Qt::CaseInsensitive) != 0) {
        const QString constraint = QString::fromLatin1("[X-KDE-PluginInfo-Name] ~~ '%1'").arg(system);
        KService::List offers = KServiceTypeTrader::self()->query(QLatin1String("KConfigBackend"), constraint);

        //qDebug() << "found" << offers.count() << "offers for KConfigBackend plugins with name" << system;
        foreach (const KService::Ptr &offer, offers) {
            backend = offer->createInstance<KConfigBackend>(nullptr);
            if (backend) {
                //qDebug() << "successfully created a backend for" << system;
                backend->setFilePath(file);
                return BackendPtr(backend);
            }
        } // foreach offers
    }
#else
    Q_UNUSED(sys);
#endif

    //qDebug() << "default creation of the Ini backend";
    backend = new KConfigIniBackend;
    backend->setFilePath(file);
    return BackendPtr(backend);
}

KConfigBackend::KConfigBackend()
    : d(new KConfigBackendPrivate)
{
}

KConfigBackend::~KConfigBackend()
{
    delete d;
}

QString KConfigBackend::filePath() const
{
    return d->localFileName;
}

void KConfigBackend::setLocalFilePath(const QString &file)
{
    d->localFileName = file;
}
