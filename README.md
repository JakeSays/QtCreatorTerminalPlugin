## A plugin for Qt Creator that provides an embedded terminal.

This is basically a fork of KDE's Konsole with all standalone functionality removed. It has also been de-KDE'ified and should work in non-KDE environments (untested).
    
Currently support is limited to Linux, but I suspect adding Macos support would be possible.

Due to the vastly different console API's between Windows and Linux a Windows port isn't being considered. This may change at some point, however, now that Microsoft has rewritten the Windows 10 console system.

To build all that is needed is an environment suitable for building Qt Creator plugins.

More specifically:
* Qt version 5.13.1 or later. Earlier versions may work but this has not been attempted.
* A Qt Creator version 4.10 install. This can be either locally built or an official installation.
* The Qt Creator 4.10 source code. Even though building Qt Creator is not required, plugins are reliant on internal headers which are only available via the source code. Because of this the terminal plugin (along with all Qt Creator plugins) are tightly coupled to the Qt Creator version they were built against.

Beyond these requirements if you can successfully build and run a Qt application then you should be able to build the plugin.

### Building

* Set two environment variables
    `QTCREATOR_SOURCES=Path to the root of the Qt creator source tree`
    `IDE_BUILD_TREE=Path to the root of the Qt Creator binary installation`
* Open `terminal.pro` in creator and configure your kit.
* Build!

For example, on my system the two environment variables are set to:
`QTCREATOR_SOURCES=/p/qt/qt-creator-opensource-src-4.10.0` and `IDE_BUILD_TREE=/apps/Qt5.13.1/Tools/QtCreator/`.

By default `libTerminalPlugin.so` is moved to `$IDE_BUILD_TREE/lib/qtcreator/plugins` directory after a successful build. This can be changed to Qt Creator's user settings directory by setting the environment variable `USE_USER_DESTDIR=yes`. On Linux one of these two locations will be used: `$XDG_DATA_HOME/data/QtProject/qtcreator` or `~/.local/share/data/QtProject/qtcreator`. On Ubuntu 19.10 Qt Creator uses the second of the two.

Note that I may change this behavior in the future. I am not keen on the idea of a development build automatically depositing binaries for me.

### Configuration
As it stands the plugin will look for konsole profiles and settings in the same location as konsole. I am re-writing this part to store configuration in its own location.

### Known issues

Currently a custom build of one of the Qt Creator libraries is required. There is one function needed by the plugin that isn't exported. I have temporarily included in the repository a build of the library (`libProjectExplorer.so`) that you can use as a drop-in replacement. The _only_ difference between the stock and custom libraries is the addition of an export macro. There are no functional code changes. I will also be submitting a PR upstream that will export the function.

If you are building Qt Creator yourself you can add the export macro by changing line 34 of `src/plugins/projectexplorer/fileinsessionfinder.h`. Prepend the macro `PROJECTEXPLORER_EXPORT` to that line and re-build.

If you choose to use the `libProjectExplorer.so` that I have provided you must copy it to `$IDE_BUILD_TREE/lib/qtcreator/plugins`. Be sure to backup the original first!! _caveat emptor!_

##### Need for this export
The terminal plugin has the ability to detect a file path in its screen buffer and, if it points to a project file (or a file known to Qt Creator) that file will be highlighted. If you control-left-click (or right-click and select open) the file will be opened in the editor. If the path is followed by `:<line>:<col>` it will be opened to that location.

`findFileInSession()` is used to find the path in a project.

