/*  This file is part of the KDE libraries
    Copyright (C) 2007, 2013 Chusslove Illich <caslav.ilic@gmx.net>

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

#include <QHash>
#include <QSet>
#include <QRegularExpression>
#include <QStack>
#include <QXmlStreamReader>
#include <QStringList>
#include <QPair>
#include <QDir>

#include <kuitmarkup.h>
#include <kuitmarkup_p.h>
#include <KLocalizedString>

#include "ki18n_logging_kuit.h"
#include "qregularexpression.h"

#define QL1S(x) QLatin1String(x)
#define QSL(x) QStringLiteral(x)
#define QL1C(x) QLatin1Char(x)

QString Kuit::escape(const QString &text)
{
    int tlen = text.length();
    QString ntext;
    ntext.reserve(tlen);
    for (int i = 0; i < tlen; ++i) {
        QChar c = text[i];
        if (c == QL1C('&')) {
            ntext += QStringLiteral("&amp;");
        } else if (c == QL1C('<')) {
            ntext += QStringLiteral("&lt;");
        } else if (c == QL1C('>')) {
            ntext += QStringLiteral("&gt;");
        } else if (c == QL1C('\'')) {
            ntext += QStringLiteral("&apos;");
        } else if (c == QL1C('"')) {
            ntext += QStringLiteral("&quot;");
        } else {
            ntext += c;
        }
    }

    return ntext;
}

// Truncates the string, for output of long messages.
// (But don't truncate too much otherwise it's impossible to determine
// which message is faulty if many messages have the same beginning).
static QString shorten(const QString &str)
{
    const int maxlen = 80;
    if (str.length() <= maxlen) {
        return str;
    } else {
        return str.left(maxlen) + QSL("...");
    }
}
static int IndexIn(const QRegularExpression& regex, const QString& text, int start = 0)
{
    auto match = regex.match(text);
    return match.hasMatch()
        ? match.capturedStart()
        : -1;
}

static void parseUiMarker(const QString &context_,
                          QString &roleName,
                          QString &cueName,
                          QString &formatName)
{
    // UI marker is in the form @role:cue/format,
    // and must start just after any leading whitespace in the context string.
    QString context = context_.trimmed();
    if (context.startsWith(QL1C('@'))) { // found UI marker
        static QRegularExpression staticWsRx(QStringLiteral("\\s"));
        QRegularExpression wsRx = staticWsRx; // QRegExp not thread-safe
        context = context.mid(1, IndexIn(wsRx, context) - 1);

        // Possible format.
        int pfmt = context.indexOf(QL1C('/'));
        if (pfmt >= 0) {
            formatName = context.mid(pfmt + 1);
            context.truncate(pfmt);
        }

        // Possible subcue.
        int pcue = context.indexOf(QL1C(':'));
        if (pcue >= 0) {
            cueName = context.mid(pcue + 1);
            context.truncate(pcue);
        }

        // Role.
        roleName = context;
    }
    // Names remain untouched if marker was not found, which is fine.

    // Normalize names.
    roleName = roleName.trimmed().toLower();
    cueName = cueName.trimmed().toLower();
    formatName = formatName.trimmed().toLower();
}

// Custom entity resolver for QXmlStreamReader.
class KuitEntityResolver : public QXmlStreamEntityResolver
{
public:

    void setEntities(const QHash<QString, QString> &entities)
    {
        entityMap = entities;
    }

    QString resolveUndeclaredEntity(const QString &name) override
    {
        QString value = entityMap.value(name);
        // This will return empty string if the entity name is not known,
        // which will make QXmlStreamReader signal unknown entity error.
        return value;
    }

private:

    QHash<QString, QString> entityMap;
};

namespace Kuit
{

enum Role { // UI marker roles
    UndefinedRole,
    ActionRole, TitleRole, OptionRole, LabelRole, ItemRole, InfoRole
};

enum Cue { // UI marker subcues
    UndefinedCue,
    ButtonCue, InmenuCue, IntoolbarCue,
    WindowCue, MenuCue, TabCue, GroupCue, ColumnCue, RowCue,
    SliderCue, SpinboxCue, ListboxCue, TextboxCue, ChooserCue,
    CheckCue, RadioCue,
    InlistboxCue, IntableCue, InrangeCue, IntextCue, ValuesuffixCue,
    TooltipCue, WhatsthisCue, PlaceholderCue, StatusCue, ProgressCue,
    TipofthedayCue, CreditCue, ShellCue
};

}

class KuitStaticData
{
public:

    QHash<QString, QString> xmlEntities;
    QHash<QString, QString> xmlEntitiesInverse;
    KuitEntityResolver xmlEntityResolver;

    QHash<QString, Kuit::Role> rolesByName;
    QHash<QString, Kuit::Cue> cuesByName;
    QHash<QString, Kuit::VisualFormat> formatsByName;
    QHash<Kuit::VisualFormat, QString> namesByFormat;
    QHash<Kuit::Role, QSet<Kuit::Cue> > knownRoleCues;

    QHash<Kuit::VisualFormat, KLocalizedString> comboKeyDelim;
    QHash<Kuit::VisualFormat, KLocalizedString> guiPathDelim;
    QHash<QString, KLocalizedString> keyNames;

    QHash<QByteArray, KuitSetup *> domainSetups;

    KuitStaticData();
    ~KuitStaticData();

    KuitStaticData(const KuitStaticData &) = delete;
    KuitStaticData &operator=(const KuitStaticData &) = delete;

    void setXmlEntityData();

    void setUiMarkerData();

    void setTextTransformData();
    QString toKeyCombo(const QStringList &languages,
                       const QString &shstr, Kuit::VisualFormat format);
    QString toInterfacePath(const QStringList &languages,
                            const QString &inpstr, Kuit::VisualFormat format);
};

KuitStaticData::KuitStaticData()
{
    setXmlEntityData();
    setUiMarkerData();
    setTextTransformData();
}

KuitStaticData::~KuitStaticData()
{
    qDeleteAll(domainSetups);
}


void KuitStaticData::setXmlEntityData()
{
    QString LT = QStringLiteral("lt");
    QString GT = QStringLiteral("gt");
    QString AMP = QStringLiteral("amp");
    QString APOS = QStringLiteral("apos");
    QString QUOT = QStringLiteral("quot");

    // Default XML entities, direct and inverse mapping.
    xmlEntities[LT] = QString(QL1C('<'));
    xmlEntities[GT] = QString(QL1C('>'));
    xmlEntities[AMP] = QString(QL1C('&'));
    xmlEntities[APOS] = QString(QL1C('\''));
    xmlEntities[QUOT] = QString(QL1C('"'));
    xmlEntitiesInverse[QString(QL1C('<'))] = LT;
    xmlEntitiesInverse[QString(QL1C('>'))] = GT;
    xmlEntitiesInverse[QString(QL1C('&'))] = AMP;
    xmlEntitiesInverse[QString(QL1C('\''))] = APOS;
    xmlEntitiesInverse[QString(QL1C('"'))] = QUOT;

    // Custom XML entities.
    xmlEntities[QStringLiteral("nbsp")] = QString(QChar(0xa0));

    xmlEntityResolver.setEntities(xmlEntities);
}

void KuitStaticData::setUiMarkerData()
{
    using namespace Kuit;

    // Role names and their available subcues.
#undef SET_ROLE
#define SET_ROLE(role, name, cues) do { \
        rolesByName[name] = role; \
        knownRoleCues[role] << cues; \
    } while (0)
    SET_ROLE(ActionRole, QStringLiteral("action"),
             ButtonCue << InmenuCue << IntoolbarCue);
    SET_ROLE(TitleRole,  QStringLiteral("title"),
             WindowCue << MenuCue << TabCue << GroupCue
             << ColumnCue << RowCue);
    SET_ROLE(LabelRole,  QStringLiteral("label"),
             SliderCue << SpinboxCue << ListboxCue << TextboxCue
             << ChooserCue);
    SET_ROLE(OptionRole, QStringLiteral("option"),
             CheckCue << RadioCue);
    SET_ROLE(ItemRole,   QStringLiteral("item"),
             InmenuCue << InlistboxCue << IntableCue << InrangeCue
             << IntextCue << ValuesuffixCue);
    SET_ROLE(InfoRole,   QStringLiteral("info"),
             TooltipCue << WhatsthisCue << PlaceholderCue << StatusCue << ProgressCue
             << TipofthedayCue << CreditCue << ShellCue);

    // Cue names.
#undef SET_CUE
#define SET_CUE(cue, name) do { \
        cuesByName[name] = cue; \
    } while (0)
    SET_CUE(ButtonCue, QStringLiteral("button"));
    SET_CUE(InmenuCue, QStringLiteral("inmenu"));
    SET_CUE(IntoolbarCue, QStringLiteral("intoolbar"));
    SET_CUE(WindowCue, QStringLiteral("window"));
    SET_CUE(MenuCue, QStringLiteral("menu"));
    SET_CUE(TabCue, QStringLiteral("tab"));
    SET_CUE(GroupCue, QStringLiteral("group"));
    SET_CUE(ColumnCue, QStringLiteral("column"));
    SET_CUE(RowCue, QStringLiteral("row"));
    SET_CUE(SliderCue, QStringLiteral("slider"));
    SET_CUE(SpinboxCue, QStringLiteral("spinbox"));
    SET_CUE(ListboxCue, QStringLiteral("listbox"));
    SET_CUE(TextboxCue, QStringLiteral("textbox"));
    SET_CUE(ChooserCue, QStringLiteral("chooser"));
    SET_CUE(CheckCue, QStringLiteral("check"));
    SET_CUE(RadioCue, QStringLiteral("radio"));
    SET_CUE(InlistboxCue, QStringLiteral("inlistbox"));
    SET_CUE(IntableCue, QStringLiteral("intable"));
    SET_CUE(InrangeCue, QStringLiteral("inrange"));
    SET_CUE(IntextCue, QStringLiteral("intext"));
    SET_CUE(ValuesuffixCue, QStringLiteral("valuesuffix"));
    SET_CUE(TooltipCue, QStringLiteral("tooltip"));
    SET_CUE(WhatsthisCue, QStringLiteral("whatsthis"));
    SET_CUE(PlaceholderCue, QStringLiteral("placeholder"));
    SET_CUE(StatusCue, QStringLiteral("status"));
    SET_CUE(ProgressCue, QStringLiteral("progress"));
    SET_CUE(TipofthedayCue, QStringLiteral("tipoftheday"));
    SET_CUE(CreditCue, QStringLiteral("credit"));
    SET_CUE(ShellCue, QStringLiteral("shell"));

    // Format names.
#undef SET_FORMAT
#define SET_FORMAT(format, name) do { \
        formatsByName[name] = format; \
        namesByFormat[format] = name; \
    } while (0)
    SET_FORMAT(UndefinedFormat, QStringLiteral("undefined"));
    SET_FORMAT(PlainText, QStringLiteral("plain"));
    SET_FORMAT(RichText, QStringLiteral("rich"));
    SET_FORMAT(TermText, QStringLiteral("term"));
}

void KuitStaticData::setTextTransformData()
{
    // i18n: Decide which string is used to delimit keys in a keyboard
    // shortcut (e.g. + in Ctrl+Alt+Tab) in plain text.
    comboKeyDelim[Kuit::PlainText] = ki18nc("shortcut-key-delimiter/plain", "+");
    comboKeyDelim[Kuit::TermText] = comboKeyDelim[Kuit::PlainText];
    // i18n: Decide which string is used to delimit keys in a keyboard
    // shortcut (e.g. + in Ctrl+Alt+Tab) in rich text.
    comboKeyDelim[Kuit::RichText] = ki18nc("shortcut-key-delimiter/rich", "+");

    // i18n: Decide which string is used to delimit elements in a GUI path
    // (e.g. -> in "Go to Settings->Advanced->Core tab.") in plain text.
    guiPathDelim[Kuit::PlainText] = ki18nc("gui-path-delimiter/plain", "→");
    guiPathDelim[Kuit::TermText] = guiPathDelim[Kuit::PlainText];
    // i18n: Decide which string is used to delimit elements in a GUI path
    // (e.g. -> in "Go to Settings->Advanced->Core tab.") in rich text.
    guiPathDelim[Kuit::RichText] = ki18nc("gui-path-delimiter/rich", "→");
    // NOTE: The '→' glyph seems to be available in all widespread fonts.

    // Collect keyboard key names.
#undef SET_KEYNAME
#define SET_KEYNAME(rawname) do { \
        /* Normalize key, trim and all lower-case. */ \
        QString normname = QStringLiteral(rawname).trimmed().toLower(); \
        keyNames[normname] = ki18nc("keyboard-key-name", rawname); \
    } while (0)
    // Now we need I18NC_NOOP that does remove context.
