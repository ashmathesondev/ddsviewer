cmake_minimum_required(VERSION 3.25)
project(ddsviewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)

set(VIEWER_SOURCES
    src/main.cpp
    src/App.cpp
    src/DDSLoader.cpp
    src/TextureRenderer.cpp
    src/UIPanel.cpp
)

add_executable(ddsviewer ${VIEWER_SOURCES})
target_compile_features(ddsviewer PRIVATE cxx_std_23)
target_include_directories(ddsviewer PRIVATE src)
# Link SDL3main first (must precede SDL3 for correct MSVC linker ordering)
if(TARGET SDL3::SDL3main)
    target_link_libraries(ddsviewer PRIVATE SDL3::SDL3main)
endif()
target_link_libraries(ddsviewer PRIVATE
    SDL3::SDL3
    imgui::imgui
    Microsoft::DirectXTex
    nfd::nfd
)

enable_testing()
add_subdirectory(tests)

# Install rules — cmake --install <builddir> --prefix dist --config Release
install(TARGETS ddsviewer RUNTIME DESTINATION .)
if(WIN32)
    # Copy dependent DLLs (SDL3, imgui, nfd, etc.) alongside the executable
    install(FILES $<TARGET_RUNTIME_DLLS:ddsviewer> DESTINATION .)
endif()
