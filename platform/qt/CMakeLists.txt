cmake_minimum_required(VERSION 3.22)
project(YACHT LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Test)

set(YACHT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${YACHT_ROOT}/bindings/c"
)

if(NOT YACHT_FFI_LIB)
    find_library(YACHT_FFI_LIB NAMES yacht_ffi libyacht_ffi
        PATHS
            "${YACHT_FFI_DIR}"
            "${YACHT_ROOT}/target/release"
            "${YACHT_ROOT}/target/debug"
            "${YACHT_ROOT}/target/aarch64-apple-darwin/release"
            "${YACHT_ROOT}/target/aarch64-apple-darwin/debug"
            "${YACHT_ROOT}/target/x86_64-apple-darwin/release"
            "${YACHT_ROOT}/target/x86_64-apple-darwin/debug"
            "${YACHT_ROOT}/target/x86_64-pc-windows-msvc/release"
            "${YACHT_ROOT}/target/aarch64-pc-windows-msvc/release"
        NO_DEFAULT_PATH
    )
    if(NOT YACHT_FFI_LIB)
        find_library(YACHT_FFI_LIB NAMES yacht_ffi libyacht_ffi)
    endif()
endif()

if(NOT YACHT_FFI_LIB)
    message(FATAL_ERROR "Could not find yacht_ffi library. Build yacht-ffi with cargo first, or pass -DYACHT_FFI_LIB=...")
endif()

message(STATUS "Using yacht_ffi: ${YACHT_FFI_LIB}")

set(SOURCES
    src/AppIdentity.cpp
    src/AppIdentity.h
    src/BatchDialog.cpp
    src/BatchDialog.h
    src/DesktopServices.cpp
    src/DesktopServices.h
    src/HtmlPreviewWidget.cpp
    src/HtmlPreviewWidget.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/Preferences.cpp
    src/Preferences.h
    src/Presets.cpp
    src/Presets.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/SingleInstance.cpp
    src/SingleInstance.h
    src/SmokeTest.cpp
    src/SmokeTest.h
    src/StyleField.cpp
    src/StyleField.h
    src/UpdateClient.cpp
    src/UpdateClient.h
    src/YachtCore.cpp
    src/YachtCore.h
    src/main.cpp
    resources/yacht.qrc
)

add_executable(YachtApp ${SOURCES})

target_link_libraries(YachtApp PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    ${YACHT_FFI_LIB}
)

if(APPLE)
    # Set rpath so @executable_path/../Frameworks or @loader_path finds libyacht_ffi.dylib
    set_target_properties(YachtApp PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@executable_path;@loader_path"
    )
elseif(UNIX)
    set_target_properties(YachtApp PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN"
    )
endif()

enable_testing()
add_subdirectory(tests)
