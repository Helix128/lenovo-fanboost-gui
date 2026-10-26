cmake_minimum_required(VERSION 3.20)
project(lenovo-fanboost-gui
    VERSION 0.1.1
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
    src/profile_io.cpp
    src/session_history.cpp
    src/autostart.cpp
)

set(HEADERS
    src/mainwindow.h
    src/curve_editor.h
    src/config_manager.h
    src/telemetry_monitor.h
    src/tray_icon.h
    src/profile_io.h
    src/session_history.h
    src/autostart.h
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

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(test-curve-editor tests/test_curve_editor.cpp src/curve_editor.cpp src/curve_editor.h)
    target_include_directories(test-curve-editor PRIVATE src)
    target_link_libraries(test-curve-editor PRIVATE Qt6::Widgets Qt6::Test)
    add_test(NAME curve-editor COMMAND test-curve-editor)

    add_executable(test-config tests/test_config.cpp src/config_manager.cpp src/config_manager.h)
    target_include_directories(test-config PRIVATE src)
    target_link_libraries(test-config PRIVATE Qt6::Widgets Qt6::Test)
    add_test(NAME config COMMAND test-config)

    set(WORKSPACE_TEST_SOURCES ${SOURCES})
    list(REMOVE_ITEM WORKSPACE_TEST_SOURCES src/main.cpp)
    add_executable(test-workspace tests/test_workspace.cpp ${WORKSPACE_TEST_SOURCES} ${HEADERS} ${RESOURCES})
    target_include_directories(test-workspace PRIVATE src)
    target_link_libraries(test-workspace PRIVATE Qt6::Widgets Qt6::Network Qt6::Test)
    add_test(NAME workspace COMMAND test-workspace)
    add_executable(test-history tests/test_session_history.cpp src/session_history.cpp src/session_history.h)
    target_include_directories(test-history PRIVATE src)
    target_link_libraries(test-history PRIVATE Qt6::Widgets Qt6::Test)
    add_test(NAME history COMMAND test-history)
    add_executable(test-features tests/test_features.cpp src/profile_io.cpp src/config_manager.cpp src/config_manager.h src/autostart.cpp)
    target_include_directories(test-features PRIVATE src)
    target_link_libraries(test-features PRIVATE Qt6::Widgets Qt6::Test)
    add_test(NAME features COMMAND test-features)
    set_tests_properties(curve-editor config workspace history features PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

include(GNUInstallDirs)
install(TARGETS lenovo-fanboost-gui
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES resources/lenovo-fanboost-gui.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

foreach(SIZE 16 24 32 48 64 128 256)
    install(FILES resources/icons/lenovo-fanboost-gui-${SIZE}.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/apps
        RENAME lenovo-fanboost-gui.png
    )
endforeach()

install(FILES resources/icons/lenovo-fanboost-gui.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
    RENAME lenovo-fanboost-gui.png
)
