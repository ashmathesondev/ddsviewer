cmake_minimum_required(VERSION 3.25)
project(ddsviewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

# Derive version string from the nearest git tag (e.g. v1.2.0).
# Re-run cmake after tagging to pick up the new version.
find_package(Git QUIET)
set(DDSVIEWER_GIT_DESCRIBE "v0.0.0-unversioned")
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --match "v*" --always --dirty=-dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE DDSVIEWER_GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(DDSVIEWER_GIT_DESCRIBE MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(DDSVIEWER_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(DDSVIEWER_VERSION_MINOR ${CMAKE_MATCH_2})
    set(DDSVIEWER_VERSION_PATCH ${CMAKE_MATCH_3})
else()
    set(DDSVIEWER_VERSION_MAJOR 0)
    set(DDSVIEWER_VERSION_MINOR 0)
    set(DDSVIEWER_VERSION_PATCH 0)
endif()
configure_file(src/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)
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
target_include_directories(ddsviewer PRIVATE src ${CMAKE_BINARY_DIR}/generated)
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
