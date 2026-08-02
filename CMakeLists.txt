cmake_minimum_required(VERSION 3.25)

option(DDSVIEWER_DISABLE_AUTO_VCPKG "Disable automatic vcpkg toolchain discovery" OFF)

set(_vcpkg_roots)
if(NOT DDSVIEWER_DISABLE_AUTO_VCPKG AND DEFINED VCPKG_ROOT AND NOT "${VCPKG_ROOT}" STREQUAL "")
    list(APPEND _vcpkg_roots "${VCPKG_ROOT}")
endif()
if(NOT DDSVIEWER_DISABLE_AUTO_VCPKG AND DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    list(APPEND _vcpkg_roots "$ENV{VCPKG_ROOT}")
endif()

set(_vcpkg_root_toolchain)
foreach(_vcpkg_root IN LISTS _vcpkg_roots)
    file(TO_CMAKE_PATH "${_vcpkg_root}" _vcpkg_root_cmake)
    set(_vcpkg_toolchain "${_vcpkg_root_cmake}/scripts/buildsystems/vcpkg.cmake")
    if(EXISTS "${_vcpkg_toolchain}")
        set(_vcpkg_root_toolchain "${_vcpkg_toolchain}")
        break()
    endif()
endforeach()

if(NOT DDSVIEWER_DISABLE_AUTO_VCPKG AND NOT _vcpkg_root_toolchain)
    find_program(_vcpkg_executable NAMES vcpkg vcpkg.exe)
    if(_vcpkg_executable)
        get_filename_component(_vcpkg_root "${_vcpkg_executable}" DIRECTORY)
        file(TO_CMAKE_PATH "${_vcpkg_root}" _vcpkg_root_cmake)
        set(_vcpkg_toolchain "${_vcpkg_root_cmake}/scripts/buildsystems/vcpkg.cmake")
        if(EXISTS "${_vcpkg_toolchain}")
            set(_vcpkg_root_toolchain "${_vcpkg_toolchain}")
        endif()
    endif()
endif()

if(DEFINED CMAKE_TOOLCHAIN_FILE
   AND NOT "${CMAKE_TOOLCHAIN_FILE}" STREQUAL ""
   AND _vcpkg_root_toolchain
   AND NOT "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "${_vcpkg_root_toolchain}"
   AND "${CMAKE_TOOLCHAIN_FILE}" MATCHES "Microsoft Visual Studio.*/VC/vcpkg/scripts/buildsystems/vcpkg\\.cmake$")
    message(STATUS "Replacing Visual Studio bundled vcpkg toolchain with VCPKG_ROOT toolchain: ${_vcpkg_root_toolchain}")
    set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_root_toolchain}" CACHE FILEPATH "vcpkg toolchain file" FORCE)
endif()

if(DEFINED CMAKE_TOOLCHAIN_FILE
   AND NOT "${CMAKE_TOOLCHAIN_FILE}" STREQUAL ""
   AND NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    message(WARNING "Ignoring missing CMAKE_TOOLCHAIN_FILE from cache: ${CMAKE_TOOLCHAIN_FILE}")
    unset(CMAKE_TOOLCHAIN_FILE CACHE)
    unset(CMAKE_TOOLCHAIN_FILE)
endif()

if(NOT DDSVIEWER_DISABLE_AUTO_VCPKG AND (NOT DEFINED CMAKE_TOOLCHAIN_FILE OR "${CMAKE_TOOLCHAIN_FILE}" STREQUAL ""))
    if(_vcpkg_root_toolchain)
        set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_root_toolchain}" CACHE FILEPATH "vcpkg toolchain file")
        message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
    endif()

    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        message(STATUS "No vcpkg toolchain found. Set VCPKG_ROOT, pass -DVCPKG_ROOT=<path>, or pass -DCMAKE_TOOLCHAIN_FILE=<path>/scripts/buildsystems/vcpkg.cmake.")
    endif()
endif()

if(DEFINED CMAKE_TOOLCHAIN_FILE
   AND "${CMAKE_TOOLCHAIN_FILE}" MATCHES "vcpkg\\.cmake$"
   AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    if(CMAKE_HOST_WIN32)
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg target triplet")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "vcpkg target triplet")
    elseif(CMAKE_HOST_APPLE)
        set(VCPKG_TARGET_TRIPLET "x64-osx" CACHE STRING "vcpkg target triplet")
    endif()
endif()

if(DEFINED CMAKE_TOOLCHAIN_FILE
   AND "${CMAKE_TOOLCHAIN_FILE}" MATCHES "vcpkg\\.cmake$"
   AND NOT VCPKG_TOOLCHAIN)
    include("${CMAKE_TOOLCHAIN_FILE}")
endif()

project(ddsviewer VERSION 0.3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

# Display version comes from project(... VERSION ...). Git describe is captured
# separately for build provenance.
set(DDSVIEWER_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(DDSVIEWER_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(DDSVIEWER_VERSION_PATCH ${PROJECT_VERSION_PATCH})
find_package(Git QUIET)
set(DDSVIEWER_GIT_DESCRIBE "v${PROJECT_VERSION}-unversioned")
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --match "v*" --always --dirty=-dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE DDSVIEWER_GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
configure_file(src/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(nfd CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set(VIEWER_SOURCES
    src/main.cpp
    src/App.cpp
    src/DDSLoader.cpp
    src/TextureRenderer.cpp
    src/UIPanel.cpp
    src/Log.cpp
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
    spdlog::spdlog
)

if(MSVC)
    target_compile_options(ddsviewer PRIVATE /utf-8)
endif()

enable_testing()
add_subdirectory(tests)

# Install rules — cmake --install <builddir> --prefix dist --config Release
install(TARGETS ddsviewer RUNTIME DESTINATION .)
if(WIN32)
    # Copy dependent DLLs (SDL3, imgui, nfd, etc.) alongside the executable
    install(FILES $<TARGET_RUNTIME_DLLS:ddsviewer> DESTINATION .)
endif()
