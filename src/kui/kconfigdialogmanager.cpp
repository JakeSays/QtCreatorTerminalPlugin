/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Benjamin C Meyer (ben+kdelibs at meyerhome dot net)
 *  Copyright (C) 2003 Waldo Bastian <bastian@kde.org>
 *  Copyright (C) 2017 Friedrich W. H. Kossebau <kossebau@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "kconfigdialogmanager.h"
#include "kconfigwidgets_debug.h"

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QMetaObject>
#include <QMetaProperty>
#include <QTimer>
#include <QRadioButton>
#include <QLayout>

#include <kconfigskeleton.h>

#include "QContainerAdapters.h"

typedef QHash<QString, QByteArray> MyHash;
Q_GLOBAL_STATIC(MyHash, s_propertyMap)
Q_GLOBAL_STATIC(MyHash, s_changedMap)

class KConfigDialogManagerPrivate
{

public:
    KConfigDialogManagerPrivate(KConfigDialogManager *q) : q(q), insideGroupBox(false) { }

public:
    KConfigDialogManager * const q;

    /**
    * KConfigSkeleton object used to store settings
     */
    KCoreConfigSkeleton *m_conf = nullptr;

    /**
    * Dialog being managed
     */
    QWidget *m_dialog = nullptr;

    QHash<QString, QWidget *> knownWidget;
    QHash<QString, QWidget *> buddyWidget;
    QSet<QWidget *> allExclusiveGroupBoxes;
    bool insideGroupBox : 1;
    bool trackChanges : 1;
};

KConfigDialogManager::KConfigDialogManager(QWidget *parent, KCoreConfigSkeleton *conf)
    : QObject(parent), d(new KConfigDialogManagerPrivate(this))
{
    d->m_conf = conf;
    d->m_dialog = parent;
    init(true);
}

KConfigDialogManager::KConfigDialogManager(QWidget *parent, KConfigSkeleton *conf)
    : QObject(parent), d(new KConfigDialogManagerPrivate(this))
{
    d->m_conf = conf;
    d->m_dialog = parent;
    init(true);
}

KConfigDialogManager::~KConfigDialogManager()
{
    delete d;
}

