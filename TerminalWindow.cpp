#include "TerminalWindow.h"
#include "TerminalDisplay.h"
#include "ProfileManager.h"
#include "Session.h"
#include "SessionManager.h"
#include "ColorSchemeManager.h"

#include <utils/environment.h>
#include <projectexplorer/session.h>
#include <projectexplorer/project.h>
#include <utils/fileutils.h>

using namespace terminal;


TerminalWindow::TerminalWindow(QWidget *parent)
    : QWidget(parent)
      , _parent(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    _copyAction = new QAction("Copy", this);
    _copyAction->setShortcut(QKeySequence::Copy);
    connect(_copyAction, &QAction::triggered, this, &TerminalWindow::onCopyAction);
    addAction(_copyAction);

    _pasteAction = new QAction("Paste", this);
    _pasteAction->setShortcut(QKeySequence::Paste);
    connect(_pasteAction, &QAction::triggered, this, &TerminalWindow::onPasteAction);
    addAction(_pasteAction);
}

static
TerminalDisplay* createTerminalDisplay(Session *session)
{
    auto display = new TerminalDisplay(session);
    display->setRandomSeed(uint(session->sessionId() | (qApp->applicationPid() << 10)));

    return display;
}

void TerminalWindow::CreateDisplay()
{
    if (_display)
    {
        delete _display;
    }

    auto profile = ProfileManager::instance()->defaultProfile();

    Q_ASSERT(profile);

    _session = SessionManager::instance()->createSession(profile);

    auto startupProject = ProjectExplorer::SessionManager::startupProject();

    if (startupProject != nullptr)
    {
        _session->setInitialWorkingDirectory(startupProject->rootProjectDirectory().toString());
    }

    auto display = createTerminalDisplay(_session);
    connect(display, &terminal::TerminalDisplay::configureRequest,
        this, &TerminalWindow::contextMenuRequested);

    display->applyProfile(profile);

    // set initial size
    const QSize &preferredSize = _session->preferredSize();

    display->setSize(preferredSize.width(), preferredSize.height());

    _session->addView(display);

    // tell the session whether it has a light or dark background
    _session->setDarkBackground(ColorSchemeManager::instance()->colorSchemeForProfile(profile)->hasDarkBackground());
    display->setFocus(Qt::OtherFocusReason);

    _display = display;
}

void TerminalWindow::initialze()
{
    if (_layout)
    {
        delete _layout;
    }

    CreateDisplay();

    _display->setScrollBarPosition(Enum::ScrollBarRight);

    setFocusProxy(_display);

    _layout = new QVBoxLayout;
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);
    _layout->addWidget(_display);
    setLayout(_layout);

    _session->run();

    emit initialized();
}

void TerminalWindow::ClearDisplay()
{
    _display->sendStringToEmu("clear\n");
}

void TerminalWindow::contextMenuRequested(const QPoint &point, SpotType spotType)
{
    QPoint globalPos = mapToGlobal(point);
    QMenu menu;

    if (spotType == SpotType::FileLink)
    {
        auto actions = _display->filterActions(point);
        menu.addActions(actions);
        auto sep = new QAction(&menu);
        sep->setSeparator(true);
        menu.addAction(sep);
    }
    else
    {
        _copyAction->setEnabled(_display->screenWindow()->hasSelection());
        _pasteAction->setEnabled(QApplication::clipboard()->mimeData()->hasText());
        menu.addAction(_copyAction);
        menu.addAction(_pasteAction);
    }

    menu.exec(globalPos);
}

void TerminalWindow::onCopyAction()
{
    _display->copyToClipboard();
}

void TerminalWindow::onPasteAction()
{
    _display->pasteFromClipboard();
}

void TerminalWindow::copyAvailable(bool available)
{
    _copyAction->setEnabled(available);
}

void TerminalWindow::closeInvoked()
{
    _display->sendStringToEmu("exit\n");
}

void TerminalWindow::finishedInvoked()
{
    initialze();
}
