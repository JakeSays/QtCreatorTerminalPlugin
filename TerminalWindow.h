#pragma once

#include <QtGlobal>
#include <QtWidgets>
#include <Filter.h>

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QVBoxLayout)

namespace terminal
{
class TerminalDisplay;
class Session;

class TerminalWindow : public QWidget
{
    Q_OBJECT

public:
    TerminalWindow(QWidget *parent);
    void initialze();

    TerminalDisplay *Display() const { return _display; }

    Session* session() const { return _session; }

    void ClearDisplay();

signals:
    void initialized();

private slots:
    void contextMenuRequested(const QPoint& location, SpotType spotType);
    void copyAvailable(bool);
    void onCopyAction();
    void onPasteAction();
    void closeInvoked();
    void finishedInvoked();

    //    void OnCompilerLine(const terminal::LineDetails &deets);

private:
    void CreateDisplay();

    Session* _session = nullptr;
    QVBoxLayout *_layout = nullptr;
    TerminalDisplay *_display = nullptr;
    QAction *_copyAction = nullptr;
    QAction *_pasteAction = nullptr;
    QAction *_closeAction = nullptr;
    QWidget *_parent = nullptr;
};
} //terminal