// KF6: Drop this and get signals only from metaObject and/or widget's dynamic properties kcfg_property/kcfg_propertyNotify
void KConfigDialogManager::initMaps()
{
    if (s_propertyMap()->isEmpty()) {
        s_propertyMap()->insert(QStringLiteral("KButtonGroup"), "current");
        s_propertyMap()->insert(QStringLiteral("KColorButton"), "color");
        s_propertyMap()->insert(QStringLiteral("KColorCombo"), "color");
    }

    if (s_changedMap()->isEmpty()) {
        // QT
        s_changedMap()->insert(QStringLiteral("QCheckBox"), SIGNAL(stateChanged(int)));
        s_changedMap()->insert(QStringLiteral("QPushButton"), SIGNAL(clicked(bool)));
        s_changedMap()->insert(QStringLiteral("QRadioButton"), SIGNAL(toggled(bool)));
        s_changedMap()->insert(QStringLiteral("QGroupBox"), SIGNAL(toggled(bool)));
        s_changedMap()->insert(QStringLiteral("QComboBox"), SIGNAL(activated(int)));
        s_changedMap()->insert(QStringLiteral("QDateEdit"), SIGNAL(dateChanged(QDate)));
        s_changedMap()->insert(QStringLiteral("QTimeEdit"), SIGNAL(timeChanged(QTime)));
        s_changedMap()->insert(QStringLiteral("QDateTimeEdit"), SIGNAL(dateTimeChanged(QDateTime)));
        s_changedMap()->insert(QStringLiteral("QDial"), SIGNAL(valueChanged(int)));
        s_changedMap()->insert(QStringLiteral("QDoubleSpinBox"), SIGNAL(valueChanged(double)));
        s_changedMap()->insert(QStringLiteral("QLineEdit"), SIGNAL(textChanged(QString)));
        s_changedMap()->insert(QStringLiteral("QSlider"), SIGNAL(valueChanged(int)));
        s_changedMap()->insert(QStringLiteral("QSpinBox"), SIGNAL(valueChanged(int)));
        s_changedMap()->insert(QStringLiteral("QTextEdit"), SIGNAL(textChanged()));
        s_changedMap()->insert(QStringLiteral("QTextBrowser"), SIGNAL(sourceChanged(QString)));
        s_changedMap()->insert(QStringLiteral("QPlainTextEdit"), SIGNAL(textChanged()));
        s_changedMap()->insert(QStringLiteral("QTabWidget"), SIGNAL(currentChanged(int)));

        // KDE
        s_changedMap()->insert(QStringLiteral("KComboBox"), SIGNAL(activated(int)));
        s_changedMap()->insert(QStringLiteral("KFontComboBox"), SIGNAL(activated(int)));
        s_changedMap()->insert(QStringLiteral("KFontRequester"), SIGNAL(fontSelected(QFont)));
        s_changedMap()->insert(QStringLiteral("KFontChooser"),  SIGNAL(fontSelected(QFont)));
        s_changedMap()->insert(QStringLiteral("KColorCombo"), SIGNAL(activated(QColor)));
        s_changedMap()->insert(QStringLiteral("KColorButton"), SIGNAL(changed(QColor)));
        s_changedMap()->insert(QStringLiteral("KDatePicker"), SIGNAL(dateSelected(QDate)));
        s_changedMap()->insert(QStringLiteral("KDateWidget"), SIGNAL(changed(QDate)));
        s_changedMap()->insert(QStringLiteral("KDateTimeWidget"), SIGNAL(valueChanged(QDateTime)));
        s_changedMap()->insert(QStringLiteral("KEditListWidget"), SIGNAL(changed()));
        s_changedMap()->insert(QStringLiteral("KListWidget"), SIGNAL(itemSelectionChanged()));
        s_changedMap()->insert(QStringLiteral("KLineEdit"), SIGNAL(textChanged(QString)));
        s_changedMap()->insert(QStringLiteral("KRestrictedLine"), SIGNAL(textChanged(QString)));
        s_changedMap()->insert(QStringLiteral("KTextEdit"), SIGNAL(textChanged()));
        s_changedMap()->insert(QStringLiteral("KUrlRequester"),  SIGNAL(textChanged(QString)));
        s_changedMap()->insert(QStringLiteral("KUrlComboRequester"),  SIGNAL(textChanged(QString)));
        s_changedMap()->insert(QStringLiteral("KUrlComboBox"),  SIGNAL(urlActivated(QUrl)));
        s_changedMap()->insert(QStringLiteral("KButtonGroup"), SIGNAL(changed(int)));
    }
}

QHash<QString, QByteArray> *KConfigDialogManager::propertyMap()
{
    initMaps();
    return s_propertyMap();
}


void KConfigDialogManager::init(bool trackChanges)
{
    initMaps();
    d->trackChanges = trackChanges;

    // Go through all of the children of the widgets and find all known widgets
    (void) parseChildren(d->m_dialog, trackChanges);
}

void KConfigDialogManager::addWidget(QWidget *widget)
{
    (void) parseChildren(widget, true);
}

