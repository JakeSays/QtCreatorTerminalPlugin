/*
    This file is part of terminal

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>
    Copyright 2009 by Thomas Dreibholz <dreibh@iem.uni-due.de>

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
#include "Session.h"

// Standard
#include <cstdlib>
#include <csignal>
#include <unistd.h>

#include <coreplugin/editormanager/editormanager.h>

// Qt
#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QKeyEvent>
#include <QStandardPaths>
#include <QThread>

// KDE
#include <KLocalizedString>
//#include <KNotification>
//#include <KRun>
//#include <KShell>
#include <kprocess.h>
#include <config/kconfiggroup.h>
//#include <KIO/DesktopExecParser>

// terminal
//#include <sessionadaptor.h>

#include "ProcessInfo.h"
#include "Pty.h"
#include "TerminalDisplay.h"
#include "ShellCommand.h"
#include "Vt102Emulation.h"
//#include "ZModemDialog.h"
#include "History.h"
#include "TerminalDebug.h"
#include "SessionManager.h"
#include "ProfileManager.h"
//#include "Profile.h"

using namespace terminal;

static QList<QString> _knownShells({QStringLiteral("bash"),
    QStringLiteral("ash"),
    QStringLiteral("csh"),
    QStringLiteral("dash"),
    QStringLiteral("fish"),
    QStringLiteral("hush"),
    QStringLiteral("ksh"),
    QStringLiteral("mksh"),
    QStringLiteral("pdksh"),
    QStringLiteral("tcsh"),
    QStringLiteral("zsh")});

int Session::lastSessionId = 0;
//static bool show_disallow_certain_dbus_methods_message = true;

//static const int ZMODEM_BUFFER_SIZE = 1048576; // 1 Mb

Session::Session(QObject* parent) :
    QObject(parent)
    , _uniqueIdentifier(QUuid())
    , _shellProcess(nullptr)
    , _emulation(nullptr)
    , _views(QList<TerminalDisplay *>())
    , _monitorActivity(false)
    , _monitorSilence(false)
    , _notifiedActivity(false)
    , _silenceSeconds(10)
    , _silenceTimer(nullptr)
    , _activityTimer(nullptr)
    , _autoClose(true)
    , _closePerUserRequest(false)
    , _nameTitle(QString())
    , _displayTitle(QString())
    , _userTitle(QString())
    , _localTabTitleFormat(QString())
    , _remoteTabTitleFormat(QString())
    , _tabTitleSetByUser(false)
    , _iconName(QString())
    , _iconText(QString())
    , _addToUtmp(true)
    , _flowControlEnabled(true)
    , _program(QString())
    , _arguments(QStringList())
    , _environment(QStringList())
    , _sessionId(0)
    , _initialWorkingDir(QString())
    , _currentWorkingDir(QString())
    , _reportedWorkingUrl(QUrl())
    , _sessionProcessInfo(nullptr)
    , _foregroundProcessInfo(nullptr)
    , _foregroundPid(0)
    , _hasDarkBackground(false)
    , _preferredSize(QSize())
    , _readOnly(false)
    , _isPrimaryScreen(true)
{
    _uniqueIdentifier = QUuid::createUuid();

    //prepare DBus communication
//    new SessionAdaptor(this);
    _sessionId = ++lastSessionId;
//    QDBusConnection::sessionBus().registerObject(QLatin1String("/Sessions/") + QString::number(_sessionId), this);

    //create emulation backend
    _emulation = new Vt102Emulation();

    connect(_emulation, &terminal::Emulation::sessionAttributeChanged, this, &terminal::Session::setSessionAttribute);
    connect(_emulation, &terminal::Emulation::stateSet, this, &terminal::Session::activityStateSet);
    connect(_emulation, &terminal::Emulation::changeTabTextColorRequest, this, &terminal::Session::changeTabTextColor);
    connect(_emulation, &terminal::Emulation::profileChangeCommandReceived, this, &terminal::Session::profileChangeCommandReceived);
    connect(_emulation, &terminal::Emulation::flowControlKeyPressed, this, &terminal::Session::updateFlowControlState);
    connect(_emulation, &terminal::Emulation::primaryScreenInUse, this, &terminal::Session::onPrimaryScreenInUse);
    connect(_emulation, &terminal::Emulation::selectionChanged, this, &terminal::Session::selectionChanged);
    connect(_emulation, &terminal::Emulation::imageResizeRequest, this, &terminal::Session::resizeRequest);
    connect(_emulation, &terminal::Emulation::sessionAttributeRequest, this, &terminal::Session::sessionAttributeRequest);

    //create new teletype for I/O with shell process
    openTeletype(-1);

    //setup timer for monitoring session activity & silence
    _silenceTimer = new QTimer(this);
    _silenceTimer->setSingleShot(true);
    connect(_silenceTimer, &QTimer::timeout, this, &terminal::Session::silenceTimerDone);

    _activityTimer = new QTimer(this);
    _activityTimer->setSingleShot(true);
    connect(_activityTimer, &QTimer::timeout, this, &terminal::Session::activityTimerDone);
}

Session::~Session()
{
    delete _foregroundProcessInfo;
    delete _sessionProcessInfo;
    delete _emulation;
    delete _shellProcess;
}

void Session::openTeletype(int fd)
{
    if (isRunning()) {
        qWarning() << "Attempted to open teletype in a running session.";
        return;
    }

    delete _shellProcess;

    if (fd < 0) {
        _shellProcess = new Pty();
    } else {
        _shellProcess = new Pty(fd);
    }

    _shellProcess->setUtf8Mode(_emulation->utf8());

    // connect the I/O between emulator and pty process
    connect(_shellProcess, &terminal::Pty::receivedData, this, &terminal::Session::onReceiveBlock);
    connect(_emulation, &terminal::Emulation::sendData, _shellProcess, &terminal::Pty::sendData);

    // UTF8 mode
    connect(_emulation, &terminal::Emulation::useUtf8Request, _shellProcess, &terminal::Pty::setUtf8Mode);

    // get notified when the pty process is finished
    connect(_shellProcess,
            QOverload<int,QProcess::ExitStatus>::of(&terminal::Pty::finished),
            this, &terminal::Session::done);

    // emulator size
    // Use a direct connection to ensure that the window size is set before it runs
    connect(_emulation, &terminal::Emulation::imageSizeChanged, this, &terminal::Session::updateWindowSize, Qt::DirectConnection);
    connect(_emulation, &terminal::Emulation::imageSizeInitialized, this, &terminal::Session::run);
}

WId Session::windowId() const
{
    // Returns a window ID for this session which is used
    // to set the WINDOWID environment variable in the shell
    // process.
    //
    // Sessions can have multiple views or no views, which means
    // that a single ID is not always going to be accurate.
    //
    // If there are no views, the window ID is just 0.  If
    // there are multiple views, then the window ID for the
    // top-level window which contains the first view is
    // returned

    if (_views.count() == 0) {
        return 0;
    } else {
        /**
         * compute the windows id to use
         * doesn't call winId on some widget, as this might lead
         * to rendering artifacts as this will trigger the
         * creation of a native window, see https://doc.qt.io/qt-5/qwidget.html#winId
         * instead, use https://doc.qt.io/qt-5/qwidget.html#effectiveWinId
         */
        QWidget* widget = _views.first();
        Q_ASSERT(widget);
        return widget->effectiveWinId();
    }
}

