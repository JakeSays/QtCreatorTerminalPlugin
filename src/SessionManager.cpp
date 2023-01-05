/*
    This source file is part of terminal, a terminal emulator.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "SessionManager.h"

#include "TerminalDebug.h"

// Qt
#include <QStringList>
#include <QTextCodec>

// KDE
#include <config/kconfig.h>
#include <config/kconfiggroup.h>

// terminal
#include "Session.h"
#include "ProfileManager.h"
#include "History.h"
#include "Enumeration.h"
#include "TerminalDisplay.h"

using namespace terminal;

SessionManager::SessionManager() :
    _sessions(QList<Session *>()),
    _sessionProfiles(QHash<Session *, Profile::Ptr>()),
    _sessionRuntimeProfiles(QHash<Session *, Profile::Ptr>()),
    _restoreMapping(QHash<Session *, int>()),
    _isClosingAllSessions(false)
{
    ProfileManager *profileMananger = ProfileManager::instance();
    connect(profileMananger, &terminal::ProfileManager::profileChanged, this,
            &terminal::SessionManager::profileChanged);
}

SessionManager::~SessionManager()
{
    if (!_sessions.isEmpty()) {
        qCDebug(TerminalDebug) << "terminal SessionManager destroyed with"
                              << _sessions.count()
                              <<"session(s) still alive";
        // ensure that the Session doesn't later try to call back and do things to the
        // SessionManager
        for (auto session : _sessions)
        {
            disconnect(session, nullptr, this, nullptr);
        }
    }
}

Q_GLOBAL_STATIC(SessionManager, theSessionManager)
SessionManager* SessionManager::instance()
{
    return theSessionManager;
}

bool SessionManager::isClosingAllSessions() const
{
    return _isClosingAllSessions;
}

void SessionManager::closeAllSessions()
{
    _isClosingAllSessions = true;
    for (auto session : _sessions)
    {
        session->close();
    }
    _sessions.clear();
}

const QList<Session *> SessionManager::sessions() const
{
    return _sessions;
}

Session *SessionManager::createSession(Profile::Ptr profile)
{
    if (!profile) {
        profile = ProfileManager::instance()->defaultProfile();
    }

    // TODO: check whether this is really needed
    if (!ProfileManager::instance()->loadedProfiles().contains(profile)) {
        ProfileManager::instance()->addProfile(profile);
    }

    //configuration information found, create a new session based on this
    auto session = new Session();
    Q_ASSERT(session);
    applyProfile(session, profile, false);

    connect(session, &terminal::Session::profileChangeCommandReceived, this,
            &terminal::SessionManager::sessionProfileCommandReceived);

    //ask for notification when session dies
    connect(session, &terminal::Session::finished, this,
            [this, session]() {
                sessionTerminated(session);
            });

    //add session to active list
    _sessions << session;
    _sessionProfiles.insert(session, profile);

    return session;
}

void SessionManager::profileChanged(const Profile::Ptr &profile)
{
    applyProfile(profile, true);
}

void SessionManager::sessionTerminated(Session *session)
{
    Q_ASSERT(session);

    _sessions.removeAll(session);
    _sessionProfiles.remove(session);
    _sessionRuntimeProfiles.remove(session);

    session->deleteLater();
}

void SessionManager::applyProfile(const Profile::Ptr &profile, bool modifiedPropertiesOnly)
{
    for (auto session : _sessions)
    {
        if (_sessionProfiles[session] == profile) {
            applyProfile(session, profile, modifiedPropertiesOnly);
        }
    }
}

Profile::Ptr SessionManager::sessionProfile(Session *session) const
{
    return _sessionProfiles[session];
}

void SessionManager::setSessionProfile(Session *session, Profile::Ptr profile)
{
    if (!profile) {
        profile = ProfileManager::instance()->defaultProfile();
    }

    Q_ASSERT(profile);

    _sessionProfiles[session] = profile;

    applyProfile(session, profile, false);

    emit sessionUpdated(session);
}

void SessionManager::applyProfile(Session *session, const Profile::Ptr &profile,
                                  bool modifiedPropertiesOnly)
{
    Q_ASSERT(profile);

    _sessionProfiles[session] = profile;

    ShouldApplyProperty apply(profile, modifiedPropertiesOnly);

    // Basic session settings
    if (apply.shouldApply(Profile::Name)) {
        session->setTitle(Session::NameRole, profile->name());
    }

    if (apply.shouldApply(Profile::Command)) {
        session->setProgram(profile->command());
    }

    if (apply.shouldApply(Profile::Arguments)) {
        session->setArguments(profile->arguments());
    }

    if (apply.shouldApply(Profile::Directory)) {
        session->setInitialWorkingDirectory(profile->defaultWorkingDirectory());
    }

    if (apply.shouldApply(Profile::Environment)) {
        // add environment variable containing home directory of current profile
        // (if specified)

        // prepend a 0 to the VERSION_MICRO part to make the version string
        // length consistent, so that conditions that depend on the exported
        // env var actually work
        // e.g. the second version should be higher than the first one:
        // 18.04.12 -> 180412
        // 18.08.0  -> 180800
        QStringList list = QStringLiteral(KONSOLE_VERSION).split(QLatin1Char('.'));
        if (list[2].length() < 2) {
                list[2].prepend(QLatin1String("0"));
        }
        const QString &numericVersion = list.join(QString());

        QStringList environment = profile->environment();
        environment << QStringLiteral("PROFILEHOME=%1").arg(profile->defaultWorkingDirectory());
        environment << QStringLiteral("KONSOLE_VERSION=%1").arg(numericVersion);

        session->setEnvironment(environment);
    }

    if (apply.shouldApply(Profile::TerminalColumns)
        || apply.shouldApply(Profile::TerminalRows)) {
        const auto columns = profile->property<int>(Profile::TerminalColumns);
        const auto rows = profile->property<int>(Profile::TerminalRows);
        session->setPreferredSize(QSize(columns, rows));
    }

    if (apply.shouldApply(Profile::Icon)) {
        session->setIconName(profile->icon());
    }

    // Key bindings
    if (apply.shouldApply(Profile::KeyBindings)) {
        session->setKeyBindings(profile->keyBindings());
    }

    // Tab formats
    // Preserve tab title changes, made by the user, when applying profile
    // changes or previewing color schemes
    if (apply.shouldApply(Profile::LocalTabTitleFormat) && !session->isTabTitleSetByUser()) {
        session->setTabTitleFormat(Session::LocalTabTitle,
                                   profile->localTabTitleFormat());
    }
    if (apply.shouldApply(Profile::RemoteTabTitleFormat) && !session->isTabTitleSetByUser()) {
        session->setTabTitleFormat(Session::RemoteTabTitle,
                                   profile->remoteTabTitleFormat());
    }

    // History
    if (apply.shouldApply(Profile::HistoryMode) || apply.shouldApply(Profile::HistorySize)) {
        const auto mode = profile->property<int>(Profile::HistoryMode);
        switch (mode) {
        case Enum::NoHistory:
            session->setHistoryType(HistoryTypeNone());
            break;

        case Enum::FixedSizeHistory:
        {
            int lines = profile->historySize();
            session->setHistoryType(CompactHistoryType(lines));
            break;
        }

        case Enum::UnlimitedHistory:
            session->setHistoryType(HistoryTypeFile());
            break;
        }
    }

    // Terminal features
    if (apply.shouldApply(Profile::FlowControlEnabled)) {
        session->setFlowControlEnabled(profile->flowControlEnabled());
    }

    // Encoding
    if (apply.shouldApply(Profile::DefaultEncoding)) {
        QByteArray name = profile->defaultEncoding().toUtf8();
        session->setCodec(QTextCodec::codecForName(name));
    }

    // Monitor Silence
    if (apply.shouldApply(Profile::SilenceSeconds)) {
        session->setMonitorSilenceSeconds(profile->silenceSeconds());
    }
}

void SessionManager::sessionProfileCommandReceived(const QString &text)
{
    auto *session = qobject_cast<Session *>(sender());
    Q_ASSERT(session);

    // store the font for each view if zoom was applied so that they can
    // be restored after applying the new profile
    QHash<TerminalDisplay *, QFont> zoomFontSizes;

    for (auto view : session->views())
    {
        const QFont &viewCurFont = view->getVTFont();
        if (viewCurFont != _sessionProfiles[session]->font()) {
            zoomFontSizes.insert(view, viewCurFont);
        }
    }

    ProfileCommandParser parser;
    QHash<Profile::Property, QVariant> changes = parser.parse(text);

    Profile::Ptr newProfile;
    if (!_sessionRuntimeProfiles.contains(session)) {
        newProfile = new Profile(_sessionProfiles[session]);
        _sessionRuntimeProfiles.insert(session, newProfile);
    } else {
        newProfile = _sessionRuntimeProfiles[session];
    }

    for (auto key : changes.keys())
    {
        newProfile->setProperty(key, changes[key]);
    }

    _sessionProfiles[session] = newProfile;
    applyProfile(newProfile, true);
    emit sessionUpdated(session);

    if (!zoomFontSizes.isEmpty())
    {
        for (auto key : zoomFontSizes.keys())
        {
            key->setVTFont(zoomFontSizes[key]);
        }
    }
}

void SessionManager::saveSessions(KConfig *config)
{
    // The session IDs can't be restored.
    // So we need to map the old ID to the future new ID.
    int n = 1;
    _restoreMapping.clear();

    for (auto session : _sessions)
    {
        QString name = QLatin1String("Session") + QString::number(n);
        KConfigGroup group(config, name);

        group.writePathEntry("Profile",
                             _sessionProfiles.value(session)->path());
        session->saveSession(group);
        _restoreMapping.insert(session, n);
        n++;
    }

    KConfigGroup group(config, "Number");
    group.writeEntry("NumberOfSessions", _sessions.count());
}

int SessionManager::getRestoreId(Session *session)
{
    return _restoreMapping.value(session);
}

void SessionManager::restoreSessions(KConfig *config)
{
    KConfigGroup group(config, "Number");
    const int sessions = group.readEntry("NumberOfSessions", 0);

    // Any sessions saved?
    for (int n = 1; n <= sessions; n++) {
        const QString name = QLatin1String("Session") + QString::number(n);
        KConfigGroup sessionGroup(config, name);

        const QString profile = sessionGroup.readPathEntry("Profile", QString());
        Profile::Ptr ptr = ProfileManager::instance()->defaultProfile();
        if (!profile.isEmpty()) {
            ptr = ProfileManager::instance()->loadProfile(profile);
        }
        Session *session = createSession(ptr);
        session->restoreSession(sessionGroup);
    }
}

Session *SessionManager::idToSession(int id)
{
    for (auto session : _sessions)
    {
        if (session->sessionId() == id) {
            return session;
        }
    }
    // this should not happen
    qCDebug(TerminalDebug) << "Failed to find session for ID" << id;
    return nullptr;
}