void KConfigDialogManager::setupWidget(QWidget *widget, KConfigSkeletonItem *item)
{
    QVariant minValue = item->minValue();
    if (minValue.isValid()) {
        // Only q3datetimeedit is using this property we can remove it if we stop supporting Qt3Support
        if (widget->metaObject()->indexOfProperty("minValue") != -1) {
            widget->setProperty("minValue", minValue);
        }
        if (widget->metaObject()->indexOfProperty("minimum") != -1) {
            widget->setProperty("minimum", minValue);
        }
    }
    QVariant maxValue = item->maxValue();
    if (maxValue.isValid()) {
        // Only q3datetimeedit is using that property we can remove it if we stop supporting Qt3Support
        if (widget->metaObject()->indexOfProperty("maxValue") != -1) {
            widget->setProperty("maxValue", maxValue);
        }
        if (widget->metaObject()->indexOfProperty("maximum") != -1) {
            widget->setProperty("maximum", maxValue);
        }
    }

    if (widget->whatsThis().isEmpty()) {
        QString whatsThis = item->whatsThis();
        if (!whatsThis.isEmpty()) {
            widget->setWhatsThis(whatsThis);
        }
    }

    if (widget->toolTip().isEmpty()) {
        QString toolTip = item->toolTip();
        if (!toolTip.isEmpty()) {
            widget->setToolTip(toolTip);
        }
    }

    // If it is a QGroupBox with only autoExclusive buttons
    // and has no custom property and the config item type
    // is an integer, assume we want to save the index like we did with
    // KButtonGroup instead of if it is checked or not
    QGroupBox *gb = qobject_cast<QGroupBox *>(widget);
    if (gb && getCustomProperty(gb).isEmpty()) {
        const KConfigSkeletonItem *item = d->m_conf->findItem(widget->objectName().mid(5));
        if (item->property().type() == QVariant::Int) {
            QObjectList children = gb->children();
            children.removeAll(gb->layout());
            const QList<QAbstractButton *> buttons = gb->findChildren<QAbstractButton *>();
            bool allAutoExclusiveDirectChildren = true;
            for (QAbstractButton *button : buttons) {
                allAutoExclusiveDirectChildren = allAutoExclusiveDirectChildren && button->autoExclusive() && button->parent() == gb;
            }
            if (allAutoExclusiveDirectChildren) {
                d->allExclusiveGroupBoxes << widget;
            }
        }
    }

    if (!item->isEqual(property(widget))) {
        setProperty(widget, item->property());
    }
}

