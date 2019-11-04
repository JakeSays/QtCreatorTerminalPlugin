/*
 * Copyright (C) 2012 Adam Treat. All rights reserved.
 */

#include "TerminalPlugin.h"
#include "TerminalOutputPane.h"
#include "TerminalWindow.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icore.h>
#include <coreplugin/imode.h>
#include <coreplugin/modemanager.h>
#include <coreplugin/id.h>
#include <extensionsystem/pluginmanager.h>

#include <QtCore/QDebug>
#include <QtCore/QtPlugin>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>

static void InitResources()
{
    static bool initialized = false;

    if (initialized)
    {
        return;
    }

    initialized = true;

    Q_INIT_RESOURCE(resources);
}

namespace terminal
{

TerminalPlugin::TerminalPlugin()
    : _outputPane(nullptr)
{
}

TerminalPlugin::~TerminalPlugin()
{
    ExtensionSystem::PluginManager::instance()->removeObject(_outputPane);
    delete _outputPane;
    _outputPane = nullptr;
}

bool TerminalPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)

    InitResources();

    _outputPane = new TerminalOutputPane(this);

    ExtensionSystem::PluginManager::instance()->addObject(_outputPane);

    return true;
}

void TerminalPlugin::extensionsInitialized()
{
}

} // namespace Terminal
