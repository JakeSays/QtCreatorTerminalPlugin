TEMPLATE = lib
QT     += core gui widgets xml network qml
CONFIG += c++17

QMAKE_CXXFLAGS += \
    -std=c++17 \
    -Wno-unused-parameter \
    -Wno-unused-variable \
    -Wno-deprecated-declarations

LIBS += -lutil

DEFINES += \
    TERMINALPLUGIN_LIBRARY \
    qtermwidget5_EXPORT \
    qtermwidget5_EXPORTS \
    HAVE_POSIX_OPENPT \
    HAVE_SYS_TIME_H \
    HAVE_UPDWTMPX \
    COLORSCHEMES_DIR="\\\"/usr/local/share/qtermwidget5/color-schemes\\\"" \
    KB_LAYOUT_DIR="\\\"/usr/local/share/qtermwidget5/kb-layouts\\\"" \
    TRANSLATIONS_DIR="\\\"/usr/local/share/qtermwidget5/translations\\\""

#to remove the necessity of building a custom creator libProjectExplorer.so
#  uncomment this line
#DEFINES += PLUGIN_DISABLE_FINDFILEINSESSION

INCLUDEPATH += \
    $$PWD \
    $$PWD/kui \
    $$PWD/settings \
    $$PWD/ki18n
#    /apps/Qt5.13.1/5.13.1/gcc_64/include/QtNetwork
#    /opt/clang9-x64-rel/include/c++/v1/

HEADERS += \
    CharacterWidth.h \
    ColorSchemeManager.h \
    Enumeration.h \
    ExtendedCharTable.h \
#    IncrementalSearchBar.h \
    KPtyProcess.h \
    KeyboardTranslatorManager.h \
    LineBlockCharacters.h \
    ProcessInfo.h \
    Profile.h \
    ProfileManager.h \
    ProfileReader.h \
    ProfileWriter.h \
    PtyConfig.h \
    ScrollState.h \
    TerminalConfig.h \
    TerminalDisplayAccessible.h \
    SessionManager.h \
    TerminalDebug.h \
    TerminalOutputPane.h \
    TerminalPlugin.h \
    TerminalWindow.h \
    config/bufferfragment_p.h \
    config/config-kconfig.h \
    config/conversioncheck.h \
    config/kauthorized.h \
    config/kconfig.h \
    config/kconfig_core_log_settings.h \
    config/kconfig_p.h \
    config/kconfig_version.h \
    config/kconfigbackend_p.h \
    config/kconfigbase.h \
    config/kconfigbase_p.h \
    config/kconfigdata.h \
    config/kconfiggroup.h \
    config/kconfiggroup_p.h \
    config/kconfigini_p.h \
    config/kconfigwatcher.h \
    config/kcoreconfigskeleton.h \
    config/kcoreconfigskeleton_p.h \
    config/ksharedconfig.h \
    KonsoleSettings.h \
    hsluv.h \
    ki18n/KLocalizedContext \
    ki18n/KLocalizedString \
    ki18n/KLocalizedTranslator \
    ki18n/KuitMarkup \
    ki18n/KuitSetup \
    ki18n/common_helpers_p.h \
    ki18n/config.h \
    ki18n/gettext.h \
    ki18n/kcatalog_p.h \
    ki18n/ki18n_export.h \
    ki18n/ki18n_logging.h \
    ki18n/ki18n_logging_kuit.h \
    ki18n/ki18n_version.h \
    ki18n/klocalizedcontext.h \
    ki18n/klocalizedstring.h \
    ki18n/klocalizedtranslator.h \
    ki18n/ktranscript_export.h \
    ki18n/ktranscript_p.h \
    ki18n/kuitmarkup.h \
    ki18n/kuitmarkup_p.h \
    ki18n/kuitsetup.h \
    kui/KCollapsibleGroupBox \
    kui/KConfigGui \
    kui/KConfigLoader \
    kui/KConfigSkeleton \
    kui/KGuiItem \
    kui/KMessageBox \
    kui/KMessageBoxDontAskAgainInterface \
    kui/KMessageBoxNotifyInterface \
    kui/KMessageWidget \
    kui/KPluralHandlingSpinBox \
    kui/KSqueezedTextLabel \
    kui/KStandardGuiItem \
    kui/fonthelpers_p.h \
    kui/kcollapsiblegroupbox.h \
    kui/kcolorbutton.h \
    kui/kconfigdialog.h \
    kui/kconfigdialogmanager.h \
    kui/kconfiggui.h \
    kui/kconfigloader.h \
    kui/kconfigloader_p.h \
    kui/kconfigloaderhandler_p.h \
    kui/kconfigskeleton.h \
    kui/kconfigwidgets_debug.h \
    kui/kfontaction.h \
    kui/kfontchooser.h \
    kui/kfontrequester.h \
    kui/kfontsizeaction.h \
    kui/kguiitem.h \
    kui/kmessagebox.h \
    kui/kmessagebox_p.h \
    kui/kmessageboxdontaskagaininterface.h \
    kui/kmessageboxnotifyinterface.h \
    kui/kmessagewidget.h \
    kui/kpagedialog.h \
    kui/kpagedialog_p.h \
    kui/kpagemodel.h \
    kui/kpagemodel_p.h \
    kui/kpageview.h \
    kui/kpageview_p.h \
    kui/kpagewidget.h \
    kui/kpagewidget_p.h \
    kui/kpagewidgetmodel.h \
    kui/kpagewidgetmodel_p.h \
    kui/kpluralhandlingspinbox.h \
    kui/kselectaction.h \
    kui/kselectaction_p.h \
    kui/ksqueezedtextlabel.h \
    kui/kstandardguiitem.h \
    kui/ktitlewidget.h \
    kui/loggingcategory.h \
    Character.h \
    CharacterColor.h \
    ColorScheme.h \
    ColorTables.h \
    Emulation.h \
    Filter.h \
    History.h \
    KeyboardTranslator.h \
    LineFont.h \
    Pty.h \
    Screen.h \
    ScreenWindow.h \
    Session.h \
    ShellCommand.h \
    TerminalCharacterDecoder.h \
    TerminalDisplay.h \
    Vt102Emulation.h \
    kprocess.h \
    kpty.h \
    kpty_p.h \
    kptydevice.h \
    profile/ColorSchemeEditor.h \
    profile/EditProfileDialog.h \
    profile/FontDialog.h \
    profile/KeyBindingEditor.h \
    settings/HistorySizeWidget.h

