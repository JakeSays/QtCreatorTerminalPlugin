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
    TerminalWindow(QWidget *parent, QString title = QString(), int id = -1);
    void initialze();

    TerminalDisplay *Display() const { return _display; }
    QString Title() const { return _title; }
    int Id() const { return _id; }

    Session* session() const { return _session; }

    void ClearDisplay();

    int tabIndex() const;
    void setTabIndex(int newTabIndex);

signals:
    void initialized();
    void sessionEnded(int terminalId);

private:
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

    void SetActionState(bool enable);

    void InstallActions();
    void RemoveActions();

private slots:
    void contextMenuRequested(const QPoint& location, SpotType spotType);
    void copyAvailable(bool);
    void onCopyAction();
    void onPasteAction();
    void closeInvoked();
    void finishedInvoked();
    void displayFocusLost();
    void displayFocusGained();

private:
    void CreateDisplay();

    QString _title;
    int _id;
    int _tabIndex;
    Session* _session = nullptr;
    QVBoxLayout *_layout = nullptr;
    TerminalDisplay *_display = nullptr;
    QAction *_copyAction = nullptr;
    QAction *_pasteAction = nullptr;
    QAction *_closeAction = nullptr;
    QWidget *_parent = nullptr;
};
} //terminal
