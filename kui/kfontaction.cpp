/* This file is part of the KDE libraries
    Copyright (C) 1999 Reginald Stadlbauer <reggie@kde.org>
              (C) 1999 Simon Hausmann <hausmann@kde.org>
              (C) 2000 Nicolas Hadacek <haadcek@kde.org>
              (C) 2000 Kurt Granroth <granroth@kde.org>
              (C) 2000 Michael Koch <koch@kde.org>
              (C) 2001 Holger Freyther <freyther@kde.org>
              (C) 2002 Ellis Whitehead <ellis@kde.org>
              (C) 2002 Joseph Wenninger <jowenn@kde.org>
              (C) 2003 Andras Mantia <amantia@kde.org>
              (C) 2005-2006 Hamish Rodda <rodda@kde.org>
              (C) 2007 Clarence Dang <dang@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kfontaction.h"

#include <QFontComboBox>

#include <kfontchooser.h>

class Q_DECL_HIDDEN KFontAction::KFontActionPrivate
{
public:
    KFontActionPrivate(KFontAction *parent)
        : q(parent),
          settingFont(0),
          fontFilters(QFontComboBox::AllFonts)
    {
    }

    void _k_slotFontChanged(const QFont &font)
    {
//        qCDebug(KWidgetsAddonsLog) << "QFontComboBox - slotFontChanged("
//                 << font.family() << ") settingFont=" << settingFont;
        if (settingFont) {
            return;
        }

        q->setFont(font.family());
        emit q->triggered(font.family());

//        qCDebug(KWidgetsAddonsLog) << "\tslotFontChanged done";
    }

    KFontAction *q;
    int settingFont;
    QFontComboBox::FontFilters fontFilters;
};

QStringList _k_fontList(const QFontComboBox::FontFilters &fontFilters = QFontComboBox::AllFonts)
{
    QFontDatabase dbase;

    QStringList families;
    if (fontFilters == QFontComboBox::AllFonts) {
        families = dbase.families();
    } else {
        const QFontComboBox::FontFilters scalableMask = (QFontComboBox::ScalableFonts | QFontComboBox::NonScalableFonts);
        const QFontComboBox::FontFilters spacingMask = (QFontComboBox::ProportionalFonts | QFontComboBox::MonospacedFonts);

        const auto allFamilies = dbase.families();
        for (const QString &family : allFamilies) {
            if ((fontFilters & scalableMask) && (fontFilters & scalableMask) != scalableMask) {
                if (bool(fontFilters & QFontComboBox::ScalableFonts) != dbase.isSmoothlyScalable(family)) {
                    continue;
                }
            }
            if ((fontFilters & spacingMask) && (fontFilters & spacingMask) != spacingMask) {
                if (bool(fontFilters & QFontComboBox::MonospacedFonts) != dbase.isFixedPitch(family)) {
                    continue;
                }
            }

            families << family;
        }
    }

    families.sort();
    return families;
}

KFontAction::KFontAction(uint fontListCriteria, QObject *parent)
    : KSelectAction(parent), d(new KFontActionPrivate(this))
{
    if (fontListCriteria & KFontChooser::FixedWidthFonts) {
        d->fontFilters |= QFontComboBox::MonospacedFonts;
    }

    if (fontListCriteria & KFontChooser::SmoothScalableFonts) {
        d->fontFilters |= QFontComboBox::ScalableFonts;
    }

    KSelectAction::setItems(_k_fontList(d->fontFilters));
    setEditable(true);
}

KFontAction::KFontAction(QObject *parent)
    : KSelectAction(parent), d(new KFontActionPrivate(this))
{
    KSelectAction::setItems(_k_fontList());
    setEditable(true);
}

KFontAction::KFontAction(const QString &text, QObject *parent)
    : KSelectAction(text, parent), d(new KFontActionPrivate(this))
{
    KSelectAction::setItems(_k_fontList());
    setEditable(true);
}

KFontAction::KFontAction(const QIcon &icon, const QString &text, QObject *parent)
    : KSelectAction(icon, text, parent), d(new KFontActionPrivate(this))
{
    KSelectAction::setItems(_k_fontList());
    setEditable(true);
}

KFontAction::~KFontAction()
{
    delete d;
}

QString KFontAction::font() const
{
    return currentText();
}

QWidget *KFontAction::createWidget(QWidget *parent)
{
//    qCDebug(KWidgetsAddonsLog) << "KFontAction::createWidget()";
#ifdef __GNUC__
//#warning FIXME: items need to be converted
#endif
    // This is the visual element on the screen.  This method overrides
    // the KSelectAction one, preventing KSelectAction from creating its
    // regular KComboBox.
    QFontComboBox *cb = new QFontComboBox(parent);
    cb->setFontFilters(d->fontFilters);

//    qCDebug(KWidgetsAddonsLog) << "\tset=" << font();
    // Do this before connecting the signal so that nothing will fire.
    cb->setCurrentFont(QFont(font().toLower()));
//    qCDebug(KWidgetsAddonsLog) << "\tspit back=" << cb->currentFont().family();

    connect(cb, &QFontComboBox::currentFontChanged, this, [this](const QFont &ft) { d->_k_slotFontChanged(ft); });
    cb->setMinimumWidth(cb->sizeHint().width());
    return cb;
}

/*
 * Maintenance note: Keep in sync with QFontComboBox::setCurrentFont()
 */
void KFontAction::setFont(const QString &family)
{
//    qCDebug(KWidgetsAddonsLog) << "KFontAction::setFont(" << family << ")";

    // Suppress triggered(QString) signal and prevent recursive call to ourself.
    d->settingFont++;

    const auto createdWidgets = this->createdWidgets();
    for (QWidget *w : createdWidgets) {
        QFontComboBox *cb = qobject_cast<QFontComboBox *>(w);
//        qCDebug(KWidgetsAddonsLog) << "\tw=" << w << "cb=" << cb;

        if (!cb) {
            continue;
        }

        cb->setCurrentFont(QFont(family.toLower()));
//        qCDebug(KWidgetsAddonsLog) << "\t\tw spit back=" << cb->currentFont().family();
    }

    d->settingFont--;

//    qCDebug(KWidgetsAddonsLog) << "\tcalling setCurrentAction()";

    QString lowerName = family.toLower();
    if (setCurrentAction(lowerName, Qt::CaseInsensitive)) {
        return;
    }

    int i = lowerName.indexOf(QLatin1String(" ["));
    if (i > -1) {
        lowerName.truncate(i);
        i = 0;
        if (setCurrentAction(lowerName, Qt::CaseInsensitive)) {
            return;
        }
    }

    lowerName += QLatin1String(" [");
    if (setCurrentAction(lowerName, Qt::CaseInsensitive)) {
        return;
    }

    // TODO: Inconsistent state if QFontComboBox::setCurrentFont() succeeded
    //       but setCurrentAction() did not and vice-versa.
//    qCDebug(KWidgetsAddonsLog) << "Font not found " << family.toLower();
}

#include "moc_kfontaction.cpp"
