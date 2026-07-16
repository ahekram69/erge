cmake_minimum_required(VERSION 3.21)

project(UsbCameraControl VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets)

qt_add_executable(UsbCameraControl WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
)

target_link_libraries(UsbCameraControl PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

set_target_properties(UsbCameraControl PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "USB Camera Control"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.usbcameracontrol"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in"
)

if(WIN32)
    install(TARGETS UsbCameraControl RUNTIME DESTINATION .)
else()
    install(TARGETS UsbCameraControl BUNDLE DESTINATION .)
endif()