void Session::setDarkBackground(bool darkBackground)
{
    _hasDarkBackground = darkBackground;
}

bool Session::isRunning() const
{
    return (_shellProcess != nullptr) && (_shellProcess->state() == QProcess::Running);
}

void Session::setCodec(QTextCodec* codec)
{
    if (isReadOnly()) {
        return;
    }

    emulation()->setCodec(codec);
}

bool Session::setCodec(const QByteArray& name)
{
    QTextCodec* codec = QTextCodec::codecForName(name);

    if (codec != nullptr) {
        setCodec(codec);
        return true;
    } else {
        return false;
    }
}

QByteArray Session::codec()
{
    return _emulation->codec()->name();
}

void Session::setProgram(const QString& program)
{
    _program = ShellCommand::expand(program);
}

void Session::setArguments(const QStringList& arguments)
{
    _arguments = ShellCommand::expand(arguments);
}

constexpr char ESCAPE = 0x1B;

QString tildeExpand(const QString &fname)
{
    if (!fname.isEmpty() && fname[0] == u'~')
    {
        int pos = fname.indexOf(u'/');
        if (pos < 0) {
            return QDir::homePath() + "/" + fname;
        }
        QString ret = QDir::homePath() + fname.mid(1, pos - 1);
        if (!ret.isNull()) {
            ret += fname.mid(pos);
        }
        return ret;
    } else if (fname.length() > 1 && fname[0] == QLatin1Char(ESCAPE) && fname[1] == u'~') {
        return fname.mid(1);
    }
    return fname;
}

void Session::setInitialWorkingDirectory(const QString& dir)
{
    _initialWorkingDir = validDirectory(tildeExpand(ShellCommand::expand(dir)));
}

QString Session::currentWorkingDirectory()
{
    if (_reportedWorkingUrl.isValid() && _reportedWorkingUrl.isLocalFile()) {
        return _reportedWorkingUrl.path();
    }

    // only returned cached value
    if (_currentWorkingDir.isEmpty()) {
        updateWorkingDirectory();
    }

    return _currentWorkingDir;
}
void Session::updateWorkingDirectory()
{
    updateSessionProcessInfo();

    const QString currentDir = _sessionProcessInfo->validCurrentDir();
    if (currentDir != _currentWorkingDir) {
        _currentWorkingDir = currentDir;
        emit currentDirectoryChanged(_currentWorkingDir);
    }
}

QList<TerminalDisplay*> Session::views() const
{
    return _views;
}

void Session::addView(TerminalDisplay* widget)
{
    Q_ASSERT(!_views.contains(widget));

    _views.append(widget);

    // connect emulation - view signals and slots
    connect(widget, &terminal::TerminalDisplay::keyPressedSignal, _emulation, &terminal::Emulation::sendKeyEvent);
    connect(widget, &terminal::TerminalDisplay::mouseSignal, _emulation, &terminal::Emulation::sendMouseEvent);
    connect(widget, &terminal::TerminalDisplay::sendStringToEmu, _emulation, &terminal::Emulation::sendString);

    // allow emulation to notify the view when the foreground process
    // indicates whether or not it is interested in Mouse Tracking events
    connect(_emulation, &terminal::Emulation::programRequestsMouseTracking, widget, &terminal::TerminalDisplay::setUsesMouseTracking);

    widget->setUsesMouseTracking(_emulation->programUsesMouseTracking());

    connect(_emulation, &terminal::Emulation::enableAlternateScrolling, widget, &terminal::TerminalDisplay::setAlternateScrolling);

    connect(_emulation, &terminal::Emulation::programBracketedPasteModeChanged, widget, &terminal::TerminalDisplay::setBracketedPasteMode);

    widget->setBracketedPasteMode(_emulation->programBracketedPasteMode());

    widget->setScreenWindow(_emulation->createWindow());

    //connect view signals and slots
    connect(widget, &terminal::TerminalDisplay::changedContentSizeSignal, this, &terminal::Session::onViewSizeChange);

    connect(widget, &terminal::TerminalDisplay::destroyed, this, &terminal::Session::viewDestroyed);

    connect(widget, &terminal::TerminalDisplay::focusLost, _emulation, &terminal::Emulation::focusLost);
    connect(widget, &terminal::TerminalDisplay::focusGained, _emulation, &terminal::Emulation::focusGained);

    connect(_emulation, &terminal::Emulation::setCursorStyleRequest, widget, &terminal::TerminalDisplay::setCursorStyle);
    connect(_emulation, &terminal::Emulation::resetCursorStyleRequest, widget, &terminal::TerminalDisplay::resetCursorStyle);
}

void Session::viewDestroyed(QObject* view)
{
    auto* display = reinterpret_cast<TerminalDisplay*>(view);

    Q_ASSERT(_views.contains(display));

    removeView(display);
}

