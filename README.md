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

By default, the script generates build directories only for Visual Studio
versions that are installed. It also pins Visual Studio generation to an
installed MSVC toolset that the generated project can build with. To generate
only VS 2026:

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

The repository includes a shared `ddsviewer` run configuration in `.run/`.
If CLion only shows `ddsviewer_tests`, reload CMake and select the `ddsviewer`
configuration from the run configuration menu. You can also build just the app
target with the `Debug ddsviewer` or `Release ddsviewer` CMake build preset.

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

The window title displays the version declared in `CMakeLists.txt`:
`project(ddsviewer VERSION x.y.z)`. This is the **only** source of the
displayed version — there is no CI/CD pipeline in this repo, and creating a
git tag does **not** by itself change the displayed version. The nearest git
tag/commit from `git describe` is separately logged at startup for build
provenance only.

**To release a new version:**

```powershell
<# bump project(ddsviewer VERSION x.y.z) in CMakeLists.txt #>
git tag v1.2.0
cmake -B build
cmake --build build --config Release
```

The `CMakeLists.txt` bump is what makes the title show `DDS Viewer 1.2.0`; the
`git tag` step only makes `git describe` (logged at startup, e.g.
`v1.2.0-3-gabcdef-dirty`) match. Skipping the tag still leaves the title
correct; skipping the `CMakeLists.txt` edit does not.

Version is read at **configure time** — re-run CMake after changing it. The
version template lives in `src/version.h.in`; the generated `version.h` is
written to the build directory and not committed.

## Dependencies

All managed via [vcpkg](https://vcpkg.io/):

| Library                                                                        | Role                                   |
|--------------------------------------------------------------------------------|----------------------------------------|
| [DirectXTex](https://github.com/microsoft/DirectXTex)                          | DDS decode (Microsoft)                 |
| [SDL3](https://libsdl.org/)                                                    | Window, renderer, input, drag-and-drop |
| [Dear ImGui](https://github.com/ocornut/imgui)                                 | UI panels and controls                 |
| [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended) | Native file open dialog                |
| [Google Test](https://github.com/google/googletest)                            | Unit tests                             |
