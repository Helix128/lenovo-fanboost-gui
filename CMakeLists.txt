cmake_minimum_required(VERSION 3.20)
project(lenovo-fanboost-gui
    VERSION 0.1.0
    DESCRIPTION "Native Qt6 companion GUI for lenovo-fanboost"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/curve_editor.cpp
    src/config_manager.cpp
    src/telemetry_monitor.cpp
    src/tray_icon.cpp
)

set(HEADERS
    src/mainwindow.h
    src/curve_editor.h
    src/config_manager.h
    src/telemetry_monitor.h
    src/tray_icon.h
)

set(RESOURCES
    resources/resources.qrc
)

add_executable(lenovo-fanboost-gui
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_link_libraries(lenovo-fanboost-gui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

target_include_directories(lenovo-fanboost-gui PRIVATE
    src
)

include(GNUInstallDirs)
install(TARGETS lenovo-fanboost-gui
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES resources/lenovo-fanboost-gui.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES resources/icons/lenovo-fanboost-gui.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