void Session::removeView(TerminalDisplay* widget)
{
    _views.removeAll(widget);

    disconnect(widget, nullptr, this, nullptr);

    // disconnect
    //  - key presses signals from widget
    //  - mouse activity signals from widget
    //  - string sending signals from widget
    //
    //  ... and any other signals connected in addView()
    disconnect(widget, nullptr, _emulation, nullptr);

    // disconnect state change signals emitted by emulation
    disconnect(_emulation, nullptr, widget, nullptr);

    // close the session automatically when the last view is removed
    if (_views.count() == 0) {
        close();
    }
}

static bool ShellExists(const QString& shell)
{
    QFileInfo info(shell);
    if (info.exists() && info.isExecutable())
    {
        return true;
    }

    return false;
}

QString Session::checkShellProgram(const QString& shell)
{
    auto exec = QString("/bin/%1").arg(shell);
    if (ShellExists(exec))
    {
        return exec;
    }

    exec = QString("/sbin/%1").arg(shell);
    if (ShellExists(exec))
    {
        return exec;
    }

    exec = QString("/usr/bin/%1").arg(shell);
    if (ShellExists(exec))
    {
        return exec;
    }

    QFileInfo info(exec);
    if (info.exists() && info.isExecutable())
    {
        return exec;
    }

    return QString();
}

// Upon a KPty error, there is no description on what that error was...
// Check to see if the given program is executable.
QString Session::checkProgram(const QString& program)
{
    QString exec = program;

    if (exec.isEmpty()) {
        return QString();
    }

    QFileInfo info(exec);
    if (info.isAbsolute() && info.exists() && info.isExecutable()) {
        return exec;
    }

//    exec = KIO::DesktopExecParser::executablePath(exec);
    exec = tildeExpand(exec);
    const QString pexec = QStandardPaths::findExecutable(exec);
    if(pexec.isEmpty())
    {
        qCritical() << i18n("Could not find binary: ") << exec;
        return QString();
    }

    return exec;
}

void Session::terminalWarning(const QString& message)
{
    static const QByteArray warningText = "Warning: ";
    QByteArray messageText = message.toLocal8Bit();

    static const char redPenOn[] = "\033[1m\033[31m";
    static const char redPenOff[] = "\033[0m";

    _emulation->receiveData(redPenOn, qstrlen(redPenOn));
    _emulation->receiveData("\n\r\n\r", 4);
    _emulation->receiveData(warningText.constData(), qstrlen(warningText.constData()));
    _emulation->receiveData(messageText.constData(), qstrlen(messageText.constData()));
    _emulation->receiveData("\n\r\n\r", 4);
    _emulation->receiveData(redPenOff, qstrlen(redPenOff));
}

QString Session::shellSessionId() const
{
    QString friendlyUuid(_uniqueIdentifier.toString());
    friendlyUuid.remove(u'-').remove(u'{').remove(u'}');

    return friendlyUuid;
}

QString Session::locateShell()
{
    QString shell = checkProgram(QString::fromUtf8(qgetenv("SHELL")));

    if (shell.isEmpty())
    {
        for (auto& shell : _knownShells)
        {
            auto shExec = checkShellProgram(shell);
            if (!shExec.isEmpty())
            {
                shell = shExec;
            }
        }

        if (shell.isEmpty())
        {
            shell = checkProgram(QStringLiteral("/bin/sh"));
        }

        if (shell.isEmpty())
        {
            terminalWarning(i18n("Could not find an interactive shell to start."));
        }
    }

    return shell;
}

void Session::run()
{
    // FIXME: run() is called twice in some instances
    if(isRunning())
    {
        qCDebug(TerminalDebug) << "Attempted to re-run an already running session ("
                               << _shellProcess->pid() << ")";
        return;
    }

    //check that everything is in place to run the session
    if(_program.isEmpty())
    {
        qWarning() << "Program to run not set.";
    }

    if(_arguments.isEmpty())
    {
        qWarning() << "No command line arguments specified.";
    }

    if(_uniqueIdentifier.isNull())
    {
        _uniqueIdentifier = QUuid::createUuid();
    }

    auto exec = locateShell();

    if (exec.isEmpty())
    {
        return;
    }

    // if no arguments are specified, fall back to program name
    auto arguments = _arguments.join(u' ').isEmpty()
        ? QStringList() << exec
        : _arguments;

    auto workingDirectory = _initialWorkingDir;

    if (Core::IDocument *doc = Core::EditorManager::instance()->currentDocument())
    {
        const QDir dir = doc->filePath().toFileInfo().absoluteDir();
        if (dir.exists())
        {
            workingDirectory = dir.canonicalPath();
        }
    }

    if (workingDirectory.isEmpty())
    {
        workingDirectory = QDir::currentPath();
    }

    _shellProcess->setInitialWorkingDirectory(workingDirectory);

    _shellProcess->setFlowControlEnabled(_flowControlEnabled);
    _shellProcess->setEraseChar(_emulation->eraseChar());
    _shellProcess->setUseUtmp(_addToUtmp);

    // this is not strictly accurate use of the COLORFGBG variable.  This does not
    // tell the terminal exactly which colors are being used, but instead approximates
    // the color scheme as "black on white" or "white on black" depending on whether
    // the background color is deemed dark or not
    const QString backgroundColorHint =_hasDarkBackground
        ? QStringLiteral("COLORFGBG=15;0")
        : QStringLiteral("COLORFGBG=0;15");

    addEnvironmentEntry(backgroundColorHint);

    addEnvironmentEntry(QStringLiteral("SHELL_SESSION_ID=%1").arg(shellSessionId()));

    addEnvironmentEntry(QStringLiteral("WINDOWID=%1").arg(QString::number(windowId())));

    addEnvironmentEntry("TERM_PROGRAM=terminal");
//from old qtermwidget    addEnvironmentEntry("TERM=xterm-256color");
    addEnvironmentEntry(QString("QTCREATOR_PID=%1").arg(QCoreApplication::applicationPid()));

//    const QString dbusService = QDBusConnection::sessionBus().baseService();
//    addEnvironmentEntry(QStringLiteral("KONSOLE_DBUS_SERVICE=%1").arg(dbusService));

//    const QString dbusObject = QStringLiteral("/Sessions/%1").arg(QString::number(_sessionId));
//    addEnvironmentEntry(QStringLiteral("KONSOLE_DBUS_SESSION=%1").arg(dbusObject));

    int result = _shellProcess->start(exec, arguments, _environment);
    if(result < 0)
    {
        terminalWarning(QString("Could not start program '%1' with arguments '%2'.")
                            .arg(exec)
                            .arg(arguments.join(u' ')));
        terminalWarning(_shellProcess->errorString());
        return;
    }

    _shellProcess->setWriteable(false);  // We are reachable via kwrited.

    emit started();
}