#undef I18NC_NOOP
#define I18NC_NOOP(ctxt, msg) msg
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Alt"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "AltGr"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Backspace"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "CapsLock"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Control"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Ctrl"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Del"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Delete"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Down"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "End"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Enter"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Esc"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Escape"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Home"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Hyper"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Ins"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Insert"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Left"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Menu"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Meta"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "NumLock"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "PageDown"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "PageUp"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "PgDown"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "PgUp"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "PauseBreak"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "PrintScreen"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "PrtScr"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Return"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Right"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "ScrollLock"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Shift"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Space"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Super"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "SysReq"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Tab"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Up"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "Win"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F1"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F2"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F3"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F4"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F5"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F6"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F7"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F8"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F9"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F10"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F11"));
    SET_KEYNAME(I18NC_NOOP("keyboard-key-name", "F12"));
    // TODO: Add rest of the key names?
}

QString KuitStaticData::toKeyCombo(const QStringList &languages,
                                   const QString &shstr,
                                   Kuit::VisualFormat format)
{
    // Take '+' or '-' as input shortcut delimiter,
    // whichever is first encountered.
    static QRegularExpression staticDelimRx(QStringLiteral("[+-]"));
    QRegularExpression delimRx = staticDelimRx; // QRegExp not thread-safe

    int p = IndexIn(delimRx, shstr); // find delimiter
    QStringList keys;
    if (p < 0) { // single-key shortcut, no delimiter found
        keys.append(shstr);
    } else { // multi-key shortcut
        QChar oldDelim = shstr[p];
        keys = shstr.split(oldDelim, Qt::SkipEmptyParts);
    }

    for (int i = 0; i < keys.size(); ++i) {
        // Normalize key, trim and all lower-case.
        QString nkey = keys[i].trimmed().toLower();
        keys[i] = keyNames.contains(nkey) ? keyNames[nkey].toString(languages)
                  : keys[i].trimmed();
    }
    QString delim = comboKeyDelim.value(format).toString(languages);
    return keys.join(delim);
}

