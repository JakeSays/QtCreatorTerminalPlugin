cmake_minimum_required(VERSION 3.10)

# Uncomment below line if you don't want to pass this as an argument
# list(APPEND CMAKE_PREFIX_PATH "<set to the build folder of QtCreator>")

project(TerminalPlugin)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Qml Widgets Xml Network Core5Compat)
set(QtX Qt${QT_VERSION_MAJOR})

add_subdirectory(src)

find_file(QTCREATOR_EXECUTABLE NAMES qtcreator PATHS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES bin NO_DEFAULT_PATH)

add_custom_target(run_qtcreator
    COMMAND "${QTCREATOR_EXECUTABLE}" -pluginpath "${CMAKE_BINARY_DIR}/lib/qtcreator/plugins" -tcs
    COMMENT "Running Qt Creator"
)