void Session::setSessionAttribute(int what, const QString& caption)
{
    // set to true if anything has actually changed
    // eg. old _nameTitle != new _nameTitle
    bool modified = false;

    if ((what == IconNameAndWindowTitle) || (what == WindowTitle)) {
        if (_userTitle != caption) {
            _userTitle = caption;
            modified = true;
        }
    }

    if ((what == IconNameAndWindowTitle) || (what == IconName)) {
        if (_iconText != caption) {
            _iconText = caption;
            modified = true;
        }
    }

    if (what == TextColor || what == BackgroundColor) {
        QString colorString = caption.section(u';', 0, 0);
        QColor color = QColor(colorString);
        if (color.isValid()) {
            if (what == TextColor) {
                emit changeForegroundColorRequest(color);
            } else {
                emit changeBackgroundColorRequest(color);
            }
        }
    }

    if (what == SessionName) {
        if (_localTabTitleFormat != caption) {
            _localTabTitleFormat = caption;
            setTitle(Session::DisplayedTitleRole, caption);
            modified = true;
        }
    }

    /* The below use of 32 works but appears to non-standard.
       It is from a commit from 2004 c20973eca8776f9b4f15bee5fdcb5a3205aa69de
     */
    // change icon via \033]32;Icon\007
    if (what == SessionIcon) {
        if (_iconName != caption) {
            _iconName = caption;

            modified = true;
        }
    }

    if (what == CurrentDirectory) {
        _reportedWorkingUrl = QUrl::fromUserInput(caption);
        emit currentDirectoryChanged(currentWorkingDirectory());
        modified = true;
    }

    if (what == ProfileChange) {
        emit profileChangeCommandReceived(caption);
        return;
    }

    if (modified) {
        emit sessionAttributeChanged();
    }
}

QString Session::userTitle() const
{
    return _userTitle;
}

void Session::setTabTitleFormat(TabTitleContext context , const QString& format)
{
    if (context == LocalTabTitle) {
        _localTabTitleFormat = format;
        ProcessInfo* process = getProcessInfo();
        process->setUserNameRequired(format.contains(u"%u"_qs));
    } else if (context == RemoteTabTitle) {
        _remoteTabTitleFormat = format;
    }
}

QString Session::tabTitleFormat(TabTitleContext context) const
{
    if (context == LocalTabTitle) {
        return _localTabTitleFormat;
    } else if (context == RemoteTabTitle) {
        return _remoteTabTitleFormat;
    }

    return QString();
}

void Session::tabTitleSetByUser(bool set)
{
    _tabTitleSetByUser = set;
}

bool Session::isTabTitleSetByUser() const
{
    return _tabTitleSetByUser;
}

void Session::silenceTimerDone()
{
    //FIXME: The idea here is that the notification popup will appear to tell the user than output from
    //the terminal has stopped and the popup will disappear when the user activates the session.
    //
    //This breaks with the addition of multiple views of a session.  The popup should disappear
    //when any of the views of the session becomes active

    //FIXME: Make message text for this notification and the activity notification more descriptive.
    if (!_monitorSilence)
    {
        emit stateChanged(NOTIFYNORMAL);
        return;
    }

//    bool hasFocus = false;
//    for(auto display : _views)
//    {
//        if(display->hasFocus())
//        {
//            hasFocus = true;
//            break;
//        }
//    }

//    KNotification::event(hasFocus ? QStringLiteral("Silence") : QStringLiteral("SilenceHidden"),
//            i18n("Silence in session '%1'", _nameTitle), QPixmap(),
//            QApplication::activeWindow(),
//            KNotification::CloseWhenWidgetActivated);
    emit stateChanged(NOTIFYSILENCE);
}

void Session::activityTimerDone()
{
    _notifiedActivity = false;
}

void Session::updateFlowControlState(bool suspended)
{
    if (suspended) {
        if (flowControlEnabled()) {
            for (auto display : _views) {
                if (display->flowControlWarningEnabled()) {
                    display->outputSuspended(true);
                }
            }
        }
    } else {
        for (auto display : _views) {
            display->outputSuspended(false);
        }
    }
}

void Session::changeTabTextColor(int i)
{
    qCDebug(TerminalDebug) << "Changing tab text color is not implemented "<<i;
}

void Session::onPrimaryScreenInUse(bool use)
{

    _isPrimaryScreen = use;
    emit primaryScreenInUse(use);
}
bool Session::isPrimaryScreen()
{
    return _isPrimaryScreen;
}

void Session::sessionAttributeRequest(int id)
{
    switch (id) {
        case BackgroundColor:
            // Get 'TerminalDisplay' (_view) background color
            emit getBackgroundColor();
            break;
    }
}

void Session::activityStateSet(int state)
{
    // TODO: should this hardcoded interval be user configurable?
    const int activityMaskInSeconds = 15;

    if (state == NOTIFYBELL) {
        emit bellRequest(i18n("Bell in session '%1'", _nameTitle));
    } else if (state == NOTIFYACTIVITY) {
        // Don't notify if the terminal is active
//        bool hasFocus = false;
//        for (auto display : _views)
//        {
//            if (display->hasFocus()) {
//                hasFocus = true;
//                break;
//            }
//        }

        if (_monitorActivity  && !_notifiedActivity) {
//            KNotification::event(hasFocus ? QStringLiteral("Activity") : QStringLiteral("ActivityHidden"),
//                                 i18n("Activity in session '%1'", _nameTitle), QPixmap(),
//                                 QApplication::activeWindow(),
//                                 KNotification::CloseWhenWidgetActivated);

            // mask activity notification for a while to avoid flooding
            _notifiedActivity = true;
            _activityTimer->start(activityMaskInSeconds * 1000);
        }

        // reset the counter for monitoring continuous silence since there is activity
        if (_monitorSilence) {
            _silenceTimer->start(_silenceSeconds * 1000);
        }
    }

    if (state == NOTIFYACTIVITY && !_monitorActivity) {
        state = NOTIFYNORMAL;
    }
    if (state == NOTIFYSILENCE && !_monitorSilence) {
        state = NOTIFYNORMAL;
    }

    emit stateChanged(state);
}