SOURCES += \
    KonsoleSettings.cpp \
    CharacterWidth.cpp \
    ColorSchemeManager.cpp \
    ExtendedCharTable.cpp \
#    IncrementalSearchBar.cpp \
    KeyboardTranslatorManager.cpp \
    LineBlockCharacters.cpp \
    ProcessInfo.cpp \
    Profile.cpp \
    ProfileManager.cpp \
    ProfileReader.cpp \
    ProfileWriter.cpp \
    ScrollState.cpp \
    TerminalDisplayAccessible.cpp \
    SessionManager.cpp \
    TerminalDebug.cpp \
    TerminalOutputPane.cpp \
    TerminalPlugin.cpp \
    TerminalWindow.cpp \
    config/kauthorized.cpp \
    config/kconfig.cpp \
    config/kconfig_core_log_settings.cpp \
    config/kconfigbackend.cpp \
    config/kconfigbase.cpp \
    config/kconfigdata.cpp \
    config/kconfiggroup.cpp \
    config/kconfigini.cpp \
    config/kconfigwatcher.cpp \
    config/kcoreconfigskeleton.cpp \
    config/ksharedconfig.cpp \
    hsluv.c \
    ki18n/common_helpers.cpp \
    ki18n/kcatalog.cpp \
    ki18n/ki18n_logging.cpp \
    ki18n/ki18n_logging_kuit.cpp \
    ki18n/klocalizedcontext.cpp \
    ki18n/klocalizedstring.cpp \
    ki18n/klocalizedtranslator.cpp \
    ki18n/ktranscript.cpp \
    ki18n/kuitmarkup.cpp \
    ki18n/main.cpp \
    kui/fonthelpers.cpp \
    kui/kcollapsiblegroupbox.cpp \
    kui/kcolorbutton.cpp \
    kui/kconfigdialog.cpp \
    kui/kconfigdialogmanager.cpp \
    kui/kconfiggroupgui.cpp \
    kui/kconfiggui.cpp \
    kui/kconfigloader.cpp \
    kui/kconfigskeleton.cpp \
    kui/kconfigwidgets_debug.cpp \
    kui/kfontaction.cpp \
    kui/kfontchooser.cpp \
    kui/kfontrequester.cpp \
    kui/kfontsizeaction.cpp \
    kui/kguiitem.cpp \
    kui/kmessagebox.cpp \
    kui/kmessagebox_p.cpp \
    kui/kmessagewidget.cpp \
    kui/kpagedialog.cpp \
    kui/kpagemodel.cpp \
    kui/kpageview.cpp \
    kui/kpageview_p.cpp \
    kui/kpagewidget.cpp \
    kui/kpagewidgetmodel.cpp \
    kui/kpluralhandlingspinbox.cpp \
    kui/kselectaction.cpp \
    kui/ksqueezedtextlabel.cpp \
    kui/kstandardguiitem.cpp \
    kui/ktitlewidget.cpp \
    kui/loggingcategory.cpp \
    ColorScheme.cpp \
    Emulation.cpp \
    Filter.cpp \
    History.cpp \
    KeyboardTranslator.cpp \
    Pty.cpp \
    Screen.cpp \
    ScreenWindow.cpp \
    Session.cpp \
    ShellCommand.cpp \
    TerminalCharacterDecoder.cpp \
    TerminalDisplay.cpp \
    Vt102Emulation.cpp \
    kprocess.cpp \
    kpty.cpp \
    kptydevice.cpp \
    kptyprocess.cpp \
    profile/ColorSchemeEditor.cpp \
    profile/EditProfileDialog.cpp \
    profile/FontDialog.cpp \
    profile/KeyBindingEditor.cpp \
    settings/HistorySizeWidget.cpp


