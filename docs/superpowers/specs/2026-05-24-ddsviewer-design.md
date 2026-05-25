# DDS Viewer — Design Spec
_2026-05-24_

## Overview

Cross-platform (Windows-primary) desktop tool for inspecting DirectDraw Surface (.dds) texture files. Supports the full DDS feature set: BC-compressed formats, mipmaps, cubemaps, texture arrays, and 3D textures. Built in C++23 with CMake + vcpkg.

Support for Windows 11 and later, Unix (Ubuntu 22.04+, macOS 12+).


## Stack

| Role | Library | vcpkg name |
|------|---------|------------|
| DDS decode | DirectXTex (Microsoft) | `directxtex` |
| Window / renderer | SDL3 | `sdl3` |
| UI | Dear ImGui | `imgui[sdl3-binding,sdl3-renderer-binding]` |
| File dialog | nativefiledialog-extended | `nativefiledialog-extended` |
| Unit tests | Google Test | `gtest` |

## Features

**File opening — three paths:**
- CLI argument: `ddsviewer texture.dds`
- Drag-and-drop: SDL `DROP_FILE` event anywhere on the window
- File dialog: File menu → Open (Ctrl+O), native OS dialog filtered to `.dds`

**Texture navigation:**
- Mip level selection (◀/▶ buttons, shows dimensions of selected mip)
- Cubemap face selection (+X −X +Y −Y +Z −Z buttons; hidden for non-cubemaps)
- Array slice / 3D depth slice navigation (◀/▶; hidden for single textures)

**Display controls:**
- RGBA channel toggles (independent, click to isolate or combine)
- Exposure slider in EV stops (−10 to +10); gamma 2.2 correction applied after
- Both controls visible only for float-format textures; UNORM textures show neither

**Canvas interaction:**
- Mouse wheel: zoom (0.05× – 32×, centred on cursor)
- Left-drag: pan
- Overlay: current dimensions and zoom level (top-right corner)

**Metadata display (bottom panel, left side):**
- Filename, DXGI format name, width × height (× depth for 3D), mip count, array size / face count

## Architecture

```
main.cpp
  └─ App                  (owns all state, drives the frame loop)
       ├─ DDSLoader        (pure: path → TextureData)
       ├─ TextureRenderer  (SDL_Texture lifecycle, uploads selected slice)
       └─ UIPanel          (ImGui bottom panel + menu bar)

TextureView                (POD state struct shared by App, TextureRenderer, UIPanel)
```

### Key Types

```cpp
struct TextureData {
    DXGI_FORMAT        format;
    uint32_t           width, height, depth;
    uint32_t           mipCount, arraySize;
    bool               isCubemap;
    // images[arrayIndex * mipCount + mipIndex] — each element is RGBA8 or RGBA16F bytes
    std::vector<std::vector<uint8_t>> images;
};

struct TextureView {
    int  selectedMip    = 0;
    int  selectedSlice  = 0;   // array index or depth slice
    int  selectedFace   = 0;   // 0-5 for cubemaps
    bool channelR = true, channelG = true, channelB = true, channelA = false;
    float exposure      = 0.0f; // EV stops
    float zoom          = 1.0f;
    float panX          = 0.0f, panY = 0.0f;
};
```

### Data Flow

1. File path received (any of three inputs)
2. `DDSLoader::Load(path)` → `std::expected<TextureData, std::string>`
   - DirectXTex loads raw DDS
   - Decompresses all BC formats to RGBA8 (or keeps RGBA16F for float formats)
   - Stores all mips × faces × slices
3. On success: `TextureRenderer::SetTexture(data, view)` uploads selected slice to `SDL_Texture`
4. Frame loop: `SDL_RenderTexture` (canvas) + `ImGui::Render` (bottom panel)
5. View state change (any control) → `TextureRenderer::UpdateView(view)` re-uploads slice

### Decode Strategy

DirectXTex `LoadFromDDSFile` followed by `Decompress` for BC formats. Internal storage:
- UNORM/integer formats → `DXGI_FORMAT_R8G8B8A8_UNORM` (8-bit per channel)
- Float formats (BC6H, R16F, R32F, R11G11B10, etc.) → `DXGI_FORMAT_R32G32B32A32_FLOAT` (32-bit per channel)

Before every SDL_Renderer upload, a CPU pass converts the selected slice to `SDL_PIXELFORMAT_RGBA32` (RGBA8):
- UNORM path: apply channel mask (zero masked channels)
- Float path: apply channel mask, apply exposure (`value * 2^EV`), linear-clamp to [0,1], convert to RGBA8

SDL_Renderer only receives RGBA8 — no float texture formats required.

## Project Structure

```
ddsviewer/
├── CMakeLists.txt
├── vcpkg.json
├── README.md
├── src/
│   ├── main.cpp
│   ├── App.h / App.cpp
│   ├── DDSLoader.h / DDSLoader.cpp
│   ├── TextureRenderer.h / TextureRenderer.cpp
│   ├── TextureView.h
│   └── UIPanel.h / UIPanel.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_dds_loader.cpp
│   ├── test_texture_view.cpp
│   └── fixtures/
│       ├── bc1_flat.dds
│       ├── bc7_flat.dds
│       ├── rgba8_uncompressed.dds
│       ├── cubemap.dds
│       ├── mipchain.dds
│       └── array.dds
└── docs/
    └── superpowers/specs/
```

## Error Handling

`DDSLoader::Load` returns `std::expected<TextureData, std::string>`. On failure, `App` displays the error string in a red status strip above the bottom panel; the previous texture (if any) remains visible.

Fatal errors (SDL init failure, renderer creation failure) print to `stderr` and `exit(1)`.

Three recoverable failure modes:
1. File not found / unreadable
2. Not a valid DDS file
3. DXGI format DirectXTex cannot decompress to RGBA

## Testing

`DDSLoader` and `TextureView` have no SDL/ImGui dependencies — GTest covers them without a graphics context.

**`test_dds_loader`:** Load each fixture file, assert `format`, `width`, `height`, `mipCount`, `arraySize`, `isCubemap`, and spot-check pixel values at known coordinates.

**`test_texture_view`:**
- Mip index clamped to `[0, mipCount-1]`
- Channel mask: all-off → treated as all-on (avoid black screen)
- Zoom clamped to `[0.05, 32.0]`
- Slice index wraps at `arraySize`

`TextureRenderer` and `UIPanel` are not unit-tested (require SDL context; they are thin wrappers).

## Build

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
# Tests
ctest --test-dir build --output-on-failure
```
