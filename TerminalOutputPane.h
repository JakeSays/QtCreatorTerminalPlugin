/*
 * Copyright (C) 2012 Adam Treat. All rights reserved.
 */

#pragma once

#include "TerminalWindow.h"
#include <QVector>
#include <coreplugin/outputwindow.h>
#include <coreplugin/ioutputpane.h>

#include <memory>

class QLabel;
class QVBoxLayout;
class QToolButton;
class QTabWidget;
class QWidget;
class QAction;

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
    void AddTab();
    void ActiveTabChanged(int index);
    void ShowContextMenu(const QPoint &pos);

private:
    std::unique_ptr<QTabWidget> _tabs;
    TerminalWindow* _activeWindow;
    QToolButton* _addButton;
    QVector<TerminalWindow*> _windows;

    QAction* _closeCurrentAction;
    QAction* _closeAllAction;
    QAction* _closeOtherAction;

    void CreateControls();
    bool CloseTab(int index);
    void CloseAllTabs(int except = -1);
    int WindowIndex(TerminalWindow* window);
    void UpdateCloseState();
};

} // namespace terminal
