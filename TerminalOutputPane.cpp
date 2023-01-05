/*
 * Copyright (C) 2012 Adam Treat. All rights reserved.
 */

#include "TerminalOutputPane.h"
#include "TerminalWindow.h"
#include "qtabwidget.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/icontext.h>
#include <extensionsystem/pluginmanager.h>
#include <texteditor/fontsettings.h>
#include <texteditor/texteditorsettings.h>
#include <utils/environment.h>
#include <utils/fileutils.h>
#include <utils/icon.h>
#include <utils/utilsicons.h>
#include <utils/algorithm.h>
#include <projectexplorer/projectexplorer.h>
#include <ProfileManager.h>

#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QVBoxLayout>
#include <QTabWidget>

#include <TerminalDisplay.h>

using ProjectExplorer::ProjectExplorerPlugin;

namespace terminal
{

namespace
{
template<typename TContainer, typename Predicate>
int IndexOf(const TContainer& container, Predicate function)
{
    typename TContainer::const_iterator begin = std::begin(container);
    typename TContainer::const_iterator end = std::end(container);

    typename TContainer::const_iterator it = std::find_if(begin, end, function);
    return it == end ? -1 : std::distance(begin, it);
}

inline
TerminalWindow* AsTerminal(QWidget* widget)
{
    return reinterpret_cast<TerminalWindow*>(widget);
}
}

TerminalOutputPane::TerminalOutputPane(QObject *parent)
   : IOutputPane(parent),
    _tabs(nullptr),
    _activeWindow(nullptr),
    _nextTerminalNumber(1),
    _delayCloseTimer(this),
    _closeCurrentAction(new QAction("Close Tab", this)),
    _closeAllAction(new QAction("Close All Tabs", this)),
    _closeOtherAction(new QAction("Close Other Tabs", this))
{
    Core::Context context("Terminal.Window");

    CreateControls();

    _delayCloseTimer.setSingleShot(true);
}

QWidget *TerminalOutputPane::outputWidget(QWidget *parent)
{
    if (!_tabs)
    {
        _tabs.reset(new QTabWidget);
        auto tabs = _tabs.get();
        tabs->setDocumentMode(true);
        tabs->setTabsClosable(true);
        tabs->setMovable(true);
        tabs->setTabShape(QTabWidget::TabShape::Rounded);
        tabs->setContextMenuPolicy(Qt::CustomContextMenu);

        connect(tabs, &QTabWidget::tabCloseRequested, this, [this](int index) { CloseTab(index); });
        connect(tabs, &QTabWidget::currentChanged, this, &TerminalOutputPane::ActiveTabChanged);
        connect(tabs, &QTabWidget::customContextMenuRequested, this, &TerminalOutputPane::ShowContextMenu);

        AddTab();
    }

    return _tabs.get();
}

QList<QWidget *> TerminalOutputPane::toolBarWidgets() const
{
    return {_addButton};
}

QString TerminalOutputPane::displayName() const
{
    return tr("Terminal");
}

int TerminalOutputPane::priorityInStatusBar() const
{
    return 50;
}

void TerminalOutputPane::clearContents()
{
    if(_activeWindow == nullptr)
    {
        return;
    }
    _activeWindow->ClearDisplay();
}

void TerminalOutputPane::visibilityChanged(bool visible)
{
    if (visible && _windows.isEmpty())
    {
        AddTab();
    }
}

void TerminalOutputPane::termInitialized()
{
}

void TerminalOutputPane::AddTab()
{
    auto id = _nextTerminalNumber++;
    auto title = QString::asprintf("Terminal %d", id);

    auto newWindow = new TerminalWindow(_tabs.get(), title, id);
    newWindow->initialze();    
    _windows.insert(id, newWindow);

    auto newWindowIndex = _tabs->addTab(newWindow, title);
    _tabs->setCurrentIndex(newWindowIndex);
    newWindow->setTabIndex(newWindowIndex);

    connect(newWindow, &TerminalWindow::sessionEnded, this, &TerminalOutputPane::SessionEnded);

    UpdateCloseState();
}

void TerminalOutputPane::SessionEnded(int terminalId)
{
    auto pos = _windows.find(terminalId);
    if (pos == _windows.end())
    {
        return;
    }

    auto terminal = pos.value();
    disconnect(terminal);

    QTimer::singleShot(200, this, [this, idx = terminal->tabIndex()]
        {
            CloseTab(idx);
        });
}

void TerminalOutputPane::ActiveTabChanged(int index)
{
    if (index < 0)
    {
        _activeWindow = nullptr;
        return;
    }
    _activeWindow = AsTerminal(_tabs->currentWidget());
    _activeWindow->setFocus(Qt::OtherFocusReason);
}

void TerminalOutputPane::CreateControls()
{
    _addButton = new QToolButton;
    _addButton->setIcon(Utils::Icons::PLUS_TOOLBAR.icon());
    _addButton->setToolTip(tr("Add Instance"));
    _addButton->setEnabled(true);

    connect(_addButton, &QToolButton::clicked, this, &TerminalOutputPane::AddTab);
}

bool TerminalOutputPane::CloseTab(int index)
{
    QTC_ASSERT(index != -1, return true);

    auto terminal = AsTerminal(_tabs->widget(index));

    _windows.remove(terminal->Id());

    _tabs->removeTab(index);

    delete terminal;

    UpdateCloseState();

    if (_windows.isEmpty())
    {
        hide();
    }

    return true;
}

void TerminalOutputPane::CloseAllTabs(int except)
{
    for (auto index = _tabs->count() - 1; index >= 0; index--)
    {
        if (index != except)
        {
            CloseTab(index);
        }
    }
}

void TerminalOutputPane::UpdateCloseState()
{
    auto tabCount = _tabs->count();
    _closeCurrentAction->setEnabled(tabCount > 0);
    _closeAllAction->setEnabled(tabCount > 0);
    _closeOtherAction->setEnabled(tabCount > 1);
}

void TerminalOutputPane::ShowContextMenu(const QPoint& pos)
{
    if (_activeWindow == nullptr)
    {
        return;
    }

    auto index = _tabs->tabBar()->tabAt(pos);
    _closeCurrentAction->setEnabled(index >= 0);
    _closeOtherAction->setEnabled(index >= 0);

    QList<QAction*> actions {_closeCurrentAction, _closeAllAction, _closeOtherAction};
    auto action = QMenu::exec(actions, _tabs->mapToGlobal(pos), nullptr, _tabs.get());

    if (action == _closeCurrentAction)
    {
        CloseTab(index);
    }
    else if (action == _closeAllAction)
    {
        CloseAllTabs();
    }
    else if (action == _closeOtherAction)
    {
        CloseAllTabs(index);
    }
}

void TerminalOutputPane::setFocus()
{
    if (_activeWindow == nullptr)
    {
        return;
    }
    _activeWindow->setFocus(Qt::OtherFocusReason);
}

bool TerminalOutputPane::hasFocus() const
{
    if (_activeWindow == nullptr)
    {
        return false;
    }
    return _activeWindow->hasFocus();
}

bool TerminalOutputPane::canFocus() const
{
    return true;
}

bool TerminalOutputPane::canNavigate() const
{
    return false;
}

bool TerminalOutputPane::canNext() const
{
    return false;
}

bool TerminalOutputPane::canPrevious() const
{
    return false;
}

void TerminalOutputPane::goToNext()
{
    // no-op
}

void TerminalOutputPane::goToPrev()
{
    // no-op
}

} // namespace terminal
