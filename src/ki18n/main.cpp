/*  This file is part of the KDE libraries
    Copyright (C) 2015 Lukáš Tinkl <ltinkl@redhat.com>

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

#include <QCoreApplication>
#include <QLocale>
#include <QLibraryInfo>
#include <QTranslator>

// load global Qt translation, needed in KDE e.g. by lots of builtin dialogs (QColorDialog, QFontDialog) that we use
static bool loadTranslation(const QString &localeName)
{
    QTranslator *translator = new QTranslator(QCoreApplication::instance());
    if (!translator->load(QLocale(localeName), QStringLiteral("qt_"), QString(), QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        delete translator;
        return false;
    }
    QCoreApplication::instance()->installTranslator(translator);
    return true;
}

static void load()
{
    // The way Qt translation system handles plural forms makes it necessary to
    // have a translation file which contains only plural forms for `en`. That's
    // why we load the `en` translation unconditionally, then load the
    // translation for the current locale to overload it.
    loadTranslation(QStringLiteral("en"));

    QLocale locale = QLocale::system();
    if (locale.name() != QStringLiteral("en")) {
        if (!loadTranslation(locale.name())) {
            loadTranslation(locale.bcp47Name());
        }
    }
}

Q_COREAPP_STARTUP_FUNCTION(load)