bool KConfigDialogManager::parseChildren(const QWidget *widget, bool trackChanges)
{
    bool valueChanged = false;
    const QList<QObject *> listOfChildren = widget->children();
    if (listOfChildren.isEmpty()) { //?? XXX
        return valueChanged;
    }

    const QMetaMethod widgetModifiedSignal = metaObject()->method(metaObject()->indexOfSignal("widgetModified()"));
    Q_ASSERT(widgetModifiedSignal.isValid() && metaObject()->indexOfSignal("widgetModified()")>=0);

    for (QObject *object : listOfChildren) {
        if (!object->isWidgetType()) {
            continue;    // Skip non-widgets
        }

        QWidget *childWidget = static_cast<QWidget *>(object);

        QString widgetName = childWidget->objectName();
        bool bParseChildren = true;
        bool bSaveInsideGroupBox = d->insideGroupBox;

        if (widgetName.startsWith(u"kcfg_"_qs)) {
            // This is one of our widgets!
            QString configId = widgetName.mid(5);
            KConfigSkeletonItem *item = d->m_conf->findItem(configId);
            if (item) {
                d->knownWidget.insert(configId, childWidget);

                setupWidget(childWidget, item);

                if (trackChanges) {
                    bool changeSignalFound = false;

                    if (d->allExclusiveGroupBoxes.contains(childWidget)) {
                        const QList<QAbstractButton *> buttons = childWidget->findChildren<QAbstractButton *>();
                        for (QAbstractButton *button : buttons) {
                            connect(button, &QAbstractButton::toggled,
                                    this, &KConfigDialogManager::widgetModified);
                        }
                    }

                    QByteArray propertyChangeSignal = getCustomPropertyChangedSignal(childWidget);
                    if (propertyChangeSignal.isEmpty()) {
                        propertyChangeSignal = getUserPropertyChangedSignal(childWidget);
                    }

                    if (propertyChangeSignal.isEmpty()) {
                        // get the change signal from the meta object
                        const QMetaObject *metaObject = childWidget->metaObject();
                        QByteArray userproperty = getCustomProperty(childWidget);
                        if (userproperty.isEmpty()) {
                            userproperty = getUserProperty(childWidget);
                        }
                        if (!userproperty.isEmpty()) {
                            const int indexOfProperty = metaObject->indexOfProperty(userproperty);
                            if (indexOfProperty != -1) {
                                const QMetaProperty property = metaObject->property(indexOfProperty);
                                const QMetaMethod notifySignal = property.notifySignal();
                                if (notifySignal.isValid()) {
                                    connect(childWidget, notifySignal, this, widgetModifiedSignal);
                                    changeSignalFound = true;
                                }
                            }
                        } else {
                            qCWarning(KCONFIG_WIDGETS_LOG) << "Don't know how to monitor widget '" << childWidget->metaObject()->className() << "' for changes!";
                        }
                    } else {
                        connect(childWidget, propertyChangeSignal,
                                this, SIGNAL(widgetModified()));
                        changeSignalFound = true;
                    }

                    if (changeSignalFound) {
                        QComboBox *cb = qobject_cast<QComboBox *>(childWidget);
                        if (cb && cb->isEditable())
                            connect(cb, &QComboBox::editTextChanged,
                                    this, &KConfigDialogManager::widgetModified);
                    }
                }
                QGroupBox *gb = qobject_cast<QGroupBox *>(childWidget);
                if (!gb) {
                    bParseChildren = false;
                } else {
                    d->insideGroupBox = true;
                }
            } else {
                qCWarning(KCONFIG_WIDGETS_LOG) << "A widget named '" << widgetName << "' was found but there is no setting named '" << configId << "'";
            }
        } else if (QLabel *label = qobject_cast<QLabel *>(childWidget)) {
            QWidget *buddy = label->buddy();
            if (!buddy) {
                continue;
            }
            QString buddyName = buddy->objectName();
            if (buddyName.startsWith(u"kcfg_"_qs)) {
                // This is one of our widgets!
                QString configId = buddyName.mid(5);
                d->buddyWidget.insert(configId, childWidget);
            }
        }
//kf5: commented out to reduce debug output
// #ifndef NDEBUG
//     else if (!widgetName.isEmpty() && trackChanges)
//     {
//       QHash<QString, QByteArray>::const_iterator changedIt = s_changedMap()->constFind(childWidget->metaObject()->className());
//       if (changedIt != s_changedMap()->constEnd())
//       {
//         if ((!d->insideGroupBox || !qobject_cast<QRadioButton*>(childWidget)) &&
//             !qobject_cast<QGroupBox*>(childWidget) &&!qobject_cast<QTabWidget*>(childWidget) )
//           qCDebug(KCONFIG_WIDGETS_LOG) << "Widget '" << widgetName << "' (" << childWidget->metaObject()->className() << ") remains unmanaged.";
//       }
//     }
// #endif

        if (bParseChildren) {
            // this widget is not known as something we can store.
            // Maybe we can store one of its children.
            valueChanged |= parseChildren(childWidget, trackChanges);
        }
        d->insideGroupBox = bSaveInsideGroupBox;
    }
    return valueChanged;
}


void KConfigDialogManager::updateWidgets()
{
    bool changed = false;
    bool bSignalsBlocked = signalsBlocked();
    blockSignals(true);

    for (auto [key, widget] : QHashAdapter(d->knownWidget))
    {
        KConfigSkeletonItem *item = d->m_conf->findItem(key);
        if (!item) {
            qCWarning(KCONFIG_WIDGETS_LOG) << "The setting '" << key << "' has disappeared!";
            continue;
        }

        if (!item->isEqual(property(widget))) {
            setProperty(widget, item->property());
//        qCDebug(KCONFIG_WIDGETS_LOG) << "The setting '" << it.key() << "' [" << widget->className() << "] has changed";
            changed = true;
        }
        if (item->isImmutable()) {
            widget->setEnabled(false);
            QWidget *buddy = d->buddyWidget.value(key, nullptr);
            if (buddy) {
                buddy->setEnabled(false);
            }
        }
    }
    blockSignals(bSignalsBlocked);

    if (changed) {
        QTimer::singleShot(0, this, &KConfigDialogManager::widgetModified);
    }
}

