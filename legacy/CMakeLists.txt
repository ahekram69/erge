cmake_minimum_required(VERSION 3.21)
project(UsbCameraControlLegacy VERSION 0.9.5 LANGUAGES CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Legacy is a Windows x86 (32-bit) target")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Multimedia)
add_executable(UsbCameraControlLegacy WIN32
    main.cpp Mp4Writer.cpp Mp4Writer.h
    ../src/NativeCameraControls_windows.cpp ../src/NativeCameraControls.h
    ../resources.qrc ../windows/app-icon.rc)
target_include_directories(UsbCameraControlLegacy PRIVATE ../src)
target_compile_definitions(UsbCameraControlLegacy PRIVATE
    APP_VERSION="${PROJECT_VERSION}" NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)
target_compile_options(UsbCameraControlLegacy PRIVATE /utf-8)
target_link_libraries(UsbCameraControlLegacy PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Multimedia
    strmiids ole32 oleaut32 mfplat mfreadwrite mfuuid)
set_target_properties(UsbCameraControlLegacy PROPERTIES OUTPUT_NAME "USB-Camera-Control-Legacy-x86-v${PROJECT_VERSION}")