QString KuitStaticData::toInterfacePath(const QStringList &languages,
                                        const QString &inpstr,
                                        Kuit::VisualFormat format)
{
    // Take '/', '|' or "->" as input path delimiter,
    // whichever is first encountered.
    static QRegularExpression staticDelimRx(QStringLiteral("\\||->"));
    auto delimRx = staticDelimRx; // QRegExp not thread-safe

    int p = IndexIn(delimRx, inpstr); // find delimiter
    if (p < 0) { // single-element path, no delimiter found
        return inpstr;
    } else { // multi-element path
        auto match = delimRx.match(inpstr);
        QString oldDelim = match.capturedTexts().at(0);
        QStringList guiels = inpstr.split(oldDelim, Qt::SkipEmptyParts);
        QString delim = guiPathDelim.value(format).toString(languages);
        return guiels.join(delim);
    }
}

Q_GLOBAL_STATIC(KuitStaticData, staticData)

static QString attributeSetKey(const QStringList &attribNames_)
{
    QStringList attribNames = attribNames_;
    std::sort(attribNames.begin(), attribNames.end());
    QString key = QL1C('[') + attribNames.join(QL1C(' ')) + QL1C(']');
    return key;
}

class KuitTag
{
public:

    QString name;
    Kuit::TagClass type;
    QSet<QString> knownAttribs;
    QHash<QString, QHash<Kuit::VisualFormat, QStringList> > attributeOrders;
    QHash<QString, QHash<Kuit::VisualFormat, KLocalizedString> > patterns;
    QHash<QString, QHash<Kuit::VisualFormat, Kuit::TagFormatter> > formatters;
    int leadingNewlines;
    QString format(const QStringList &languages,
                   const QHash<QString, QString> &attributes,
                   const QString &text,
                   const QStringList &tagPath,
                   Kuit::VisualFormat format) const;
};

QString KuitTag::format(const QStringList &languages,
                        const QHash<QString, QString> &attributes,
                        const QString &text,
                        const QStringList &tagPath,
                        Kuit::VisualFormat format) const
{
    KuitStaticData *s = staticData();
    QString formattedText = text;
    QString attribKey = attributeSetKey(attributes.keys());
    const QHash<Kuit::VisualFormat, KLocalizedString> pattern = patterns.value(attribKey);
    if (pattern.contains(format)) {
        QString modText;
        Kuit::TagFormatter formatter = formatters.value(attribKey).value(format);
        if (formatter != nullptr) {
            modText = formatter(languages, name, attributes, text, tagPath, format);
        } else {
            modText = text;
        }
        KLocalizedString aggText = pattern.value(format);
        // line below is first-aid fix.for e.g. <emphasis strong='true'>.
        // TODO: proper handling of boolean attributes still needed
        aggText = aggText.relaxSubs();
        if (!aggText.isEmpty()) {
            aggText = aggText.subs(modText);
            const QStringList attributeOrder = attributeOrders.value(attribKey).value(format);
            for (const QString &attribName : attributeOrder) {
                aggText = aggText.subs(attributes.value(attribName));
            }
            formattedText = aggText.ignoreMarkup().toString(languages);
        } else {
            formattedText = modText;
        }
    } else if (patterns.contains(attribKey)) {
        qCWarning(KI18N_KUIT) << QStringLiteral(
                               "Undefined visual format for tag <%1> and attribute combination %2: %3.")
                               .arg(name, attribKey, s->namesByFormat.value(format));
    } else {
        qCWarning(KI18N_KUIT) << QStringLiteral(
                               "Undefined attribute combination for tag <%1>: %2.")
                               .arg(name, attribKey);
    }
    return formattedText;
}

KuitSetup &Kuit::setupForDomain(const QByteArray& domain)
{
    KuitStaticData *s = staticData();
    KuitSetup *setup = s->domainSetups.value(domain);
    if (!setup) {
        setup = new KuitSetup(domain);
        s->domainSetups.insert(domain, setup);
    }
    return *setup;
}

KuitSetup &Kuit::setupForDomain(const char *domain)
{
    return setupForDomain(QByteArray(domain));
}

class KuitSetupPrivate
{
public:

    void setTagPattern(const QString &tagName,
                       const QStringList &attribNames,
                       Kuit::VisualFormat format,
                       const KLocalizedString &pattern,
                       Kuit::TagFormatter formatter,
                       int leadingNewlines);

    void setTagClass(const QString &tagName, Kuit::TagClass aClass);

    void setFormatForMarker(const QString &marker, Kuit::VisualFormat format);

    void setDefaultMarkup();
    void setDefaultFormats();

    QByteArray domain;
    QHash<QString, KuitTag> knownTags;
    QHash<Kuit::Role, QHash<Kuit::Cue, Kuit::VisualFormat> > formatsByRoleCue;
};

void KuitSetupPrivate::setTagPattern(const QString &tagName,
                                     const QStringList &attribNames_,
                                     Kuit::VisualFormat format,
                                     const KLocalizedString &pattern,
                                     Kuit::TagFormatter formatter,
                                     int leadingNewlines_)
{
    bool isNewTag = knownTags.contains(tagName);
    KuitTag &tag = knownTags[tagName];
    if (isNewTag) {
        tag.name = tagName;
        tag.type = Kuit::PhraseTag;
    }
    QStringList attribNames = attribNames_;
    attribNames.removeAll(QString());
    for (const QString &attribName : qAsConst(attribNames)) {
        tag.knownAttribs.insert(attribName);
    }
    QString attribKey = attributeSetKey(attribNames);
    tag.attributeOrders[attribKey][format] = attribNames;
    tag.patterns[attribKey][format] = pattern;
    tag.formatters[attribKey][format] = formatter;
    tag.leadingNewlines = leadingNewlines_;
}

void KuitSetupPrivate::setTagClass(const QString &tagName,
                                   Kuit::TagClass aClass)
{
    bool isNewTag = knownTags.contains(tagName);
    KuitTag &tag = knownTags[tagName];
    if (isNewTag) {
        tag.name = tagName;
    }
    tag.type = aClass;
}

void KuitSetupPrivate::setFormatForMarker(const QString &marker,
        Kuit::VisualFormat format)
{
    KuitStaticData *s = staticData();

    QString roleName, cueName, formatName;
    parseUiMarker(marker, roleName, cueName, formatName);

    Kuit::Role role;
    if (s->rolesByName.contains(roleName)) {
        role = s->rolesByName.value(roleName);
    } else if (!roleName.isEmpty()) {
        qCWarning(KI18N_KUIT) << QStringLiteral(
                               "Unknown role '@%1' in UI marker {%2}, visual format not set.")
                               .arg(roleName, marker);
        return;
    } else {
        qCWarning(KI18N_KUIT) << QStringLiteral(
                               "Empty role in UI marker {%1}, visual format not set.")
                               .arg(marker);
        return;
    }

    Kuit::Cue cue;
    if (s->cuesByName.contains(cueName)) {
        cue = s->cuesByName.value(cueName);
        if (!s->knownRoleCues.value(role).contains(cue)) {
            qCWarning(KI18N_KUIT) << QStringLiteral(
                                   "Subcue ':%1' does not belong to role '@%2' in UI marker {%3}, visual format not set.")
                                   .arg(cueName, roleName, marker);
            return;
        }
    } else if (!cueName.isEmpty()) {
        qCWarning(KI18N_KUIT) << QStringLiteral(
                               "Unknown subcue ':%1' in UI marker {%2}, visual format not set.")
                               .arg(cueName, marker);
        return;
    } else {
        cue = Kuit::UndefinedCue;
    }

    formatsByRoleCue[role][cue] = format;
}

