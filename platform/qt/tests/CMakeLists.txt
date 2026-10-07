cmake_minimum_required(VERSION 3.22)

set(TEST_SOURCES
    test_integration.cpp
    ../src/AppIdentity.cpp
    ../src/AppIdentity.h
    ../src/BatchDialog.cpp
    ../src/BatchDialog.h
    ../src/DesktopServices.cpp
    ../src/DesktopServices.h
    ../src/HtmlPreviewWidget.cpp
    ../src/HtmlPreviewWidget.h
    ../src/MainWindow.cpp
    ../src/MainWindow.h
    ../src/Preferences.cpp
    ../src/Preferences.h
    ../src/Presets.cpp
    ../src/Presets.h
    ../src/SettingsDialog.cpp
    ../src/SettingsDialog.h
    ../src/SingleInstance.cpp
    ../src/SingleInstance.h
    ../src/SmokeTest.cpp
    ../src/SmokeTest.h
    ../src/StyleField.cpp
    ../src/StyleField.h
    ../src/UpdateClient.cpp
    ../src/UpdateClient.h
    ../src/YachtCore.cpp
    ../src/YachtCore.h
    ../resources/yacht.qrc
)

add_executable(test_integration ${TEST_SOURCES})

target_link_libraries(test_integration PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Test
    ${YACHT_FFI_LIB}
)

if(APPLE)
    set_target_properties(test_integration PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@executable_path;@loader_path"
    )
elseif(UNIX)
    set_target_properties(test_integration PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN"
    )
endif()

add_test(NAME IntegrationTests COMMAND test_integration)
