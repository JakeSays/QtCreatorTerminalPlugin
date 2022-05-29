/**
 * Copyright 2014 Laurent Montel <montel@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "kpluralhandlingspinbox.h"

class Q_DECL_HIDDEN KPluralHandlingSpinBox::KPluralHandlingSpinBoxPrivate
{
public:
    KPluralHandlingSpinBoxPrivate(QSpinBox *q)
        : q(q)
    {
        connect(q, SIGNAL(valueChanged(int)), q, SLOT(updateSuffix(int)));
    }

    void updateSuffix(int value)
    {
        if (!pluralSuffix.isEmpty()) {
            KLocalizedString s = pluralSuffix;
            q->setSuffix(s.subs(value).toString());
        }
    }

    QSpinBox *q;
    KLocalizedString pluralSuffix;
};


KPluralHandlingSpinBox::KPluralHandlingSpinBox(QWidget *parent)
    : QSpinBox(parent),
      d(new KPluralHandlingSpinBoxPrivate(this))
{
}

KPluralHandlingSpinBox::~KPluralHandlingSpinBox()
{
    delete d;
}

void KPluralHandlingSpinBox::setSuffix(const KLocalizedString &suffix)
{
    d->pluralSuffix = suffix;
    if (suffix.isEmpty()) {
        QSpinBox::setSuffix(QString());
    } else {
        d->updateSuffix(value());
    }
}
#include "moc_kpluralhandlingspinbox.cpp"