void KConfigDialogManager::updateWidgetsDefault()
{
    bool bUseDefaults = d->m_conf->useDefaults(true);
    updateWidgets();
    d->m_conf->useDefaults(bUseDefaults);
}

void KConfigDialogManager::updateSettings()
{
    bool changed = false;

    for (auto [key, widget] : QHashAdapter(d->knownWidget))
    {

        KConfigSkeletonItem *item = d->m_conf->findItem(key);
        if (!item) {
            qCWarning(KCONFIG_WIDGETS_LOG) << "The setting '" << key << "' has disappeared!";
            continue;
        }

        QVariant fromWidget = property(widget);
        if (!item->isEqual(fromWidget)) {
            item->setProperty(fromWidget);
            changed = true;
        }
    }
    if (changed) {
        d->m_conf->save();
        emit settingsChanged();
    }
}

QByteArray KConfigDialogManager::getUserProperty(const QWidget *widget) const
{
    if (!s_propertyMap()->contains(QLatin1String(widget->metaObject()->className()))) {
        const QMetaObject *metaObject = widget->metaObject();
        const QMetaProperty user = metaObject->userProperty();
        if (user.isValid()) {
            s_propertyMap()->insert(QLatin1String(widget->metaObject()->className()), user.name());
            //qCDebug(KCONFIG_WIDGETS_LOG) << "class name: '" << widget->metaObject()->className()
            //<< " 's USER property: " << metaProperty.name() << endl;
        } else {
            return QByteArray(); //no USER property
        }
    }
    const QComboBox *cb = qobject_cast<const QComboBox *>(widget);
    if (cb) {
        const char *qcomboUserPropertyName = cb->QComboBox::metaObject()->userProperty().name();
        const int qcomboUserPropertyIndex = qcomboUserPropertyName ? cb->QComboBox::metaObject()->indexOfProperty(qcomboUserPropertyName) : -1;
        const char *widgetUserPropertyName = widget->metaObject()->userProperty().name();
        const int widgetUserPropertyIndex = widgetUserPropertyName ? cb->metaObject()->indexOfProperty(widgetUserPropertyName) : -1;

        // no custom user property set on subclass of QComboBox?
        if (qcomboUserPropertyIndex == widgetUserPropertyIndex) {
            return QByteArray(); // use the q/kcombobox special code
        }
    }

    return s_propertyMap()->value(QLatin1String(widget->metaObject()->className()));
}

QByteArray KConfigDialogManager::getCustomProperty(const QWidget *widget) const
{
    QVariant prop(widget->property("kcfg_property"));
    if (prop.isValid()) {
        if (!prop.canConvert(QVariant::ByteArray)) {
            qCWarning(KCONFIG_WIDGETS_LOG) << "kcfg_property on" << widget->metaObject()->className()
                       << "is not of type ByteArray";
        } else {
            return prop.toByteArray();
        }
    }
    return QByteArray();
}

QByteArray KConfigDialogManager::getUserPropertyChangedSignal(const QWidget *widget) const
{
    QHash<QString, QByteArray>::const_iterator changedIt = s_changedMap()->constFind(QLatin1String(widget->metaObject()->className()));

    if (changedIt == s_changedMap()->constEnd()) {
        // If the class name of the widget wasn't in the monitored widgets map, then look for
        // it again using the super class name. This fixes a problem with using QtRuby/Korundum
        // widgets with KConfigXT where 'Qt::Widget' wasn't being seen a the real deal, even
        // though it was a 'QWidget'.
        if (widget->metaObject()->superClass()) {
            changedIt = s_changedMap()->constFind(QLatin1String(widget->metaObject()->superClass()->className()));
        }
    }

    return (changedIt == s_changedMap()->constEnd()) ? QByteArray() : *changedIt;
}

