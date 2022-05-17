## A plugin for Qt Creator that provides an embedded terminal.

This is basically a fork of KDE's Konsole with all standalone functionality removed. It has also been de-KDE'ified and should work in non-KDE environments (untested).
    
Currently support is limited to Linux, but I suspect adding Macos support would be possible (Feel free to offer a PR).

Due to the vastly different console API's between Windows and Linux a Windows port isn't being considered. This may change at some point, however, now that Microsoft has rewritten the Windows 10 console system.

### Building

To build all that is needed is an environment suitable for building Qt Creator plugins.

More specifically:

* Qt version 6 or later (for QtCreator 6 or later or Qt5 for QtCreator 4 and 5). Qt Creator builds are usually tied to a particular version of Qt so try to match the two as close as possible.
* A Qt Creator install with _Qt Creator Plugin Development_ component or a local Qt Creator build with source code.

 Run CMake and specify two path:
 * Qt folder
 * A Qt Creator installation or build directory

For example, on my system the two environment variables are set to:
`mkdir build`
`cd build`
`cmake -DCMAKE_PREFIX_PATH="~/Programs/Qt/6.2.4/gcc_64;~/Programs/qtcreator-7.0.1" ..`
or when compiling against a local Qt Creator build:
`cmake -DCMAKE_PREFIX_PATH="~/Programs/Qt/6.2.4/gcc_64;~/Sources/qt-creator/build" ..`

When building using QtCreator you don't only need to specify path to Qt Creator, so the easiest way is to add it as a line in CMakeLists.txt:
`list(APPEND CMAKE_PREFIX_PATH "/home/developer/Programs/qtcreator-7.0.1")`


#### Running
In order to run from the build directory you need to 'build' the **qt_creator** target:
`cmake --build . -t qtcreator_run`

When building using Qt Creator you need to edit the run settings:
Executable: `<path to>/bin/qtcreator`
Arguments: `-pluginpath "%{buildDir}/lib/qtcreator/plugins" -tcs`

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
