cmake_minimum_required(VERSION 3.22)

project(SpriteStitcher3Prototype VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

add_library(SpriteStitcher3Core STATIC
    src/DmcMatcher.cpp
    src/DmcMatcher.h
    src/ImageAnalysis.cpp
    src/ImageAnalysis.h
)

target_include_directories(SpriteStitcher3Core PUBLIC src)
target_link_libraries(SpriteStitcher3Core PUBLIC Qt6::Gui Qt6::Core)

add_executable(SpriteStitcher3
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
)

target_link_libraries(SpriteStitcher3 PRIVATE SpriteStitcher3Core Qt6::Widgets)
target_compile_definitions(SpriteStitcher3 PRIVATE APP_VERSION="${PROJECT_VERSION}")

add_executable(SpriteStitcher3ImageAnalysisTest
    tests/ImageAnalysisTest.cpp
)

target_link_libraries(SpriteStitcher3ImageAnalysisTest PRIVATE SpriteStitcher3Core)

enable_testing()
add_test(NAME image_analysis COMMAND SpriteStitcher3ImageAnalysisTest)