#define TAG_FORMATTER_ARGS \
    const QStringList &languages, \
    const QString &tagName, \
    const QHash<QString, QString> &attributes, \
    const QString &text, \
    const QStringList &tagPath, \
    Kuit::VisualFormat format

static QString tagFormatterFilename(TAG_FORMATTER_ARGS)
{
    Q_UNUSED(languages);
    Q_UNUSED(tagName);
    Q_UNUSED(attributes);
    Q_UNUSED(tagPath);
#ifdef Q_OS_WIN
    // with rich text the path can include <foo>...</foo> which will be replaced by <foo>...<\foo> on Windows!
    // the same problem also happens for tags such as <br/> -> <br\>
    if (format == Kuit::RichText) {
        // replace all occurrences of "</" or "/>" to make sure toNativeSeparators() doesn't destroy XML markup
        const auto KUIT_CLOSE_XML_REPLACEMENT = QStringLiteral("__kuit_close_xml_tag__");
        const auto KUIT_NOTEXT_XML_REPLACEMENT = QStringLiteral("__kuit_notext_xml_tag__");

        QString result = text;
        result.replace(QStringLiteral("</"), KUIT_CLOSE_XML_REPLACEMENT);
        result.replace(QStringLiteral("/>"), KUIT_NOTEXT_XML_REPLACEMENT);
        result = QDir::toNativeSeparators(result);
        result.replace(KUIT_CLOSE_XML_REPLACEMENT, QStringLiteral("</"));
        result.replace(KUIT_NOTEXT_XML_REPLACEMENT, QStringLiteral("/>"));
        return result;
    }
#else
    Q_UNUSED(format);
#endif
    return QDir::toNativeSeparators(text);
}

static QString tagFormatterShortcut(TAG_FORMATTER_ARGS)
{
    Q_UNUSED(tagName);
    Q_UNUSED(attributes);
    Q_UNUSED(tagPath);
    KuitStaticData *s = staticData();
    return s->toKeyCombo(languages, text, format);
}

static QString tagFormatterInterface(TAG_FORMATTER_ARGS)
{
    Q_UNUSED(tagName);
    Q_UNUSED(attributes);
    Q_UNUSED(tagPath);
    KuitStaticData *s = staticData();
    return s->toInterfacePath(languages, text, format);
}

