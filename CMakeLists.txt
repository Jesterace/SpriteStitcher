cmake_minimum_required(VERSION 3.22)

project(SpriteStitcher VERSION 2.10.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

add_executable(SpriteStitcher
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/PatternEngine.cpp
    src/PatternEngine.h
    resources/resources.qrc
)

target_link_libraries(SpriteStitcher PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core)

target_compile_definitions(SpriteStitcher PRIVATE APP_VERSION="${PROJECT_VERSION}")

install(TARGETS SpriteStitcher RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES packaging/linux/spritestitcher.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

install(FILES packaging/icons/spritestitcher.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)

