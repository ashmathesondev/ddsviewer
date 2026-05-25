# DDS Viewer

A cross-platform desktop viewer for DirectDraw Surface (`.dds`) texture files.

## Features

- BC1 – BC7 compressed formats, uncompressed RGBA, HDR (BC6H, R16F, R32F, …)
- Mip level navigation
- Cubemap face selection (+X −X +Y −Y +Z −Z)
- Texture array and 3D depth slice navigation
- Per-channel RGBA toggle
- Exposure control (EV stops) for float/HDR textures
- Zoom (scroll wheel) and pan (left drag)
- Open via CLI argument, drag-and-drop onto window, or File → Open (Ctrl+O)

## Requirements

- CMake 3.25+
- [vcpkg](https://vcpkg.io/) with `VCPKG_ROOT` environment variable set
- Visual Studio 2022 / Clang 16+ / GCC 13+ (C++23 required)

## Building

**Windows (PowerShell):**
```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

**Linux / macOS:**
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
```

## Usage

```
ddsviewer [file.dds]
```

Drag a `.dds` file onto the window or use **File → Open** (Ctrl+O).

## Running Tests

```powershell
# Generate test fixtures (first time only)
./build/Release/generate_fixtures.exe tests/fixtures

# Run all tests
ctest --test-dir build -C Release --output-on-failure
```

## Dependencies

All managed via [vcpkg](https://vcpkg.io/):

| Library | Role |
|---------|------|
| [DirectXTex](https://github.com/microsoft/DirectXTex) | DDS decode (Microsoft) |
| [SDL3](https://libsdl.org/) | Window, renderer, input, drag-and-drop |
| [Dear ImGui](https://github.com/ocornut/imgui) | UI panels and controls |
| [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended) | Native file open dialog |
| [Google Test](https://github.com/google/googletest) | Unit tests |