void KuitSetupPrivate::setDefaultMarkup()
{
    using namespace Kuit;

    const QString INTERNAL_TOP_TAG_NAME = QStringLiteral("__kuit_internal_top__");
    const QString TITLE = QStringLiteral("title");
    const QString EMPHASIS = QStringLiteral("emphasis");
    const QString COMMAND = QStringLiteral("command");
    const QString WARNING = QStringLiteral("warning");
    const QString LINK = QStringLiteral("link");
    const QString NOTE = QStringLiteral("note");

    // Macro to hide message from extraction.
#define HI18NC ki18nc

    // Macro to expedite setting the patterns.
#undef SET_PATTERN
#define SET_PATTERN(tagName, attribNames_, format, pattern, formatter, leadNl) \
    do { \
        QStringList attribNames; \
        attribNames << attribNames_; \
        setTagPattern(tagName, attribNames, format, pattern, formatter, leadNl); \
        /* Make TermText pattern same as PlainText if not explicitly given. */ \
        KuitTag &tag = knownTags[tagName]; \
        QString attribKey = attributeSetKey(attribNames); \
        if (format == PlainText && !tag.patterns[attribKey].contains(TermText)) { \
            setTagPattern(tagName, attribNames, TermText, pattern, formatter, leadNl); \
        } \
    } while (0)

    // NOTE: The following "i18n:" comments are oddly placed in order that
    // xgettext extracts them properly.

    // -------> Internal top tag
    setTagClass(INTERNAL_TOP_TAG_NAME, StructTag);
    setTagClass(INTERNAL_TOP_TAG_NAME, StructTag);
    SET_PATTERN(INTERNAL_TOP_TAG_NAME, QString(), PlainText,
                HI18NC("tag-format-pattern <> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1"),
                nullptr, 0);
    SET_PATTERN(INTERNAL_TOP_TAG_NAME, QString(), RichText,
                HI18NC("tag-format-pattern <> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1"),
                nullptr, 0);

    // -------> Title
    setTagClass(TITLE, StructTag);
    SET_PATTERN(TITLE, QString(), PlainText,
                ki18nc("tag-format-pattern <title> plain",
                       // i18n: The messages with context "tag-format-pattern <tag ...> format"
                       // are KUIT patterns for formatting the text found inside KUIT tags.
                       // The format is either "plain" or "rich", and tells if the pattern
                       // is used for plain text or rich text (which can use HTML tags).
                       // You may be in general satisfied with the patterns as they are in the
                       // original. Some things you may consider changing:
                       // - the proper quotes, those used in msgid are English-standard
                       // - the <i> and <b> tags, does your language script work well with them?
                       "== %1 =="),
                nullptr, 2);
    SET_PATTERN(TITLE, QString(), RichText,
                ki18nc("tag-format-pattern <title> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<h2>%1</h2>"),
                nullptr, 2);

    // -------> Subtitle
    setTagClass(QSL("subtitle"), StructTag);
    SET_PATTERN(QSL("subtitle"), QString(), PlainText,
                ki18nc("tag-format-pattern <subtitle> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "~ %1 ~"),
                nullptr, 2);
    SET_PATTERN(QSL("subtitle"), QString(), RichText,
                ki18nc("tag-format-pattern <subtitle> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<h3>%1</h3>"),
                nullptr, 2);

    // -------> Para
    setTagClass(QSL("para"), StructTag);
    SET_PATTERN(QSL("para"), QString(), PlainText,
                ki18nc("tag-format-pattern <para> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1"),
                nullptr, 2);
    SET_PATTERN(QSL("para"), QString(), RichText,
                ki18nc("tag-format-pattern <para> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<p>%1</p>"),
                nullptr, 2);

    // -------> List
    setTagClass(QSL("list"), StructTag);
    SET_PATTERN(QSL("list"), QString(), PlainText,
                ki18nc("tag-format-pattern <list> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1"),
                nullptr, 1);
    SET_PATTERN(QSL("list"), QString(), RichText,
                ki18nc("tag-format-pattern <list> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<ul>%1</ul>"),
                nullptr, 1);

    // -------> Item
    setTagClass(QSL("item"), StructTag);
    SET_PATTERN(QSL("item"), QString(), PlainText,
                ki18nc("tag-format-pattern <item> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "  * %1"),
                nullptr, 1);
    SET_PATTERN(QSL("item"), QString(), RichText,
                ki18nc("tag-format-pattern <item> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<li>%1</li>"),
                nullptr, 1);

    // -------> Note
    SET_PATTERN(NOTE, QString(), PlainText,
                ki18nc("tag-format-pattern <note> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "Note: %1"),
                nullptr, 0);
    SET_PATTERN(NOTE, QString(), RichText,
                ki18nc("tag-format-pattern <note> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<i>Note</i>: %1"),
                nullptr, 0);
    SET_PATTERN(NOTE, QSL("label"), PlainText,
                ki18nc("tag-format-pattern <note label=> plain\n"
                       "%1 is the text, %2 is the note label",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%2: %1"),
                nullptr, 0);
    SET_PATTERN(NOTE, QSL("label"), RichText,
                ki18nc("tag-format-pattern <note label=> rich\n"
                       "%1 is the text, %2 is the note label",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<i>%2</i>: %1"),
                nullptr, 0);

    // -------> Warning
    SET_PATTERN(WARNING, QString(), PlainText,
                ki18nc("tag-format-pattern <warning> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "WARNING: %1"),
                nullptr, 0);
    SET_PATTERN(WARNING, QString(), RichText,
                ki18nc("tag-format-pattern <warning> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<b>Warning</b>: %1"),
                nullptr, 0);
    SET_PATTERN(WARNING, QSL("label"), PlainText,
                ki18nc("tag-format-pattern <warning label=> plain\n"
                       "%1 is the text, %2 is the warning label",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%2: %1"),
                nullptr, 0);
    SET_PATTERN(WARNING, QSL("label"), RichText,
                ki18nc("tag-format-pattern <warning label=> rich\n"
                       "%1 is the text, %2 is the warning label",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<b>%2</b>: %1"),
                nullptr, 0);

    // -------> Link
    SET_PATTERN(LINK, QString(), PlainText,
                ki18nc("tag-format-pattern <link> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1"),
                nullptr, 0);
    SET_PATTERN(LINK, QString(), RichText,
                ki18nc("tag-format-pattern <link> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<a href=\"%1\">%1</a>"),
                nullptr, 0);
    SET_PATTERN(LINK, QSL("url"), PlainText,
                ki18nc("tag-format-pattern <link url=> plain\n"
                       "%1 is the descriptive text, %2 is the URL",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1 (%2)"),
                nullptr, 0);
    SET_PATTERN(LINK, QSL("url"), RichText,
                ki18nc("tag-format-pattern <link url=> rich\n"
                       "%1 is the descriptive text, %2 is the URL",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<a href=\"%2\">%1</a>"),
                nullptr, 0);

    // -------> Filename
    SET_PATTERN(QSL("filename"), QString(), PlainText,
                ki18nc("tag-format-pattern <filename> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "‘%1’"),
                tagFormatterFilename, 0);
    SET_PATTERN(QSL("filename"), QString(), RichText,
                ki18nc("tag-format-pattern <filename> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<tt>%1</tt>"),
                tagFormatterFilename, 0);

    // -------> Application
    SET_PATTERN(QSL("application"), QString(), PlainText,
                ki18nc("tag-format-pattern <application> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1"),
                nullptr, 0);
    SET_PATTERN(QSL("application"), QString(), RichText,
                ki18nc("tag-format-pattern <application> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1"),
                nullptr, 0);

    // -------> Command
    SET_PATTERN(COMMAND, QString(), PlainText,
                ki18nc("tag-format-pattern <command> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1"),
                nullptr, 0);
    SET_PATTERN(COMMAND, QString(), RichText,
                ki18nc("tag-format-pattern <command> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<tt>%1</tt>"),
                nullptr, 0);
    SET_PATTERN(COMMAND, QSL("section"), PlainText,
                ki18nc("tag-format-pattern <command section=> plain\n"
                       "%1 is the command name, %2 is its man section",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1(%2)"),
                nullptr, 0);
    SET_PATTERN(COMMAND, QSL("section"), RichText,
                ki18nc("tag-format-pattern <command section=> rich\n"
                       "%1 is the command name, %2 is its man section",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<tt>%1(%2)</tt>"),
                nullptr, 0);

    // -------> Resource
    SET_PATTERN(QSL("resource"), QString(), PlainText,
                ki18nc("tag-format-pattern <resource> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "“%1”"),
                nullptr, 0);
    SET_PATTERN(QSL("resource"), QString(), RichText,
                ki18nc("tag-format-pattern <resource> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "“%1”"),
                nullptr, 0);

    // -------> Icode
    SET_PATTERN(QSL("icode"), QString(), PlainText,
                ki18nc("tag-format-pattern <icode> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "“%1”"),
                nullptr, 0);
    SET_PATTERN(QSL("icode"), QString(), RichText,
                ki18nc("tag-format-pattern <icode> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<tt>%1</tt>"),
                nullptr, 0);

    // -------> Bcode
    SET_PATTERN(QSL("bcode"), QString(), PlainText,
                ki18nc("tag-format-pattern <bcode> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "\n%1\n"),
                nullptr, 2);
    SET_PATTERN(QSL("bcode"), QString(), RichText,
                ki18nc("tag-format-pattern <bcode> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<pre>%1</pre>"),
                nullptr, 2);

    // -------> Shortcut
    SET_PATTERN(QSL("shortcut"), QString(), PlainText,
                ki18nc("tag-format-pattern <shortcut> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1"),
                tagFormatterShortcut, 0);
    SET_PATTERN(QSL("shortcut"), QString(), RichText,
                ki18nc("tag-format-pattern <shortcut> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<b>%1</b>"),
                tagFormatterShortcut, 0);

    // -------> Interface
    SET_PATTERN(QSL("interface"), QString(), PlainText,
                ki18nc("tag-format-pattern <interface> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "|%1|"),
                tagFormatterInterface, 0);
    SET_PATTERN(QSL("interface"), QString(), RichText,
                ki18nc("tag-format-pattern <interface> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<i>%1</i>"),
                tagFormatterInterface, 0);

    // -------> Emphasis
    SET_PATTERN(EMPHASIS, QString(), PlainText,
                ki18nc("tag-format-pattern <emphasis> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "*%1*"),
                nullptr, 0);
    SET_PATTERN(EMPHASIS, QString(), RichText,
                ki18nc("tag-format-pattern <emphasis> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<i>%1</i>"),
                nullptr, 0);
    SET_PATTERN(EMPHASIS, QSL("strong"), PlainText,
                ki18nc("tag-format-pattern <emphasis-strong> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "**%1**"),
                nullptr, 0);
    SET_PATTERN(EMPHASIS, QSL("strong"), RichText,
                ki18nc("tag-format-pattern <emphasis-strong> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<b>%1</b>"),
                nullptr, 0);

    // -------> Placeholder
    SET_PATTERN(QSL("placeholder"), QString(), PlainText,
                ki18nc("tag-format-pattern <placeholder> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "&lt;%1&gt;"),
                nullptr, 0);
    SET_PATTERN(QSL("placeholder"), QString(), RichText,
                ki18nc("tag-format-pattern <placeholder> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "&lt;<i>%1</i>&gt;"),
                nullptr, 0);

    // -------> Email
    SET_PATTERN(QSL("email"), QString(), PlainText,
                ki18nc("tag-format-pattern <email> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "&lt;%1&gt;"),
                nullptr, 0);
    SET_PATTERN(QSL("email"), QString(), RichText,
                ki18nc("tag-format-pattern <email> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "&lt;<a href=\"mailto:%1\">%1</a>&gt;"),
                nullptr, 0);
    SET_PATTERN(QSL("email"), QSL("address"), PlainText,
                ki18nc("tag-format-pattern <email address=> plain\n"
                       "%1 is name, %2 is address",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1 &lt;%2&gt;"),
                nullptr, 0);
    SET_PATTERN(QSL("email"), QSL("address"), RichText,
                ki18nc("tag-format-pattern <email address=> rich\n"
                       "%1 is name, %2 is address",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<a href=\"mailto:%2\">%1</a>"),
                nullptr, 0);

    // -------> Envar
    SET_PATTERN(QSL("envar"), QString(), PlainText,
                ki18nc("tag-format-pattern <envar> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "$%1"),
                nullptr, 0);
    SET_PATTERN(QSL("envar"), QString(), RichText,
                ki18nc("tag-format-pattern <envar> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<tt>$%1</tt>"),
                nullptr, 0);

    // -------> Message
    SET_PATTERN(QSL("message"), QString(), PlainText,
                ki18nc("tag-format-pattern <message> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "/%1/"),
                nullptr, 0);
    SET_PATTERN(QSL("message"), QString(), RichText,
                ki18nc("tag-format-pattern <message> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "<i>%1</i>"),
                nullptr, 0);

    // -------> Nl
    SET_PATTERN(QSL("nl"), QString(), PlainText,
                ki18nc("tag-format-pattern <nl> plain",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1\n"),
                nullptr, 0);
    SET_PATTERN(QSL("nl"), QString(), RichText,
                ki18nc("tag-format-pattern <nl> rich",
                       // i18n: KUIT pattern, see the comment to the first of these entries above.
                       "%1<br/>"),
                nullptr, 0);
}

void KuitSetupPrivate::setDefaultFormats()
{
    using namespace Kuit;

    // Setup formats by role.
    formatsByRoleCue[ActionRole][UndefinedCue] = PlainText;
    formatsByRoleCue[TitleRole][UndefinedCue] = PlainText;
    formatsByRoleCue[LabelRole][UndefinedCue] = PlainText;
    formatsByRoleCue[OptionRole][UndefinedCue] = PlainText;
    formatsByRoleCue[ItemRole][UndefinedCue] = PlainText;
    formatsByRoleCue[InfoRole][UndefinedCue] = RichText;

    // Setup override formats by subcue.
    formatsByRoleCue[InfoRole][StatusCue] = PlainText;
    formatsByRoleCue[InfoRole][ProgressCue] = PlainText;
    formatsByRoleCue[InfoRole][CreditCue] = PlainText;
    formatsByRoleCue[InfoRole][ShellCue] = TermText;
}

KuitSetup::KuitSetup(const QByteArray &domain)
    : d(new KuitSetupPrivate)
{
    d->domain = domain;
    d->setDefaultMarkup();
    d->setDefaultFormats();
}

KuitSetup::~KuitSetup()
{
    delete d;
}

void KuitSetup::setTagPattern(const QString &tagName,
                              const QStringList &attribNames,
                              Kuit::VisualFormat format,
                              const KLocalizedString &pattern,
                              Kuit::TagFormatter formatter,
                              int leadingNewlines)
{
    d->setTagPattern(tagName, attribNames, format, pattern, formatter,
                     leadingNewlines);
}

void KuitSetup::setTagClass(const QString &tagName, Kuit::TagClass aClass)
{
    d->setTagClass(tagName, aClass);
}

void KuitSetup::setFormatForMarker(const QString &marker,
                                   Kuit::VisualFormat format)
{
    d->setFormatForMarker(marker, format);
}

class KuitFormatterPrivate
{
public:

    KuitFormatterPrivate(const QString &language);

    QString format(const QByteArray &domain,
                   const QString &context, const QString &text,
                   Kuit::VisualFormat format) const;

    // Get metatranslation (formatting patterns, etc.)
    QString metaTr(const char *context, const char *text) const;

    // Set visual formatting patterns for text within tags.
    void setFormattingPatterns();

    // Set data used in transformation of text within tags.
    void setTextTransformData();

    // Determine visual format by parsing the UI marker in the context.
    static Kuit::VisualFormat formatFromUiMarker(const QString &context,
            const KuitSetup &setup);

    // Determine if text has block structure (multiple paragraphs, etc).
    static bool determineIsStructured(const QString &text,
                                      const KuitSetup &setup);

    // Format KUIT text into visual text.
    QString toVisualText(const QString &text,
                         Kuit::VisualFormat format,
                         const KuitSetup &setup) const;

    // Final touches to the formatted text.
    QString finalizeVisualText(const QString &ftext,
                               Kuit::VisualFormat format) const;

    // In case of markup errors, try to make result not look too bad.
    QString salvageMarkup(const QString &text,
                          Kuit::VisualFormat format,
                          const KuitSetup &setup) const;

    // Data for XML parsing state.
    class OpenEl
    {
    public:

        enum Handling { Proper, Ignored, Dropout };

        KuitTag tag;
        QString name;
        QHash<QString, QString> attributes;
        QString attribStr;
        Handling handling;
        QString formattedText;
        QStringList tagPath;
    };

    // Gather data about current element for the parse state.
    KuitFormatterPrivate::OpenEl parseOpenEl(const QXmlStreamReader &xml,
            const OpenEl &enclosingOel,
            const QString &text,
            const KuitSetup &setup) const;

    // Format text of the element.
    QString formatSubText(const QString &ptext, const OpenEl &oel,
                          Kuit::VisualFormat format,
                          const KuitSetup &setup) const;

    // Count number of newlines at start and at end of text.
    static void countWrappingNewlines(const QString &ptext,
                                      int &numle, int &numtr);

private:

    QString language;
    QStringList languageAsList;

    QHash<Kuit::VisualFormat, QString> comboKeyDelim;
    QHash<Kuit::VisualFormat, QString> guiPathDelim;

    QHash<QString, QString> keyNames;
};

KuitFormatterPrivate::KuitFormatterPrivate(const QString &language_)
    : language(language_)
{
}

QString KuitFormatterPrivate::format(const QByteArray &domain,
                                     const QString &context, const QString &text,
                                     Kuit::VisualFormat format) const
{
    const KuitSetup &setup = Kuit::setupForDomain(domain);

    // If format is undefined, determine it based on UI marker inside context.
    Kuit::VisualFormat resolvedFormat = format;
    if (resolvedFormat == Kuit::UndefinedFormat) {
        resolvedFormat = formatFromUiMarker(context, setup);
    }

    // Quick check: are there any tags at all?
    QString ftext;
    if (text.indexOf(QL1C('<')) < 0) {
        ftext = finalizeVisualText(text, resolvedFormat);
    } else {
        // Format the text.
        ftext = toVisualText(text, resolvedFormat, setup);
        if (ftext.isEmpty()) { // error while processing markup
            ftext = salvageMarkup(text, resolvedFormat, setup);
        }
    }
    return ftext;
}

Kuit::VisualFormat KuitFormatterPrivate::formatFromUiMarker(const QString &context,
        const KuitSetup &setup)
{
    KuitStaticData *s = staticData();

    QString roleName, cueName, formatName;
    parseUiMarker(context, roleName, cueName, formatName);

    // Set role from name.
    Kuit::Role role = s->rolesByName.value(roleName, Kuit::UndefinedRole);
    if (role == Kuit::UndefinedRole) { // unknown role
        if (!roleName.isEmpty()) {
            qCWarning(KI18N_KUIT) << QStringLiteral(
                                   "Unknown role '@%1' in UI marker in context {%2}.")
                                   .arg(roleName, shorten(context));
        }
    }

    // Set subcue from name.
    Kuit::Cue cue;
    if (role != Kuit::UndefinedRole) {
        cue = s->cuesByName.value(cueName, Kuit::UndefinedCue);
        if (cue != Kuit::UndefinedCue) { // known subcue
            if (!s->knownRoleCues.value(role).contains(cue)) {
                cue = Kuit::UndefinedCue;
                qCWarning(KI18N_KUIT) << QStringLiteral(
                                       "Subcue ':%1' does not belong to role '@%2' in UI marker in context {%3}.")
                                       .arg(cueName, roleName, shorten(context));
            }
        } else { // unknown or not given subcue
            if (!cueName.isEmpty()) {
                qCWarning(KI18N_KUIT) << QStringLiteral(
                                       "Unknown subcue ':%1' in UI marker in context {%2}.")
                                       .arg(cueName, shorten(context));
            }
        }
    } else {
        // Bad role, silently ignore the cue.
        cue = Kuit::UndefinedCue;
    }

    // Set format from name, or by derivation from contex/subcue.
    Kuit::VisualFormat format = s->formatsByName.value(formatName, Kuit::UndefinedFormat);
    if (format == Kuit::UndefinedFormat) { // unknown or not given format
        // Check first if there is a format defined for role/subcue
        // combination, then for role only, otherwise default to undefined.
        if (setup.d->formatsByRoleCue.contains(role)) {
            if (setup.d->formatsByRoleCue.value(role).contains(cue)) {
                format = setup.d->formatsByRoleCue.value(role).value(cue);
            } else {
                format = setup.d->formatsByRoleCue.value(role).value(Kuit::UndefinedCue);
            }
        }
        if (!formatName.isEmpty()) {
            qCWarning(KI18N_KUIT) << QStringLiteral(
                                   "Unknown format '/%1' in UI marker for message {%2}.")
                                   .arg(formatName, shorten(context));
        }
    }
    if (format == Kuit::UndefinedFormat) {
        format = Kuit::PlainText;
    }

    return format;
}

bool KuitFormatterPrivate::determineIsStructured(const QString &text,
        const KuitSetup &setup)
{
    // If the text opens with a structuring tag, then it is structured,
    // otherwise not. Leading whitespace is ignored for this purpose.
    static QRegularExpression staticOpensWithTagRx(QStringLiteral("^\\s*<\\s*(\\w+)[^>]*>"));
    auto opensWithTagRx = staticOpensWithTagRx; // QRegExp not thread-safe
    bool isStructured = false;
    auto match = opensWithTagRx.match(text);
    int p = match.hasMatch()
        ? match.capturedStart()
        : -1;
    if (p >= 0) {
        QString tagName = match.capturedTexts().at(1).toLower();
        if (setup.d->knownTags.contains(tagName)) {
            const KuitTag &tag = setup.d->knownTags.value(tagName);
            isStructured = (tag.type == Kuit::StructTag);
        }
    }
    return isStructured;
}

#define ENTITY_SUBRX "[a-z]+|#[0-9]+|#x[0-9a-fA-F]+"

QString KuitFormatterPrivate::toVisualText(const QString &text_,
        Kuit::VisualFormat format,
        const KuitSetup &setup) const
{
    KuitStaticData *s = staticData();

    // Replace &-shortcut marker with "&amp;", not to confuse the parser;
    // but do not touch & which forms an XML entity as it is.
    QString original = text_;
    QString text;
    int p = original.indexOf(QL1C('&'));
    while (p >= 0) {
        text.append(original.mid(0, p + 1));
        original.remove(0, p + 1);
        static QRegularExpression staticRestRx(QLatin1String("^(" ENTITY_SUBRX ");"));
        QRegularExpression restRx = staticRestRx; // QRegExp not thread-safe
        if (original.indexOf(restRx) != 0) { // not an entity
            text.append(QSL("amp;"));
        }
        p = original.indexOf(QL1C('&'));
    }
    text.append(original);

    // FIXME: Do this and then check proper use of structuring and phrase tags.
#if 0
    // Determine whether this is block-structured text.
    bool isStructured = determineIsStructured(text, setup);
#endif

    const QString INTERNAL_TOP_TAG_NAME = QStringLiteral("__kuit_internal_top__");
    // Add top tag, not to confuse the parser.
    text = QStringLiteral("<%2>%1</%2>").arg(text, INTERNAL_TOP_TAG_NAME);

    QStack<OpenEl> openEls;
    QXmlStreamReader xml(text);
    xml.setEntityResolver(&s->xmlEntityResolver);
    QStringView lastElementName;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            lastElementName = xml.name();

            // Find first proper enclosing element.
            OpenEl enclosingOel;
            for (int i = openEls.size() - 1; i >= 0; --i) {
                if (openEls[i].handling == OpenEl::Proper) {
                    enclosingOel = openEls[i];
                    break;
                }
            }

            // Collect data about this element.
            OpenEl oel = parseOpenEl(xml, enclosingOel, text, setup);

            // Record the new element on the parse stack.
            openEls.push(oel);
        } else if (xml.isEndElement()) {
            // Get closed element data.
            OpenEl oel = openEls.pop();

            // If this was closing of the top element, we're done.
            if (openEls.isEmpty()) {
                // Return with final touches applied.
                return finalizeVisualText(oel.formattedText, format);
            }

            // Append formatted text segment.
            QString ptext = openEls.top().formattedText; // preceding text
            openEls.top().formattedText += formatSubText(ptext, oel,
                                           format, setup);
        } else if (xml.isCharacters()) {
            // Stream reader will automatically resolve default XML entities,
            // which is not desired in this case, as the entities are to be
            // resolved in finalizeVisualText. Convert back into entities.
            const QString ctext = xml.text().toString();
            QString nctext;
            for (const QChar c : ctext) {
                if (s->xmlEntitiesInverse.contains(c)) {
                    const QString entName = s->xmlEntitiesInverse[c];
                    nctext += QL1C('&') + entName + QL1C(';');
                } else {
                    nctext += c;
                }
            }
            openEls.top().formattedText += nctext;
        }
    }

    if (xml.hasError()) {
        qCWarning(KI18N_KUIT) << QStringLiteral(
                               "Markup error in message {%1}: %2. Last tag parsed: %3. Complete message follows:\n%4")
                               .arg(shorten(text), xml.errorString(), lastElementName.toString(),
                               text);
        return QString();
    }

    // Cannot reach here.
    return text;
}

KuitFormatterPrivate::OpenEl
KuitFormatterPrivate::parseOpenEl(const QXmlStreamReader &xml,
                                  const OpenEl &enclosingOel,
                                  const QString &text,
                                  const KuitSetup &setup) const
{
    OpenEl oel;
    oel.name = xml.name().toString().toLower();

    // Collect attribute names and values, and format attribute string.
    QStringList attribNames, attribValues;
    const auto listAttributes = xml.attributes();
    for (const QXmlStreamAttribute &xatt : listAttributes) {
        attribNames += xatt.name().toString().toLower();
        attribValues += xatt.value().toString();
        QChar qc =   attribValues.last().indexOf(QL1C('\'')) < 0
                     ? QL1C('\'') : QL1C('"');
        oel.attribStr +=   QL1C(' ') + attribNames.last() + QL1C('=')
                           + qc + attribValues.last() + qc;
    }

    if (setup.d->knownTags.contains(oel.name)) { // known KUIT element
        const KuitTag &tag = setup.d->knownTags.value(oel.name);
        const KuitTag &etag = setup.d->knownTags.value(enclosingOel.name);

        // If this element can be contained within enclosing element,
        // mark it proper, otherwise mark it for removal.
        if (tag.name.isEmpty()
                || tag.type == Kuit::PhraseTag
                || etag.type == Kuit::StructTag) {
            oel.handling = OpenEl::Proper;
        } else {
            oel.handling = OpenEl::Dropout;
            qCWarning(KI18N_KUIT) << QStringLiteral(
                                   "Structuring tag ('%1') cannot be subtag of phrase tag ('%2') in message {%3}.")
                                   .arg(tag.name, etag.name, shorten(text));
        }

        // Resolve attributes and compute attribute set key.
        QSet<QString> attset;
        for (int i = 0; i < attribNames.size(); ++i) {
            QString att = attribNames[i];
            if (tag.knownAttribs.contains(att)) {
                attset << att;
                oel.attributes[att] = attribValues[i];
            } else {
                qCWarning(KI18N_KUIT) << QStringLiteral(
                                       "Attribute '%1' not defined for tag '%2' in message {%3}.")
                                       .arg(att, tag.name, shorten(text));
            }
        }

        // Continue tag path.
        oel.tagPath = enclosingOel.tagPath;
        oel.tagPath.prepend(enclosingOel.name);

    } else { // unknown element, leave it in verbatim
        oel.handling = OpenEl::Ignored;
        qCWarning(KI18N_KUIT) << QStringLiteral(
                               "Tag '%1' is not defined in message {%2}.")
                               .arg(oel.name, shorten(text));
    }

    return oel;
}

QString KuitFormatterPrivate::formatSubText(const QString &ptext,
        const OpenEl &oel,
        Kuit::VisualFormat format,
        const KuitSetup &setup) const
{
    if (oel.handling == OpenEl::Proper) {
        const KuitTag &tag = setup.d->knownTags.value(oel.name);
        QString ftext = tag.format(languageAsList,
                                   oel.attributes, oel.formattedText,
                                   oel.tagPath, format);

        // Handle leading newlines, if this is not start of the text
        // (ptext is the preceding text).
        if (!ptext.isEmpty() && tag.leadingNewlines > 0) {
            // Count number of present newlines.
            int pnumle, pnumtr, fnumle, fnumtr;
            countWrappingNewlines(ptext, pnumle, pnumtr);
            countWrappingNewlines(ftext, fnumle, fnumtr);
            // Number of leading newlines already present.
            int numle = pnumtr + fnumle;
            // The required extra newlines.
            QString strle;
            if (numle < tag.leadingNewlines) {
                strle = QString(tag.leadingNewlines - numle, QL1C('\n'));
            }
            ftext = strle + ftext;
        }

        return ftext;

    } else if (oel.handling == OpenEl::Ignored) {
        return   QL1C('<') + oel.name + oel.attribStr + QL1C('>')
                 + oel.formattedText
                 + QSL("</") + oel.name + QL1C('>');

    } else { // oel.handling == OpenEl::Dropout
        return oel.formattedText;
    }
}

void KuitFormatterPrivate::countWrappingNewlines(const QString &text,
        int &numle, int &numtr)
{
    int len = text.length();
    // Number of newlines at start of text.
    numle = 0;
    while (numle < len && text[numle] == QL1C('\n')) {
        ++numle;
    }
    // Number of newlines at end of text.
    numtr = 0;
    while (numtr < len && text[len - numtr - 1] == QL1C('\n')) {
        ++numtr;
    }
}

QString KuitFormatterPrivate::finalizeVisualText(const QString &text_,
        Kuit::VisualFormat format) const
{
    KuitStaticData *s = staticData();

    QString text = text_;

    // Resolve XML entities.
    if (format != Kuit::RichText) {
        static QRegularExpression staticEntRx(QLatin1String("&(" ENTITY_SUBRX ");"));
        auto entRx = staticEntRx; // QRegExp not thread-safe
        auto match = entRx.match(text);
        int p = match.hasMatch()
            ? match.capturedStart()
            : -1;
        QString plain;
        while (p >= 0) {
            QString ent = match.capturedTexts().at(1);
            plain.append(text.mid(0, p));
            text.remove(0, p + ent.length() + 2);
            if (ent.startsWith(QL1C('#'))) { // numeric character entity
                QChar c;
                bool ok;
                if (ent[1] == QL1C('x')) {
                    c = QChar(ent.mid(2).toInt(&ok, 16));
                } else {
                    c = QChar(ent.mid(1).toInt(&ok, 10));
                }
                if (ok) {
                    plain.append(c);
                } else { // unknown Unicode point, leave as is
                    plain.append(QL1C('&') + ent + QL1C(';'));
                }
            } else if (s->xmlEntities.contains(ent)) { // known entity
                plain.append(s->xmlEntities[ent]);
            } else { // unknown entity, just leave as is
                plain.append(QL1C('&') + ent + QL1C(';'));
            }

            p = match.hasMatch()
                ? match.capturedStart()
                : -1;        }
        plain.append(text);
        text = plain;
    }

    // Add top tag.
    if (format == Kuit::RichText) {
        text = QStringLiteral("<html>") + text + QStringLiteral("</html>");
    }

    return text;
}

QString KuitFormatterPrivate::salvageMarkup(const QString &text_,
        Kuit::VisualFormat format,
        const KuitSetup &setup) const
{
    QString text = text_;
    QString ntext;
    int pos;

    // Resolve tags simple-mindedly.

    // - tags with content
    static QRegularExpression staticWrapRx(QStringLiteral("(<\\s*(\\w+)\\b([^>]*)>)(.*)(<\\s*/\\s*\\2\\s*>)"));
    auto wrapRx = staticWrapRx; // QRegExp not thread-safe
    //wrapRx.setMinimal(true);
    pos = 0;
    //ntext.clear();
    while (true) {
        int previousPos = pos;
        auto match = wrapRx.match(text);
        pos = match.hasMatch()
            ? match.capturedStart()
            : -1;
        if (pos < 0) {
            ntext += text.mid(previousPos);
            break;
        }
        ntext += text.mid(previousPos, pos - previousPos);
        const QStringList capts = match.capturedTexts();
        QString tagname = capts[2].toLower();
        QString content = salvageMarkup(capts[4], format, setup);
        if (setup.d->knownTags.contains(tagname)) {
            const KuitTag &tag = setup.d->knownTags.value(tagname);
            QHash<QString, QString> attributes;
            // TODO: Do not ignore attributes (in capts[3]).
            ntext += tag.format(languageAsList,
                                attributes, content,
                                QStringList(), format);
        } else {
            ntext += capts[1] + content + capts[5];
        }
        pos += match.capturedLength();
    }
    text = ntext;

    // - tags without content
    static QRegularExpression staticNowrRx(QStringLiteral("<\\s*(\\w+)\\b([^>]*)/\\s*>"));
    auto nowrRx = staticNowrRx; // QRegExp not thread-safe
//    nowrRx.setMinimal(true);
    pos = 0;
    ntext.clear();
    while (true) {
        int previousPos = pos;
        auto match = nowrRx.match(text, previousPos);
        pos = match.hasMatch()
            ? match.capturedStart()
            : -1;
        if (pos < 0) {
            ntext += text.mid(previousPos);
            break;
        }
        ntext += text.mid(previousPos, pos - previousPos);
        const QStringList capts = match.capturedTexts();
        QString tagname = capts[1].toLower();
        if (setup.d->knownTags.contains(tagname)) {
            const KuitTag &tag = setup.d->knownTags.value(tagname);
            ntext += tag.format(languageAsList,
                                QHash<QString, QString>(), QString(),
                                QStringList(), format);
        } else {
            ntext += capts[0];
        }
        pos += match.capturedLength();
    }
    text = ntext;

    // Add top tag.
    if (format == Kuit::RichText) {
        text = QStringLiteral("<html>") + text + QStringLiteral("</html>");
    }

    return text;
}

KuitFormatter::KuitFormatter(const QString &language)
    : d(new KuitFormatterPrivate(language))
{
}

KuitFormatter::~KuitFormatter()
{
    delete d;
}

QString KuitFormatter::format(const QByteArray &domain,
                              const QString &context, const QString &text,
                              Kuit::VisualFormat format) const
{
    return d->format(domain, context, text, format);
}
