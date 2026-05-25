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

**Windows (PowerShell) — quick:**
```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

**Windows — using the generation script:**
```powershell
./generate-build-files.ps1 -Target windows -SkipVs2026
cmake --build build/vs2022 --config Release
```

**Linux / macOS:**
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
```

## Installing (self-contained dist folder)

Builds a `dist/` folder containing `ddsviewer` and all required DLLs/shared libraries.

**Windows — one step:**
```powershell
./generate-build-files.ps1 -Target windows -SkipVs2026 -Install
# Output: dist/ddsviewer.exe + SDL3.dll, DirectXTex.dll, nfd.dll
```

**Manual install (any platform):**
```bash
cmake --install build --prefix dist --config Release
```

The resulting `dist/` folder is self-contained and can be copied anywhere.

## Usage

```
ddsviewer [file.dds]
```

Drag a `.dds` file onto the window or use **File → Open** (Ctrl+O).

## Running Tests

```powershell
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
