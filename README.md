## A plugin for Qt Creator that provides an embedded terminal.

This is basically a fork of KDE's Konsole with all standalone functionality removed. It has also been de-KDE'ified and should work in non-KDE environments (untested).
    
Currently support is limited to Linux, but I suspect adding Macos support would be possible (Feel free to offer a PR).

Due to the vastly different console API's between Windows and Linux a Windows port isn't being considered. This may change at some point, however, now that Microsoft has rewritten the Windows 10 console system.

### Supported Qt Creator versions

This plugin supports Qt Creator version 4.15 up to 7.0.x. You need to download a version with the major number equal to your Qt Creator's version, ie. plugin version 5.0.x for Qt Creator 5.0.x. When compiling from source you need to use a branch that is equal to the target Qt Creator version, ie. branch "5.0" for Qt Creator 5.0.x.

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

When building using QtCreator you only need to specify path to Qt Creator, so the easiest way is to add it as a line in CMakeLists.txt:
`list(APPEND CMAKE_PREFIX_PATH "/home/developer/Programs/qtcreator-7.0.1")`

#### Running
In order to run from the build directory you need to 'build' the **qt_creator** target:
`cmake --build . -t run_qtcreator`

When building using Qt Creator you need to edit the run settings:
Executable: `<path to>/bin/qtcreator`
Arguments: `-pluginpath "%{buildDir}/lib/qtcreator/plugins" -tcs`

Both of the above methods will start Qt Creator with a fresh copy of settings so you can test this plugin without breaking your regular Qt Creator installation.

### Installation from source
`cmake --install .`
`libTerminalPlugin.so` is copied to `$IDE_BUILD_TREE/lib/qtcreator/plugins` directory by the install target. This can be changed to Qt Creator's user settings directory by setting the environment variable `USE_USER_DESTDIR=yes`. On Linux one of these two locations will be used: `$XDG_DATA_HOME/data/QtProject/qtcreator` or `~/.local/share/data/QtProject/qtcreator`. On Ubuntu 19.10 Qt Creator uses the second of the two.


### Configuration
As it stands the plugin will look for konsole profiles and settings in the same location as konsole. I will be re-writing this part to store configuration in its own location.

### Acknowledgements

The Qt Creator terminal plugin contains _very_ little code written by myself. As such 99.897% of the credit goes to the KDE project and their excellent KDE Foundation libraries.
This code has been ported to CMake, Qt 6 and Qt Creator 7 API by [Przemysław Adam Sowa](https://github.com/pa-sowa) and it is a fork of the orignal work by [JakeSays](https://github.com/JakeSays/QtCreatorTerminalPlugin).

Inspiration for this implementation of a Qt Creator terminal came from
* https://github.com/JakeSays/QtCreatorTerminalPlugin - original Qt5 / QtCreator 4.x implementation
* https://github.com/manyoso/qt-creator-terminalplugin
* https://github.com/lxqt/qtermwidget
