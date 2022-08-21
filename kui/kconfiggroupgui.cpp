/*
   This file is part of the KDE libraries
   Copyright (c) 2007 Thiago Macieira <thiago@kde.org>

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

#include <config/kconfiggroup.h>

#include <QMutableListIterator>
#include <QColor>
#include <QDebug>
#include <QFont>

#include <config/kconfiggroup_p.h>

/**
 * Try to read a GUI type from config group @p cg at key @p key.
 * @p input is the default value and also indicates the type to be read.
 * @p output is to be set with the value that has been read.
 *
 * @returns true if something was handled (even if output was set to clear or default)
 *          or false if nothing was handled (e.g., Core type)
 */
static bool readEntryGui(const QByteArray &data, const char *key, const QVariant &input,
                         QVariant &output)
{
    const auto errString = [&]() {
        return QStringLiteral("\"%1\" - conversion from \"%3\" to %2 failed")
                              .arg(QLatin1String(key),
                                   QLatin1String(QVariant::typeToName(input.type())),
                                   QLatin1String(data.constData()) );
    };

    // set in case of failure
    output = input;

    switch (static_cast<QMetaType::Type>(input.type())) {
    case QMetaType::QColor: {
        if (data.isEmpty() || data == "invalid") {
            output = QColor();  // return what was stored
            return true;
        } else if (data.at(0) == '#') {
            QColor col;
            col.setNamedColor(QString::fromUtf8(data.constData(), data.length()));
            output = col;
            return true;
        } else if (!data.contains(',')) {
            QColor col;
            col.setNamedColor(QString::fromUtf8(data.constData(), data.length()));
            if (!col.isValid()) {
                qCritical() << qPrintable(errString());
            }
            output = col;
            return true;
        } else {
            const QList<QByteArray> list = data.split(',');
            const int count = list.count();

            if (count != 3 && count != 4) {
                const QString formatError = QStringLiteral(" (wrong format: expected '%1' items, read '%2')");
                qCritical() << qPrintable(errString()) << qPrintable(formatError.arg(QStringLiteral("3' or '4")).arg(count));
                return true;    // return default
            }

            int temp[4];
            // bounds check components
            for (int i = 0; i < count; i++) {
                bool ok;
                const int j = temp[i] = list.at(i).toInt(&ok);
                if (!ok) { // failed to convert to int
                    qCritical() << qPrintable(errString()) << " (integer conversion failed)";
                    return true; // return default
                }
                if (j < 0 || j > 255) {
                    static const char *const components[] = {
                        "red", "green", "blue", "alpha"
                    };
                    const QString boundsError = QStringLiteral(" (bounds error: %1 component %2)");
                    qCritical() << qPrintable(errString())
                                << qPrintable(boundsError.arg(QLatin1String(components[i])).arg(j < 0 ? QStringLiteral("< 0") : QStringLiteral("> 255")));
                    return true; // return default
                }
            }
            QColor aColor(temp[0], temp[1], temp[2]);
            if (count == 4) {
                aColor.setAlpha(temp[3]);
            }

            if (aColor.isValid()) {
                output = aColor;
            } else {
                qCritical() << qPrintable(errString());
            }
            return true;
        }
    }

    case QMetaType::QFont: {
        QVariant tmp = QString::fromUtf8(data.constData(), data.length());
        if (tmp.convert(QMetaType::QFont)) {
            output = tmp;
        } else {
            qCritical() << qPrintable(errString());
        }
        return true;
    }
    case QMetaType::QPixmap:
    case QMetaType::QImage:
    case QMetaType::QBrush:
    case QMetaType::QPalette:
    case QMetaType::QIcon:
    case QMetaType::QRegion:
    case QMetaType::QBitmap:
    case QMetaType::QCursor:
    case QMetaType::QSizePolicy:
    case QMetaType::QPen:
    // we may want to handle these in the future

    default:
        break;
    }

    return false;               // not handled
}

/**
 * Try to write a GUI type @p prop to config group @p cg at key @p key.
 *
 * @returns true if something was handled (even if an empty value was written)
 *          or false if nothing was handled (e.g., Core type)
 */
static bool writeEntryGui(KConfigGroup *cg, const char *key, const QVariant &prop,
                          KConfigGroup::WriteConfigFlags pFlags)
{
    switch (static_cast<QMetaType::Type>(prop.type())) {
    case QMetaType::QColor: {
        const QColor rColor = prop.value<QColor>();

        if (!rColor.isValid()) {
            cg->writeEntry(key, "invalid", pFlags);
            return true;
        }

        QList<int> list;
        list.insert(0, rColor.red());
        list.insert(1, rColor.green());
        list.insert(2, rColor.blue());
        if (rColor.alpha() != 255) {
            list.insert(3, rColor.alpha());
        }

        cg->writeEntry(key, list, pFlags);
        return true;
    }
    case QMetaType::QFont:
        cg->writeEntry(key, prop.toString().toUtf8(), pFlags);
        return true;

    case QMetaType::QPixmap:
    case QMetaType::QImage:
    case QMetaType::QBrush:
    case QMetaType::QPalette:
    case QMetaType::QIcon:
    case QMetaType::QRegion:
    case QMetaType::QBitmap:
    case QMetaType::QCursor:
    case QMetaType::QSizePolicy:
    case QMetaType::QPen:
        // we may want to handle one of these in the future
        break;

    default:
        break;
    }

    return false;
}

static int initKConfigGroupGui()
{
    _kde_internal_KConfigGroupGui.readEntryGui = readEntryGui;
    _kde_internal_KConfigGroupGui.writeEntryGui = writeEntryGui;
    return 42;                  // because 42 is nicer than 1 or 0
}

#ifdef Q_CONSTRUCTOR_FUNCTION
Q_CONSTRUCTOR_FUNCTION(initKConfigGroupGui)
#else
static int dummyKConfigGroupGui = initKConfigGroupGui();
#endif