## set the QTC_SOURCE environment variable to override the setting here
QTCREATOR_SOURCES = $$(QTC_SOURCE)
isEmpty(QTCREATOR_SOURCES):QTCREATOR_SOURCES=/p/qt/qt-creator-opensource-src-4.10.0/

## set the QTC_BUILD environment variable to override the setting here
IDE_BUILD_TREE = $$(QTC_BUILD)
#isEmpty(IDE_BUILD_TREE):IDE_BUILD_TREE=/p/qt/qt-creator-opensource-src-4.10.0/build-qtc/release/
isEmpty(IDE_BUILD_TREE):IDE_BUILD_TREE=/apps/Qt5.13.1b/Tools/QtCreator/

## uncomment to build plugin into user config directory
## <localappdata>/plugins/<ideversion>
##    where <localappdata> is e.g.
##    "%LOCALAPPDATA%\QtProject\qtcreator" on Windows Vista and later
##    "$XDG_DATA_HOME/data/QtProject/qtcreator" or "~/.local/share/data/QtProject/qtcreator" on Linux
##    "~/Library/Application Support/QtProject/Qt Creator" on OS X
#USE_USER_DESTDIR = yes

QTC_PLUGIN_NAME = TerminalPlugin
QTC_LIB_DEPENDS += \
    # nothing here at this time

QTC_PLUGIN_DEPENDS += \
    coreplugin texteditor projectexplorer

QTC_PLUGIN_RECOMMENDS += \
    # optional plugin dependencies. nothing here at this time

include($$QTCREATOR_SOURCES/src/qtcreatorplugin.pri)

FORMS += \
    profile/ColorSchemeEditor.ui \
    profile/EditProfileAdvancedPage.ui \
    profile/EditProfileAppearancePage.ui \
    profile/EditProfileGeneralPage.ui \
    profile/EditProfileKeyboardPage.ui \
    profile/EditProfileMousePage.ui \
    profile/EditProfileScrollingPage.ui \
    profile/KeyBindingEditor.ui \
    settings/HistorySizeWidget.ui

DISTFILES += \
    ki18n/COPYING.LIB \
    ki18n/Messages.sh