void Session::onViewSizeChange(int /*height*/, int /*width*/)
{
    updateTerminalSize();
}

void Session::updateTerminalSize()
{
    int minLines = -1;
    int minColumns = -1;

    // minimum number of lines and columns that views require for
    // their size to be taken into consideration ( to avoid problems
    // with new view widgets which haven't yet been set to their correct size )
    const int VIEW_LINES_THRESHOLD = 2;
    const int VIEW_COLUMNS_THRESHOLD = 2;

    //select largest number of lines and columns that will fit in all visible views
    for (auto view : _views)
    {
        if (!view->isHidden() &&
                view->lines() >= VIEW_LINES_THRESHOLD &&
                view->columns() >= VIEW_COLUMNS_THRESHOLD) {
            minLines = (minLines == -1) ? view->lines() : qMin(minLines , view->lines());
            minColumns = (minColumns == -1) ? view->columns() : qMin(minColumns , view->columns());
            view->processFilters();
        }
    }

    // backend emulation must have a _terminal of at least 1 column x 1 line in size
    if (minLines > 0 && minColumns > 0) {
        _emulation->setImageSize(minLines , minColumns);
    }
}
void Session::updateWindowSize(int lines, int columns)
{
    Q_ASSERT(lines > 0 && columns > 0);
    _shellProcess->setWindowSize(columns, lines);
}
void Session::refresh()
{
    // attempt to get the shell process to redraw the display
    //
    // this requires the program running in the shell
    // to cooperate by sending an update in response to
    // a window size change
    //
    // the window size is changed twice, first made slightly larger and then
    // resized back to its normal size so that there is actually a change
    // in the window size (some shells do nothing if the
    // new and old sizes are the same)
    //
    // if there is a more 'correct' way to do this, please
    // send an email with method or patches to konsole-devel@kde.org

    const QSize existingSize = _shellProcess->windowSize();
    _shellProcess->setWindowSize(existingSize.width() + 1, existingSize.height());
    // introduce small delay to avoid changing size too quickly
    QThread::usleep(500);
    _shellProcess->setWindowSize(existingSize.width(), existingSize.height());
}

void Session::sendSignal(int signal)
{
    const ProcessInfo* process = getProcessInfo();
    bool ok = false;
    int pid;
    pid = process->foregroundPid(&ok);

    if (ok) {
        ::kill(pid, signal);
    }
}

void Session::reportBackgroundColor(const QColor& c)
{
    #define to65k(a) (QStringLiteral("%1").arg(int(((a)*0xFFFF)), 4, 16, QLatin1Char('0')))
    QString msg = QStringLiteral("\033]11;rgb:")
                + to65k(c.redF())   + u'/'
                + to65k(c.greenF()) + u'/'
                + to65k(c.blueF())  + u'';
    _emulation->sendString(msg.toUtf8());
    #undef to65k
}

bool Session::kill(int signal)
{
    if (_shellProcess->pid() <= 0) {
        return false;
    }

    int result = ::kill(_shellProcess->pid(), signal);

    if (result == 0) {
        return _shellProcess->waitForFinished(1000);
    } else {
        return false;
    }
}

void Session::close()
{
    if (isRunning()) {
        if (!closeInNormalWay()) {
            closeInForceWay();
        }
    } else {
        // terminal process has finished, just close the session
        QTimer::singleShot(1, this, &terminal::Session::finished);
    }
}

bool Session::closeInNormalWay()
{
    _autoClose    = true;
    _closePerUserRequest = true;

    // for the possible case where following events happen in sequence:
    //
    // 1). the terminal process crashes
    // 2). the tab stays open and displays warning message
    // 3). the user closes the tab explicitly
    //
    if (!isRunning()) {
        emit finished();
        return true;
    }

    // If only the session's shell is running, try sending an EOF for a clean exit
    if (!isForegroundProcessActive() && _knownShells.contains(QFileInfo(_program).fileName())) {
        _shellProcess->sendEof();

        if (_shellProcess->waitForFinished(1000)) {
            return true;
        }
        qWarning() << "shell did not close, sending SIGHUP";
    }


    // We tried asking nicely, ask a bit less nicely
    if (kill(SIGHUP)) {
        return true;
    } else {
        qWarning() << "Process " << _shellProcess->pid() << " did not die with SIGHUP";
        _shellProcess->closePty();
        return (_shellProcess->waitForFinished(1000));
    }
}

bool Session::closeInForceWay()
{
    _autoClose    = true;
    _closePerUserRequest = true;

    if (kill(SIGKILL)) {
        return true;
    } else {
        qWarning() << "Process " << _shellProcess->pid() << " did not die with SIGKILL";
        return false;
    }
}

void Session::sendTextToTerminal(const QString& text, const QChar& eol) const
{
    if (isReadOnly()) {
        return;
    }

    if (eol.isNull()) {
        _emulation->sendText(text);
    } else {
        _emulation->sendText(text + eol);
    }
}

// Only D-Bus calls this function (via SendText or runCommand)
void Session::sendText(const QString& text) const
{
    if (isReadOnly()) {
        return;
    }

#if !defined(REMOVE_SENDTEXT_RUNCOMMAND_DBUS_METHODS)
    if (show_disallow_certain_dbus_methods_message) {

        KNotification::event(KNotification::Warning, QStringLiteral("terminal D-Bus Warning"),
            i18n("The D-Bus methods sendText/runCommand were just used.  There are security concerns about allowing these methods to be public.  If desired, these methods can be changed to internal use only by re-compiling terminal. <p>This warning will only show once for this terminal instance.</p>"));

        show_disallow_certain_dbus_methods_message = false;
    }
#endif

    _emulation->sendText(text);
}

