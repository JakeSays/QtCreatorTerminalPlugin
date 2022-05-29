/*
 * Copyright (C) 2012 Adam Treat. All rights reserved.
 */

#include "TerminalOutputPane.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/icontext.h>
#include <extensionsystem/pluginmanager.h>
#include <texteditor/fontsettings.h>
#include <texteditor/texteditorsettings.h>
#include <utils/environment.h>
#include <utils/fileutils.h>
#include <projectexplorer/projectexplorer.h>
#include <ProfileManager.h>

#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QVBoxLayout>

#include <TerminalDisplay.h>

using ProjectExplorer::ProjectExplorerPlugin;

namespace terminal {

TerminalOutputPane::TerminalOutputPane(QObject *parent)
   : IOutputPane(parent)
   , _window(0)
{
    Core::Context context("Terminal.Window");
}

QWidget *TerminalOutputPane::outputWidget(QWidget *parent)
{
    if (!_window)
        _window = new TerminalWindow(parent);
    connect(_window, &TerminalWindow::initialized,
            this, &TerminalOutputPane::termInitialized);
    return _window;
}

QList<QWidget *> TerminalOutputPane::toolBarWidgets() const
{
    return QList<QWidget *>();
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
    if(_window == nullptr)
    {
        return;
    }
    _window->ClearDisplay();
}

void TerminalOutputPane::visibilityChanged(bool visible)
{
    static bool initialized = false;
    if (_window == nullptr || !visible || initialized)
        return;

    _window->initialze();
    initialized = true;
}

void TerminalOutputPane::termInitialized()
{
    //moved to Session
//    if (Core::IDocument *doc = Core::EditorManager::instance()->currentDocument()) {
//        const QDir dir = doc->filePath().toFileInfo().absoluteDir();
//        if (dir.exists())
//            _window->termWidget()->setWorkingDirectory(dir.canonicalPath());
//    }
}

void TerminalOutputPane::setFocus()
{
    if (_window == nullptr)
        return;
    _window->setFocus(Qt::OtherFocusReason);
}

bool TerminalOutputPane::hasFocus() const
{
    if (_window == nullptr)
        return false;
    return _window->hasFocus();
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
