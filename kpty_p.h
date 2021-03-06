/* This file is part of the KDE libraries

    Copyright (C) 2003,2007 Oswald Buddenhagen <ossi@kde.org>

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

#ifndef kpty_p_h
#define kpty_p_h

#include "kpty.h"

#include <PtyConfig.h>

#include <QByteArray>
#include <QString>

class KPtyPrivate
{
public:
    Q_DECLARE_PUBLIC(KPty)

    KPtyPrivate(KPty *parent);
    virtual ~KPtyPrivate();
#if ! HAVE_OPENPTY
    bool chownpty(bool grant);
#endif

    int masterFd;
    int slaveFd;
    bool ownMaster: 1;

    QByteArray ttyName;
    QString utempterPath;

    KPty *q_ptr;
};

#endif
