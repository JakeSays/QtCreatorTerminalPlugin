/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "klocalizedtranslator.h"
#include "klocalizedstring.h"

// Qt
#include <QMetaObject>
#include <QMetaProperty>

class KLocalizedTranslatorPrivate
{
public:
    QString translationDomain;
    QSet<QString> monitoredContexts;
};

KLocalizedTranslator::KLocalizedTranslator(QObject *parent)
    : QTranslator(parent)
    , d(new KLocalizedTranslatorPrivate)
{
}

KLocalizedTranslator::~KLocalizedTranslator()
{
}

void KLocalizedTranslator::setTranslationDomain(const QString &translationDomain)
{
    d->translationDomain = translationDomain;
}

void KLocalizedTranslator::addContextToMonitor(const QString &context)
{
    d->monitoredContexts.insert(context);
}

void KLocalizedTranslator::removeContextToMonitor(const QString &context)
{
    d->monitoredContexts.remove(context);
}

QString KLocalizedTranslator::translate(const char *context, const char *sourceText, const char *disambiguation, int n) const
{
    if (d->translationDomain.isEmpty() || !d->monitoredContexts.contains(QString::fromUtf8(context))) {
        return QTranslator::translate(context, sourceText, disambiguation, n);
    }
    if (qstrlen(disambiguation) == 0) {
        return ki18nd(d->translationDomain.toUtf8().constData(), sourceText).toString();
    } else {
        return ki18ndc(d->translationDomain.toUtf8().constData(), disambiguation, sourceText).toString();
    }
}
