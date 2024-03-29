/*
   This file is part of the KDE libraries
   Copyright (c) 2006, 2007 Thomas Braxton <kde.braxton@gmail.com>
   Copyright (c) 1999-2000 Preston Brown <pbrown@kde.org>
   Copyright (C) 1996-2000 Matthias Kalle Dalheimer <kalle@kde.org>

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

#include <config/kconfigdata.h>

QDebug operator<<(QDebug dbg, const KEntryKey &key)
{
    dbg.nospace() << "[" << key.mGroup << ", " << key.mKey << (key.bLocal ? " localized" : "") <<
                  (key.bDefault ? " default" : "") << (key.bRaw ? " raw" : "") << "]";
    return dbg.space();
}

QDebug operator<<(QDebug dbg, const KEntry &entry)
{
    dbg.nospace() << "[" << entry.mValue << (entry.bDirty ? " dirty" : "") <<
                  (entry.bGlobal ? " global" : "") << (entry.bImmutable ? " immutable" : "") <<
                  (entry.bDeleted ? " deleted" : "") << (entry.bReverted ? " reverted" : "") <<
                  (entry.bExpand ? " expand" : "") << "]";

    return dbg.space();
}

QMap< KEntryKey, KEntry >::Iterator KEntryMap::findExactEntry(const QByteArray &group, const QByteArray &key, SearchFlags flags)
{
    KEntryKey theKey(group, key, bool(flags & SearchFlag::SearchLocalized), bool(flags & SearchFlag::SearchDefaults));
    return find(theKey);
}

QMap< KEntryKey, KEntry >::Iterator KEntryMap::findEntry(const QByteArray &group, const QByteArray &key, SearchFlags flags)
{
    KEntryKey theKey(group, key, false, bool(flags & SearchFlag::SearchDefaults));

    // try the localized key first
    if (flags & SearchFlag::SearchLocalized) {
        theKey.bLocal = true;

        Iterator it = find(theKey);
        if (it != end()) {
            return it;
        }

        theKey.bLocal = false;
    }
    return find(theKey);
}

QMap< KEntryKey, KEntry >::ConstIterator KEntryMap::findEntry(const QByteArray &group, const QByteArray &key, SearchFlags flags) const
{
    KEntryKey theKey(group, key, false, bool(flags & SearchFlag::SearchDefaults));

    // try the localized key first
    if (flags & SearchFlag::SearchLocalized) {
        theKey.bLocal = true;

        ConstIterator it = find(theKey);
        if (it != constEnd()) {
            return it;
        }

        theKey.bLocal = false;
    }
    return find(theKey);
}

bool KEntryMap::setEntry(const QByteArray &group, const QByteArray &key, const QByteArray &value, EntryOptions options)
{
    KEntryKey k;
    KEntry e;
    bool newKey = false;

    const Iterator it = findExactEntry(group, key, SearchFlags(options >> 16));

    if (key.isEmpty()) { // inserting a group marker
        k.mGroup = group;
        e.bImmutable = (options & EntryOption::EntryImmutable);
        if (options & EntryOption::EntryDeleted) {
            qWarning("Internal KConfig error: cannot mark groups as deleted");
        }
        if (it == end()) {
            insert(k, e);
            return true;
        } else if (it.value() == e) {
            return false;
        }

        it.value() = e;
        return true;
    }

    if (it != end()) {
        if (it->bImmutable) {
            return false;    // we cannot change this entry. Inherits group immutability.
        }
        k = it.key();
        e = *it;
        //qDebug() << "found existing entry for key" << k;
    } else {
        // make sure the group marker is in the map
        KEntryMap const *that = this;
        ConstIterator cit = that->findEntry(group);
        if (cit == constEnd()) {
            insert(KEntryKey(group), KEntry());
        } else if (cit->bImmutable) {
            return false;    // this group is immutable, so we cannot change this entry.
        }

        k = KEntryKey(group, key);
        newKey = true;
    }

    // set these here, since we may be changing the type of key from the one we found
    k.bLocal = (options & EntryOption::EntryLocalized);
    k.bDefault = (options & EntryOption::EntryDefault);
    k.bRaw = (options & EntryOption::EntryRawKey);

    e.mValue = value;
    e.bDirty = e.bDirty || (options & EntryOption::EntryDirty);
    e.bNotify = e.bNotify || (options & EntryOption::EntryNotify);

    e.bGlobal = (options & EntryOption::EntryGlobal); //we can't use || here, because changes to entries in
    //kdeglobals would be written to kdeglobals instead
    //of the local config file, regardless of the globals flag
    e.bImmutable = e.bImmutable || (options & EntryOption::EntryImmutable);
    if (value.isNull()) {
        e.bDeleted = e.bDeleted || (options & EntryOption::EntryDeleted);
    } else {
        e.bDeleted = false;    // setting a value to a previously deleted entry
    }
    e.bExpand = (options & EntryOption::EntryExpansion);
    e.bReverted = false;
    if (options & EntryOption::EntryLocalized) {
        e.bLocalizedCountry = (options & EntryOption::EntryLocalizedCountry);
    } else {
        e.bLocalizedCountry = false;
    }

    if (newKey) {
        //qDebug() << "inserting" << k << "=" << value;
        insert(k, e);
        if (k.bDefault) {
            k.bDefault = false;
            //qDebug() << "also inserting" << k << "=" << value;
            insert(k, e);
        }
        // TODO check for presence of unlocalized key
        return true;
    } else {
//                KEntry e2 = it.value();
        if (options & EntryOption::EntryLocalized) {
            // fast exit checks for cases where the existing entry is more specific
            const KEntry &e2 = it.value();
            if (e2.bLocalizedCountry && !e.bLocalizedCountry) {
                // lang_COUNTRY > lang
                return false;
            }
        }
        if (it.value() != e) {
            //qDebug() << "changing" << k << "from" << e.mValue << "to" << value;
            it.value() = e;
            if (k.bDefault) {
                KEntryKey nonDefaultKey(k);
                nonDefaultKey.bDefault = false;
                insert(nonDefaultKey, e);
            }
            if (!(options & EntryOption::EntryLocalized)) {
                KEntryKey theKey(group, key, true, false);
                //qDebug() << "non-localized entry, remove localized one:" << theKey;
                remove(theKey);
                if (k.bDefault) {
                    theKey.bDefault = true;
                    remove(theKey);
                }
            }
            return true;
        } else {
            //qDebug() << k << "was already set to" << e.mValue;
            if (!(options & EntryOption::EntryLocalized)) {
                //qDebug() << "unchanged non-localized entry, remove localized one.";
                KEntryKey theKey(group, key, true, false);
                bool ret = false;
                Iterator cit = find(theKey);
                if (cit != end()) {
                    erase(cit);
                    ret = true;
                }
                if (k.bDefault) {
                    theKey.bDefault = true;
                    Iterator cit = find(theKey);
                    if (cit != end()) {
                        erase(cit);
                        return true;
                    }
                }
                return ret;
            }
            //qDebug() << "localized entry, unchanged, return false";
            // When we are writing a default, we know that the non-
            // default is the same as the default, so we can simply
            // use the same branch.
            return false;
        }
    }
}

QString KEntryMap::getEntry(const QByteArray &group, const QByteArray &key, const QString &defaultValue, SearchFlags flags, bool *expand) const
{
    const ConstIterator it = findEntry(group, key, flags);
    QString theValue = defaultValue;

    if (it != constEnd() && !it->bDeleted) {
        if (!it->mValue.isNull()) {
            const QByteArray data = it->mValue;
            theValue = QString::fromUtf8(data.constData(), data.length());
            if (expand) {
                *expand = it->bExpand;
            }
        }
    }

    return theValue;
}

bool KEntryMap::hasEntry(const QByteArray &group, const QByteArray &key, SearchFlags flags) const
{
    const ConstIterator it = findEntry(group, key, flags);
    if (it == constEnd()) {
        return false;
    }
    if (it->bDeleted) {
        return false;
    }
    if (key.isNull()) { // looking for group marker
        return it->mValue.isNull();
    }
    // if it->bReverted, we'll just return true; the real answer depends on lookup up with SearchDefaults, though.
    return true;
}

bool KEntryMap::getEntryOption(const QMap< KEntryKey, KEntry >::ConstIterator &it, EntryOption option) const
{
    if (it != constEnd()) {
        switch (option) {
        case EntryOption::EntryDirty:
            return it->bDirty;
        case EntryOption::EntryLocalized:
            return it.key().bLocal;
        case EntryOption::EntryGlobal:
            return it->bGlobal;
        case EntryOption::EntryImmutable:
            return it->bImmutable;
        case EntryOption::EntryDeleted:
            return it->bDeleted;
        case EntryOption::EntryExpansion:
            return it->bExpand;
        case EntryOption::EntryNotify:
            return it->bNotify;
        default:
            break; // fall through
        }
    }

    return false;
}

void KEntryMap::setEntryOption(QMap< KEntryKey, KEntry >::Iterator it, EntryOption option, bool bf)
{
    if (it != end()) {
        switch (option) {
        case EntryOption::EntryDirty:
            it->bDirty = bf;
            break;
        case EntryOption::EntryGlobal:
            it->bGlobal = bf;
            break;
        case EntryOption::EntryImmutable:
            it->bImmutable = bf;
            break;
        case EntryOption::EntryDeleted:
            it->bDeleted = bf;
            break;
        case EntryOption::EntryExpansion:
            it->bExpand = bf;
            break;
        case EntryOption::EntryNotify:
            it->bNotify = bf;
            break;
        default:
            break; // fall through
        }
    }
}

bool KEntryMap::revertEntry(const QByteArray &group, const QByteArray &key, EntryOptions options, SearchFlags flags)
{
    Q_ASSERT((flags & SearchFlag::SearchDefaults) == 0);
    Iterator entry = findEntry(group, key, flags);
    if (entry != end()) {
        //qDebug() << "reverting" << entry.key() << " = " << entry->mValue;
        if (entry->bReverted) { // already done before
            return false;
        }

        KEntryKey defaultKey(entry.key());
        defaultKey.bDefault = true;
        //qDebug() << "looking up default entry with key=" << defaultKey;
        const ConstIterator defaultEntry = constFind(defaultKey);
        if (defaultEntry != constEnd()) {
            Q_ASSERT(defaultEntry.key().bDefault);
            //qDebug() << "found, update entry";
            *entry = *defaultEntry; // copy default value, for subsequent lookups
        } else {
            entry->mValue = QByteArray();
        }
        entry->bNotify = entry->bNotify || (options & EntryOption::EntryNotify);
        entry->bDirty = true;
        entry->bReverted = true; // skip it when writing out to disk

        //qDebug() << "Here's what we have now:" << *this;
        return true;
    }
    return false;
}
