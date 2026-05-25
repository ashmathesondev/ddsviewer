cmake_minimum_required(VERSION 3.25)
project(ddsviewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

set(VIEWER_SOURCES
    src/main.cpp
    src/App.cpp
    src/DDSLoader.cpp
    src/TextureRenderer.cpp
    src/UIPanel.cpp
)

add_executable(ddsviewer ${VIEWER_SOURCES})
target_include_directories(ddsviewer PRIVATE src)
target_link_libraries(ddsviewer PRIVATE
    SDL3::SDL3
    imgui::imgui
    Microsoft::DirectXTex
    nfd::nfd
)
if(TARGET SDL3::SDL3main)
    target_link_libraries(ddsviewer PRIVATE SDL3::SDL3main)
endif()

enable_testing()
add_subdirectory(tests)