QByteArray KConfigDialogManager::getCustomPropertyChangedSignal(const QWidget *widget) const
{
    QVariant prop(widget->property("kcfg_propertyNotify"));
    if (prop.isValid()) {
        if (!prop.canConvert(QVariant::ByteArray)) {
            qCWarning(KCONFIG_WIDGETS_LOG) << "kcfg_propertyNotify on" << widget->metaObject()->className()
                       << "is not of type ByteArray";
        } else {
            return prop.toByteArray();
        }
    }
    return QByteArray();
}

void KConfigDialogManager::setProperty(QWidget *w, const QVariant &v)
{
    if (d->allExclusiveGroupBoxes.contains(w)) {
        const QList<QAbstractButton *> buttons = w->findChildren<QAbstractButton *>();
        if (v.toInt() < buttons.count()) {
            buttons[v.toInt()]->setChecked(true);
        }
        return;
    }

    QByteArray userproperty = getCustomProperty(w);
    if (userproperty.isEmpty()) {
        userproperty = getUserProperty(w);
    }
    if (userproperty.isEmpty()) {
        QComboBox *cb = qobject_cast<QComboBox *>(w);
        if (cb) {
            if (cb->isEditable()) {
                int i = cb->findText(v.toString());
                if (i != -1) {
                    cb->setCurrentIndex(i);
                } else {
                    cb->setEditText(v.toString());
                }
            } else {
                cb->setCurrentIndex(v.toInt());
            }
            return;
        }
    }
    if (userproperty.isEmpty()) {
        qCWarning(KCONFIG_WIDGETS_LOG) << w->metaObject()->className() << " widget not handled!";
        return;
    }

    w->setProperty(userproperty, v);
}

QVariant KConfigDialogManager::property(QWidget *w) const
{
    if (d->allExclusiveGroupBoxes.contains(w)) {
        const QList<QAbstractButton *> buttons = w->findChildren<QAbstractButton *>();
        for (int i = 0; i < buttons.count(); ++i) {
            if (buttons[i]->isChecked())
                return i;
        }
        return -1;
    }

    QByteArray userproperty = getCustomProperty(w);
    if (userproperty.isEmpty()) {
        userproperty = getUserProperty(w);
    }
    if (userproperty.isEmpty()) {
        QComboBox *cb = qobject_cast<QComboBox *>(w);
        if (cb) {
            if (cb->isEditable()) {
                return QVariant(cb->currentText());
            } else {
                return QVariant(cb->currentIndex());
            }
        }
    }
    if (userproperty.isEmpty()) {
        qCWarning(KCONFIG_WIDGETS_LOG) << w->metaObject()->className() << " widget not handled!";
        return QVariant();
    }

    return w->property(userproperty);
}

bool KConfigDialogManager::hasChanged() const
{
    for (auto [key, widget] : QHashAdapter(d->knownWidget))
    {
        KConfigSkeletonItem *item = d->m_conf->findItem(key);
        if (!item) {
            qCWarning(KCONFIG_WIDGETS_LOG) << "The setting '" << key << "' has disappeared!";
            continue;
        }

        if (!item->isEqual(property(widget))) {
            // qCDebug(KCONFIG_WIDGETS_LOG) << "Widget for '" << it.key() << "' has changed.";
            return true;
        }
    }
    return false;
}

bool KConfigDialogManager::isDefault() const
{
    bool bUseDefaults = d->m_conf->useDefaults(true);
    bool result = !hasChanged();
    d->m_conf->useDefaults(bUseDefaults);
    return result;
}