// Only D-Bus calls this function
void Session::runCommand(const QString& command) const
{
    sendText(command + u'\n');
}

void Session::sendMouseEvent(int buttons, int column, int line, int eventType)
{
    _emulation->sendMouseEvent(buttons, column, line, eventType);
}

void Session::done(int exitCode, QProcess::ExitStatus exitStatus)
{
    // This slot should be triggered only one time
    disconnect(_shellProcess,
               QOverload<int,QProcess::ExitStatus>::of(&terminal::Pty::finished),
               this, &terminal::Session::done);

    if (!_autoClose) {
        _userTitle = i18nc("@info:shell This session is done", "Finished");
        emit sessionAttributeChanged();
        return;
    }

    if (_closePerUserRequest) {
        emit finished();
        return;
    }

    QString message;

    if (exitCode != 0) {
        if (exitStatus != QProcess::NormalExit) {
            message = QString("Program '%1' crashed.").arg(_program);
        } else {
            message = QString("Program '%1' exited with status %2.").arg(_program).arg(exitCode);
        }

//        //FIXME: See comments in Session::silenceTimerDone()
//        KNotification::event(QStringLiteral("Finished"), message , QPixmap(),
//                             QApplication::activeWindow(),
//                             KNotification::CloseWhenWidgetActivated);
    }

    if (exitStatus != QProcess::NormalExit) {
        // this seeming duplicated line is for the case when exitCode is 0
        message = QString("Program '%1' crashed.").arg(_program);
        terminalWarning(message);
    } else {
        emit finished();
    }
}

Emulation* Session::emulation() const
{
    return _emulation;
}

QString Session::keyBindings() const
{
    return _emulation->keyBindings();
}

QStringList Session::environment() const
{
    return _environment;
}

void Session::setEnvironment(const QStringList& environment)
{
    if (isReadOnly()) {
        return;
    }

    _environment = environment;
}

void Session::addEnvironmentEntry(const QString& entry)
{
    _environment << entry;
}

int Session::sessionId() const
{
    return _sessionId;
}

void Session::setKeyBindings(const QString& name)
{
    _emulation->setKeyBindings(name);
}

void Session::setTitle(TitleRole role , const QString& newTitle)
{
    if (title(role) != newTitle) {
        if (role == NameRole) {
            _nameTitle = newTitle;
        } else if (role == DisplayedTitleRole) {
            _displayTitle = newTitle;
        }

        emit sessionAttributeChanged();
    }
}

QString Session::title(TitleRole role) const
{
    if (role == NameRole) {
        return _nameTitle;
    } else if (role == DisplayedTitleRole) {
        return _displayTitle;
    } else {
        return QString();
    }
}

ProcessInfo* Session::getProcessInfo()
{
    ProcessInfo* process = nullptr;

    if (isForegroundProcessActive() && updateForegroundProcessInfo()) {
        process = _foregroundProcessInfo;
    } else {
        updateSessionProcessInfo();
        process = _sessionProcessInfo;
    }

    return process;
}

void Session::updateSessionProcessInfo()
{
    Q_ASSERT(_shellProcess);

    bool ok;
    // The checking for pid changing looks stupid, but it is needed
    // at the moment to workaround the problem that processId() might
    // return 0
    if ((_sessionProcessInfo == nullptr) ||
            (processId() != 0 && processId() != _sessionProcessInfo->pid(&ok))) {
        delete _sessionProcessInfo;
        _sessionProcessInfo = ProcessInfo::newInstance(processId());
        _sessionProcessInfo->setUserHomeDir();
    }
    _sessionProcessInfo->update();
}

bool Session::updateForegroundProcessInfo()
{
    Q_ASSERT(_shellProcess);

    const int foregroundPid = _shellProcess->foregroundProcessGroup();
    if (foregroundPid != _foregroundPid) {
        delete _foregroundProcessInfo;
        _foregroundProcessInfo = ProcessInfo::newInstance(foregroundPid);
        _foregroundPid = foregroundPid;
    }

    if (_foregroundProcessInfo != nullptr) {
        _foregroundProcessInfo->update();
        return _foregroundProcessInfo->isValid();
    } else {
        return false;
    }
}

bool Session::isRemote()
{
    ProcessInfo* process = getProcessInfo();

    bool ok = false;
    return (process->name(&ok) == QLatin1String("ssh") && ok);
}

QString Session::getDynamicTitle()
{

    ProcessInfo* process = getProcessInfo();

    // format tab titles using process info
    bool ok = false;
    if (process->name(&ok) == QLatin1String("ssh") && ok) {
        SSHProcessInfo sshInfo(*process);
        return sshInfo.format(tabTitleFormat(Session::RemoteTabTitle));
    }

    /*
     * Parses an input string, looking for markers beginning with a '%'
     * character and returns a string with the markers replaced
     * with information from this process description.
     * <br>
     * The markers recognized are:
     * <ul>
     * <li> %B - User's Bourne prompt sigil ($, or # for superuser). </li>
     * <li> %u - Name of the user which owns the process. </li>
     * <li> %n - Replaced with the name of the process.   </li>
     * <li> %d - Replaced with the last part of the path name of the
     *      process' current working directory.
     *
     *      (eg. if the current directory is '/home/bob' then
     *      'bob' would be returned)
     * </li>
     * <li> %D - Replaced with the current working directory of the process. </li>
     * </ul>
     */
    QString title = tabTitleFormat(Session::LocalTabTitle);
    // search for and replace known marker

    int UID = process->userId(&ok);
    if(!ok) {
        title.replace(u"%B"_qs, QStringLiteral("-"));
    } else {
        //title.replace(QLatin1String("%I"), QString::number(UID));
        if (UID == 0) {
            title.replace(u"%B"_qs, QStringLiteral("#"));
        } else {
            title.replace(u"%B"_qs, QStringLiteral("$"));
        }
    }


    title.replace(u"%u"_qs, process->userName());
    title.replace(u"%h"_qs, terminal::ProcessInfo::localHost());
    title.replace(u"%n"_qs, process->name(&ok));

    QString dir = _reportedWorkingUrl.toLocalFile();
    ok = true;
    if (dir.isEmpty()) {
        // update current directory from process
        updateWorkingDirectory();
        dir = process->currentDir(&ok);
    }
    if(!ok) {
        title.replace(u"%d"_qs, QStringLiteral("-"));
        title.replace(u"%D"_qs, QStringLiteral("-"));
    } else {
        // allow for shortname to have the ~ as homeDir
        const QString homeDir = process->userHomeDir();
        if (!homeDir.isEmpty()) {
            if (dir.startsWith(homeDir)) {
                dir.remove(0, homeDir.length());
                dir.prepend(u'~');
            }
        }
        title.replace(u"%D"_qs, dir);
        title.replace(u"%d"_qs, process->formatShortDir(dir));
    }

    return title;
}

