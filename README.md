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
cmake -B build
cmake --build build --config Release
```

**Windows — using the generation script:**
```powershell
./generate-build-files.ps1 -Target windows
cmake --build build/vs2022 --config Release
```

By default, the script generates both Visual Studio 2022 and Visual Studio 2026
build directories when those generators are available. To generate only VS 2026:

```powershell
./generate-build-files.ps1 -Target windows -SkipVs2022
```

`generate-build-files.ps1` generates build files by default. To generate, build, and install a Release build to `dist/` in one step:

```powershell
./generate-build-files.ps1 -Install
```

**Linux / macOS:**
```bash
cmake -B build
cmake --build build
```

When `VCPKG_ROOT` is set, the project automatically uses
`$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`. You can still override this by
passing `-DCMAKE_TOOLCHAIN_FILE=...` explicitly.

**CLion:**

CLion may not inherit environment variables from your shell when launched from
the Start menu. The project will also find `vcpkg` on `PATH`; if that is not
available, add this to the CLion CMake profile options:

```text
-DVCPKG_ROOT=E:/vcpkg
```

Reload CMake after changing the profile.

If CLion is using Ninja with MSVC, make sure the CLion toolchain is a Visual
Studio toolchain so MSVC include paths are initialized. Otherwise DirectXTex's
OpenMP dependency may fail to find `omp.h`.

**Visual Studio Code / Insiders:**

Use the CMake Tools extension and select the `Debug`, `Release`, or
`RelWithDebInfo` configure preset. The presets intentionally do not force
Ninja; on Windows, CMake can use the installed Visual Studio generator instead.

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
./run-tests.ps1
```

Use `./run-tests.ps1 -TestRegex DDSLoader` to run a subset, or
`./run-tests.ps1 -Generate` to create build files first when needed.

## Versioning

The window title displays the version derived from the nearest git tag.

**To release a new version:**

```powershell
git tag v1.2.0
cmake -B build
cmake --build build --config Release
```

The title will show `DDS Viewer v1.2.0`. Commits after a tag show the tag plus a commit offset and SHA, e.g. `DDS Viewer v1.2.0-3-gabcdef`. Uncommitted changes append `-dirty`.

Version is read at **configure time** — re-run cmake after tagging to update it. The version template lives in `src/version.h.in`; the generated `version.h` is written to the build directory and not committed.

## Dependencies

All managed via [vcpkg](https://vcpkg.io/):

| Library | Role |
|---------|------|
| [DirectXTex](https://github.com/microsoft/DirectXTex) | DDS decode (Microsoft) |
| [SDL3](https://libsdl.org/) | Window, renderer, input, drag-and-drop |
| [Dear ImGui](https://github.com/ocornut/imgui) | UI panels and controls |
| [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended) | Native file open dialog |
| [Google Test](https://github.com/google/googletest) | Unit tests |
