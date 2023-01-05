/*
 * Copyright (C) 2012 Adam Treat. All rights reserved.
 */

#pragma once

#include "TerminalOutputPane.h"

#include <extensionsystem/iplugin.h>

namespace terminal {

class TerminalWindow;

class TerminalPlugin
  : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "TerminalPlugin.json")

public:
    TerminalPlugin();
    ~TerminalPlugin();

    bool initialize(const QStringList &arguments, QString *errorMessage);
    void extensionsInitialized();

private:
    TerminalOutputPane* _outputPane;
};

} // namespace Terminal
