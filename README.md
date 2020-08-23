## A plugin for Qt Creator that provides an embedded terminal.

This is basically a fork of KDE's Konsole with all standalone functionality removed. It has also been de-KDE'ified and should work in non-KDE environments (untested).
    
Currently support is limited to Linux, but I suspect adding Macos support would be possible (Feel free to offer a PR).

Due to the vastly different console API's between Windows and Linux a Windows port isn't being considered. This may change at some point, however, now that Microsoft has rewritten the Windows 10 console system.

### Quick & Dirty
If you wish to get things going quickly you can copy `binaries/libTerminalPlugin.so` to one of the directories listed in the Installation section below. Note that this shared object will only run on 64 bit Linux, and it does not require a custom Qt Creator library described in Known Issues below.

    Eventually "official" releases will be provided, but this is what you get for now.

### Building

To build all that is needed is an environment suitable for building Qt Creator plugins.

More specifically:

* Qt version 5.13.1 or later. Earlier versions may work - use at your own risk. Qt Creator builds are usually tied to a particular version of Qt so try to match the two as close as possible.
* A Qt Creator version 4.10+ install. This can be either locally built or an official installation.
* The Qt Creator 4.10+ source code. Even though building Qt Creator is not required, plugins are reliant on internal headers which are only available via the source code. Because of this the terminal plugin (along with all Qt Creator plugins) are tightly coupled to the Qt Creator version they were built against.

> I have yet to confirm this but I believe the required Qt Creator headers are now provided independent of the source code. If anyone knows anything about this feel free to let me know.

#### for Qt Creator version 4.12 and later


Beyond these requirements if you can successfully build and run a Qt application then you should be able to build the plugin.

* Set two environment variables
    `QTCREATOR_SOURCES=Path to the root of the Qt creator source tree`
    `IDE_BUILD_TREE=Path to the root of the Qt Creator binary installation`
* Open `terminal.pro` in creator and configure your kit.
* Build!

For example, on my system the two environment variables are set to:

`QTCREATOR_SOURCES=/p/qt/qt-creator-opensource-src-4.10.0` and `IDE_BUILD_TREE=/apps/Qt5.13.1/Tools/QtCreator`.

### Installation
By default `libTerminalPlugin.so` is moved to `$IDE_BUILD_TREE/lib/qtcreator/plugins` directory after a successful build. This can be changed to Qt Creator's user settings directory by setting the environment variable `USE_USER_DESTDIR=yes`. On Linux one of these two locations will be used: `$XDG_DATA_HOME/data/QtProject/qtcreator` or `~/.local/share/data/QtProject/qtcreator`. On Ubuntu 19.10 Qt Creator uses the second of the two.

Note that I may change this behavior in the future. I am not keen on the idea of a development build automatically depositing binaries for me.

### Configuration
As it stands the plugin will look for konsole profiles and settings in the same location as konsole. I will be re-writing this part to store configuration in its own location.

### Known Issues

The terminal plugin has the ability to detect file paths in its screen buffer and, if it points to a project file (or a file known to Qt Creator) that file will be highlighted. If you control-left-click (or right-click and select open) the file will be opened in the editor. If the path is followed by `:<line>:<col>` it will be opened to that location.

> The following applies to versions 4.10 and 4.11 of Qt Creator. The needed functionality was added to 4.12 by the Qt Creator team. The file path ability described above works with stock Creator.

To enable this hot link functionaly a custom build of one of the Qt Creator libraries is required. There is one function needed by the plugin that isn't exported. I have temporarily included a local build in the repository of the library (`libProjectExplorer.so`) that you can use as a drop-in replacement. The _only_ difference between the stock and custom libraries is the addition of an export macro. There are no functional code changes. I will also be submitting a PR upstream for this.

If you are building Qt Creator yourself you can add the export macro by changing line 34 of `src/plugins/projectexplorer/fileinsessionfinder.h`. Prepend `PROJECTEXPLORER_EXPORT` to that line and re-build.

If you choose to use the `libProjectExplorer.so` that I have provided you must copy it to `$IDE_BUILD_TREE/lib/qtcreator/plugins`. Be sure to backup the original first!! _caveat emptor!_

`findFileInSession()` is the offending function.

    Note that if you can live without this hot link feature, see line 26 of terminal.pro.
    Doing so will allow the plugin to work with a stock Qt Creator install.

### Localization
Translation has been disabled due to the dependency on the KDE i18n project. I indend to fix this at some point.

### Acknowledgements

The Qt Creator terminal plugin contains _very_ little code written by myself. As such 99.897% of the credit goes to the KDE project and their excellent KDE Foundation libraries.

Inspiration for this implementation of a Qt Creator terminal came from
* https://github.com/manyoso/qt-creator-terminalplugin
* https://github.com/lxqt/qtermwidget