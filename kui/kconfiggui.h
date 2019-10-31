/*
   This file is part of the KDE libraries
   Copyright (c) 1999 Matthias Ettrich <ettrich@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) version 3, or any
   later version accepted by the membership of KDE e.V. (or its
   successor approved by the membership of KDE e.V.), which shall
   act as a proxy defined in Section 6 of version 3 of the license.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef KCONFIGGUI_H
#define KCONFIGGUI_H

#include <QString>

class KConfig;

/**
 * Interface-related functions.
 */
namespace KConfigGui
{
/**
 * Returns the current application session config object.
 *
 * @return A pointer to the application's instance specific
 * KConfig object.
 * @see KConfig
 */
KConfig *sessionConfig();

/**
 * Replaces the current application session config object.
 *
 * @param id  new session id
 * @param key new session key
 *
 * @since 5.11
 */
void setSessionConfig(const QString &id, const QString &key);

/**
 * Indicates if a session config has been created for that application
 * (ie. if sessionConfig() got called at least once)
 *
 * @return true if a sessionConfig object was created, false otherwise
 */
bool hasSessionConfig();

//#if KCONFIGGUI_ENABLE_DEPRECATED_SINCE(5, 11)
///**
// * Returns the name of the application session
// *
// * @return the application session name
// * @deprecated since 5.11, use sessionConfig()->name()
// */
//KCONFIGGUI_DEPRECATED_VERSION(5, 11, "Use KConfigGui::sessionConfig()->name()")
//KCONFIGGUI_EXPORT QString sessionConfigName();
//#endif
}

#endif // KCONFIGGUI_H