QUrl Session::getUrl()
{
    if (_reportedWorkingUrl.isValid()) {
        return _reportedWorkingUrl;
    }

    QString path;

    updateSessionProcessInfo();
    if (_sessionProcessInfo->isValid()) {
        bool ok = false;

        // check if foreground process is bookmark-able
        if (isForegroundProcessActive() && _foregroundProcessInfo->isValid()) {
            // for remote connections, save the user and host
            // bright ideas to get the directory at the other end are welcome :)
            if (_foregroundProcessInfo->name(&ok) == QLatin1String("ssh") && ok) {
                SSHProcessInfo sshInfo(*_foregroundProcessInfo);

                QUrl url;
                url.setScheme(QStringLiteral("ssh"));
                url.setUserName(sshInfo.userName());
                url.setHost(sshInfo.host());

                const QString port = sshInfo.port();
                if (!port.isEmpty() && port != QLatin1String("22")) {
                    url.setPort(port.toInt());
                }
                return url;
            } else {
                path = _foregroundProcessInfo->currentDir(&ok);
                if (!ok) {
                    path.clear();
                }
            }
        } else { // otherwise use the current working directory of the shell process
            path = _sessionProcessInfo->currentDir(&ok);
            if (!ok) {
                path.clear();
            }
        }
    }

    return QUrl::fromLocalFile(path);
}

void Session::setIconName(const QString& iconName)
{
    if (iconName != _iconName) {
        _iconName = iconName;
        emit sessionAttributeChanged();
    }
}

void Session::setIconText(const QString& iconText)
{
    _iconText = iconText;
}

QString Session::iconName() const
{
    return isReadOnly() ? QStringLiteral("object-locked") : _iconName;
}

QString Session::iconText() const
{
    return _iconText;
}

void Session::setHistoryType(const HistoryType& hType)
{
    _emulation->setHistory(hType);
}

const HistoryType& Session::historyType() const
{
    return _emulation->history();
}

void Session::clearHistory()
{
    _emulation->clearHistory();
}

QStringList Session::arguments() const
{
    return _arguments;
}

QString Session::program() const
{
    return _program;
}

bool Session::isMonitorActivity() const
{
    return _monitorActivity;
}
bool Session::isMonitorSilence()  const
{
    return _monitorSilence;
}

void Session::setMonitorActivity(bool monitor)
{
    if (_monitorActivity == monitor) {
        return;
    }

    _monitorActivity  = monitor;
    _notifiedActivity = false;

    // This timer is meaningful only after activity has been notified
    _activityTimer->stop();

    activityStateSet(NOTIFYNORMAL);
}

void Session::setMonitorSilence(bool monitor)
{
    if (_monitorSilence == monitor) {
        return;
    }

    _monitorSilence = monitor;
    if (_monitorSilence) {
        _silenceTimer->start(_silenceSeconds * 1000);
    } else {
        _silenceTimer->stop();
    }

    activityStateSet(NOTIFYNORMAL);
}

void Session::setMonitorSilenceSeconds(int seconds)
{
    _silenceSeconds = seconds;
    if (_monitorSilence) {
        _silenceTimer->start(_silenceSeconds * 1000);
    }
}

void Session::setAddToUtmp(bool add)
{
    _addToUtmp = add;
}

void Session::setAutoClose(bool close)
{
    _autoClose = close;
}

bool Session::autoClose() const
{
    return _autoClose;
}

void Session::setFlowControlEnabled(bool enabled)
{
    if (isReadOnly()) {
        return;
    }

    _flowControlEnabled = enabled;

    if (_shellProcess != nullptr) {
        _shellProcess->setFlowControlEnabled(_flowControlEnabled);
    }

    emit flowControlEnabledChanged(enabled);
}
bool Session::flowControlEnabled() const
{
    if (_shellProcess != nullptr) {
        return _shellProcess->flowControlEnabled();
    } else {
        return _flowControlEnabled;
    }
}

void Session::onReceiveBlock(const char* buf, int len)
{
    emit receiveBlock(buf, len);
    _emulation->receiveData(buf, len);
}

QSize Session::size()
{
    return _emulation->imageSize();
}

void Session::setSize(const QSize& size)
{
    if ((size.width() <= 1) || (size.height() <= 1)) {
        return;
    }

    emit resizeRequest(size);
}

QSize Session::preferredSize() const
{
    return _preferredSize;
}

void Session::setPreferredSize(const QSize& size)
{
    _preferredSize = size;
}

int Session::processId() const
{
    return _shellProcess->pid();
}

void Session::setTitle(int role , const QString& title)
{
    switch (role) {
    case 0:
        setTitle(Session::NameRole, title);
        break;
    case 1:
        setTitle(Session::DisplayedTitleRole, title);

        // without these, that title will be overridden by the expansion of
        // title format shortly after, which will confuses users.
        _localTabTitleFormat = title;
        _remoteTabTitleFormat = title;

        break;
    }
}

QString Session::title(int role) const
{
    switch (role) {
    case 0:
        return title(Session::NameRole);
    case 1:
        return title(Session::DisplayedTitleRole);
    default:
        return QString();
    }
}

void Session::setTabTitleFormat(int context , const QString& format)
{
    switch (context) {
    case 0:
        setTabTitleFormat(Session::LocalTabTitle, format);
        break;
    case 1:
        setTabTitleFormat(Session::RemoteTabTitle, format);
        break;
    }
}

