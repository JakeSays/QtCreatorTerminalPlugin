/*
 * Copyright (C) 2012 Adam Treat. All rights reserved.
 */

#pragma once

#include "TerminalWindow.h"

#include <coreplugin/outputwindow.h>
#include <coreplugin/ioutputpane.h>

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QVBoxLayout)

namespace terminal
{
class TerminalOutputPane: public Core::IOutputPane
{
    Q_OBJECT

public:
    TerminalOutputPane(QObject* parent = 0);

    QWidget* outputWidget(QWidget* parent) override;
    QList<QWidget*> toolBarWidgets() const override;
    QString displayName() const override;
    int priorityInStatusBar() const override;
    void clearContents() override;
    void visibilityChanged(bool visible) override;
    void setFocus() override;
    bool hasFocus() const override;
    bool canFocus() const override;
    bool canNavigate() const override;
    bool canNext() const override;
    bool canPrevious() const override;
    void goToNext() override;
    void goToPrev() override;

private slots:
    void termInitialized();

private:
    TerminalWindow* _window;
};

} // namespace terminal
