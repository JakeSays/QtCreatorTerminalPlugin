## A plugin for Qt Creator that provides an embedded terminal (QtCreator v8+).

This is basically a fork of KDE's Konsole with all standalone functionality removed. It has also been de-KDE'ified and should work in non-KDE environments (untested).
    
Currently support is limited to Linux, but I suspect adding Macos support would be possible (Feel free to offer a PR).

Due to the vastly different console API's between Windows and Linux a Windows port isn't being considered. This may change at some point, however, now that Microsoft has rewritten the Windows 10 console system.

### Building

To build all that is needed is an environment suitable for building Qt Creator plugins.

More specifically:

* Qt version 6.3.1 or later. Earlier versions may work - use at your own risk, but at least Qt 6 is required. Qt Creator builds are usually tied to a particular version of Qt so try to match the two as close as possible.
* A Qt Creator version 8+ install. This can be either locally built or an official installation.
* One of the following:
    * **Easy way**: Install plugin support when installing Qt Creator.
    * **Hard way**: The Qt Creator 8+ source code. Even though building Qt Creator is not required, plugins are reliant on internal headers which are only available via the source code. Because of this the terminal plugin (along with all Qt Creator plugins) are tightly coupled to the Qt Creator version they were built against. This method is currently untested.

* Open the project in Qt Creator (It's cmake based, so open CMakeLists.txt)
* In the Build Settings add the following cmake variable:
    * `QtCreator_DIR=<creator install dir>/lib/cmake/QtCreator`
* Build!

### Installation
By default `libTerminalPlugin.so` is moved to `<output directory>/lib/qtcreator/plugins` directory after a successful build. This can be changed to Qt Creator's user settings directory by setting the environment variable `USE_USER_DESTDIR=yes`. On Linux one of these two locations will be used: `$XDG_DATA_HOME/data/QtProject/qtcreator` or `~/.local/share/data/QtProject/qtcreator`. On Ubuntu 19.10 Qt Creator uses the second of the two.

Note that I may change this behavior in the future. I am not keen on the idea of a development build automatically depositing binaries for me.

### Configuration
As it stands the plugin will look for konsole profiles and settings in the same location as konsole. I will be re-writing this part to store configuration in its own location.

### Known Issues

Translation has been disabled due to the dependency on the KDE i18n project.

### Acknowledgements

The Qt Creator terminal plugin contains _very_ little code written by myself. As such 99.897% of the credit goes to the KDE project and their excellent KDE Foundation libraries.

Inspiration for this implementation of a Qt Creator terminal came from
* https://github.com/manyoso/qt-creator-terminalplugin
* https://github.com/lxqt/qtermwidget