QString Session::tabTitleFormat(int context) const
{
    switch (context) {
    case 0:
        return tabTitleFormat(Session::LocalTabTitle);
    case 1:
        return tabTitleFormat(Session::RemoteTabTitle);
    default:
        return QString();
    }
}

void Session::setHistorySize(int lines)
{
    if (isReadOnly()) {
        return;
    }

    if (lines < 0) {
        setHistoryType(HistoryTypeFile());
    } else if (lines == 0) {
        setHistoryType(HistoryTypeNone());
    } else {
        setHistoryType(CompactHistoryType(lines));
    }
}

int Session::historySize() const
{
    const HistoryType& currentHistory = historyType();

    if (currentHistory.isEnabled()) {
        if (currentHistory.isUnlimited()) {
            return -1;
        } else {
            return currentHistory.maximumLineCount();
        }
    } else {
        return 0;
    }
}

QString Session::profile()
{
    return SessionManager::instance()->sessionProfile(this)->name();
}

void Session::setProfile(const QString &profileName)
{
  const QList<Profile::Ptr> profiles = ProfileManager::instance()->allProfiles();
  foreach (const Profile::Ptr &profile, profiles) {
    if (profile->name() == profileName) {
      SessionManager::instance()->setSessionProfile(this, profile);
    }
  }
}

int Session::foregroundProcessId()
{
    int pid;

    bool ok = false;
    pid = getProcessInfo()->pid(&ok);
    if (!ok) {
        pid = -1;
    }

    return pid;
}

bool Session::isForegroundProcessActive()
{
    // foreground process info is always updated after this
    return (_shellProcess->pid() != _shellProcess->foregroundProcessGroup());
}

QString Session::foregroundProcessName()
{
    QString name;

    if (updateForegroundProcessInfo()) {
        bool ok = false;
        name = _foregroundProcessInfo->name(&ok);
        if (!ok) {
            name.clear();
        }
    }

    return name;
}

void Session::saveSession(KConfigGroup& group)
{
    group.writePathEntry("WorkingDir", currentWorkingDirectory());
    group.writeEntry("LocalTab",       tabTitleFormat(LocalTabTitle));
    group.writeEntry("RemoteTab",      tabTitleFormat(RemoteTabTitle));
    group.writeEntry("SessionGuid",    _uniqueIdentifier.toString());
    group.writeEntry("Encoding",       QString::fromUtf8(codec()));
}

void Session::restoreSession(KConfigGroup& group)
{
    QString value;

    value = group.readPathEntry("WorkingDir", QString());
    if (!value.isEmpty()) {
        setInitialWorkingDirectory(value);
    }
    value = group.readEntry("LocalTab");
    if (!value.isEmpty()) {
        setTabTitleFormat(LocalTabTitle, value);
    }
    value = group.readEntry("RemoteTab");
    if (!value.isEmpty()) {
        setTabTitleFormat(RemoteTabTitle, value);
    }
    value = group.readEntry("SessionGuid");
    if (!value.isEmpty()) {
        _uniqueIdentifier = QUuid(value);
    }
    value = group.readEntry("Encoding");
    if (!value.isEmpty()) {
        setCodec(value.toUtf8());
    }
}

QString Session::validDirectory(const QString& dir) const
{
    QString validDir = dir;
    if (validDir.isEmpty()) {
        validDir = QDir::currentPath();
    }

    const QFileInfo fi(validDir);
    if (!fi.exists() || !fi.isDir()) {
        validDir = QDir::homePath();
    }

    return validDir;
}

bool Session::isReadOnly() const
{
    return _readOnly;
}

void Session::setReadOnly(bool readOnly)
{
    if (_readOnly != readOnly) {
        _readOnly = readOnly;

        // Needed to update the tab icons and all
        // attached views.
        emit readOnlyChanged();
    }
}

SessionGroup::SessionGroup(QObject* parent)
    : QObject(parent), _masterMode(0)
{
}
SessionGroup::~SessionGroup() = default;

int SessionGroup::masterMode() const
{
    return _masterMode;
}
QList<Session*> SessionGroup::sessions() const
{
    return _sessions.keys();
}
bool SessionGroup::masterStatus(Session* session) const
{
    return _sessions[session];
}

void SessionGroup::addSession(Session* session)
{
    connect(session, &terminal::Session::finished, this, &terminal::SessionGroup::sessionFinished);
    _sessions.insert(session, false);
}
void SessionGroup::removeSession(Session* session)
{
    disconnect(session, &terminal::Session::finished, this, &terminal::SessionGroup::sessionFinished);
    setMasterStatus(session, false);
    _sessions.remove(session);
}
void SessionGroup::sessionFinished()
{
    auto* session = qobject_cast<Session*>(sender());
    Q_ASSERT(session);
    removeSession(session);
}
void SessionGroup::setMasterMode(int mode)
{
    _masterMode = mode;
}
QList<Session*> SessionGroup::masters() const
{
    return _sessions.keys(true);
}
void SessionGroup::setMasterStatus(Session* session , bool master)
{
    const bool wasMaster = _sessions[session];

    if (wasMaster == master) {
        // No status change -> nothing to do.
        return;
    }
    _sessions[session] = master;

    if (master) {
        connect(session->emulation(), &terminal::Emulation::sendData, this, &terminal::SessionGroup::forwardData);
    } else {
        disconnect(session->emulation(), &terminal::Emulation::sendData,
                   this, &terminal::SessionGroup::forwardData);
    }
}
void SessionGroup::forwardData(const QByteArray& data)
{
    static bool _inForwardData = false;
    if (_inForwardData) {  // Avoid recursive calls among session groups!
        // A recursive call happens when a master in group A calls forwardData()
        // in group B. If one of the destination sessions in group B is also a
        // master of a group including the master session of group A, this would
        // again call forwardData() in group A, and so on.
        return;
    }

    _inForwardData = true;
    const QList<Session*> sessionsKeys = _sessions.keys();
    for (auto other : sessionsKeys)
    {
        if (!_sessions[other]) {
            other->emulation()->sendString(data);
        }
    }
    _inForwardData = false;
}

