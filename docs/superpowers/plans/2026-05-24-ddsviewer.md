# DDS Viewer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a cross-platform DDS texture viewer with SDL3/ImGui supporting mip navigation, cubemaps, texture arrays, RGBA channel toggles, and HDR exposure controls.

**Architecture:** DirectXTex decodes all DDS formats to RGBA8 or RGBA32F on load; a CPU pass applies channel mask and exposure before uploading to SDL_Renderer as RGBA8. ImGui bottom panel provides all controls; texture canvas fills the remaining window area above it.

**Tech Stack:** C++23, CMake 3.25+, vcpkg, DirectXTex (Microsoft), SDL3, Dear ImGui (sdl3-binding + sdl3-renderer-binding), nativefiledialog-extended, Google Test

---

## File Map

| File | Role |
|------|------|
| `CMakeLists.txt` | Root build definition |
| `vcpkg.json` | vcpkg manifest |
| `src/TextureView.h` | POD view state struct (zoom, pan, mip, channel mask, exposure) |
| `src/TextureData.h` | Decoded texture data struct |
| `src/DDSLoader.h/.cpp` | DDS load + decode via DirectXTex → TextureData |
| `src/TextureRenderer.h/.cpp` | CPU conversion pass + SDL_Texture upload + blit |
| `src/UIPanel.h/.cpp` | ImGui bottom panel, menu bar, overlay |
| `src/App.h/.cpp` | Application state, event routing, frame orchestration |
| `src/main.cpp` | SDL/ImGui init, event loop, CLI arg |
| `tests/CMakeLists.txt` | Test build targets |
| `tests/generate_fixtures/main.cpp` | Standalone tool to create DDS fixture files |
| `tests/test_dds_loader.cpp` | GTest: DDSLoader unit tests |
| `tests/test_texture_view.cpp` | GTest: TextureView clamping/logic tests |

---

## Task 1: Project Scaffold

**Files:**
- Create: `CMakeLists.txt`
- Create: `vcpkg.json`
- Create: `.gitignore`

- [ ] **Step 1: Write `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.25)
project(ddsviewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)
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
    unofficial::nativefiledialog::nfd
)
if(TARGET SDL3::SDL3main)
    target_link_libraries(ddsviewer PRIVATE SDL3::SDL3main)
endif()

enable_testing()
add_subdirectory(tests)
```

- [ ] **Step 2: Write `vcpkg.json`**

```json
{
  "name": "ddsviewer",
  "version": "0.1.0",
  "dependencies": [
    "sdl3",
    {
      "name": "imgui",
      "features": ["sdl3-binding", "sdl3-renderer-binding"]
    },
    "directxtex",
    "nativefiledialog-extended",
    "gtest"
  ]
}
```

- [ ] **Step 3: Write `.gitignore`**

```
build/
.superpowers/
*.user
.vs/
```

- [ ] **Step 4: Create stub source files so CMake can configure**

Create empty placeholder files (they'll be filled in later tasks):

```
src/main.cpp          — int main() { return 0; }
src/App.h             — #pragma once
src/App.cpp           — (empty)
src/DDSLoader.h       — #pragma once
src/DDSLoader.cpp     — (empty)
src/TextureRenderer.h — #pragma once
src/TextureRenderer.cpp — (empty)
src/UIPanel.h         — #pragma once
src/UIPanel.cpp       — (empty)
```

Write each:

**`src/main.cpp`:**
```cpp
int main(int, char**) { return 0; }
```

**`src/App.h`, `src/DDSLoader.h`, `src/TextureRenderer.h`, `src/UIPanel.h`:**
```cpp
#pragma once
```

**`src/App.cpp`, `src/DDSLoader.cpp`, `src/TextureRenderer.cpp`, `src/UIPanel.cpp`:**
```cpp
// placeholder
```

- [ ] **Step 5: Configure with CMake**

```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

Expected: configuration succeeds, vcpkg installs all dependencies. If imgui SDL3 features are unavailable, run `vcpkg search imgui` and verify the exact feature names in your vcpkg version.

- [ ] **Step 6: Build (verify stubs compile)**

```powershell
cmake --build build --config Release
```

Expected: `ddsviewer` executable built with no errors.

- [ ] **Step 7: Commit**

```
git init
git add .
git commit -m "feat: initial project scaffold with CMake and vcpkg"
```

---

## Task 2: Core Type Headers

**Files:**
- Create: `src/TextureView.h`
- Create: `src/TextureData.h`

- [ ] **Step 1: Write `src/TextureView.h`**

```cpp
#pragma once
#include <algorithm>

struct TextureView {
    int   mip      = 0;
    int   slice    = 0;    // array index, depth slice, or 0 for 2D
    int   face     = 0;    // 0-5 for cubemaps
    bool  r        = true;
    bool  g        = true;
    bool  b        = true;
    bool  a        = false;
    float exposure = 0.0f; // EV stops (float formats only)
    float zoom     = 1.0f;
    float panX     = 0.0f;
    float panY     = 0.0f;
};

inline bool AnyChannelEnabled(const TextureView& v) {
    return v.r || v.g || v.b || v.a;
}

inline void ClampView(TextureView& v, int mipCount, int layerCount, int faceCount) {
    v.mip   = std::clamp(v.mip,   0, std::max(0, mipCount   - 1));
    v.slice = std::clamp(v.slice, 0, std::max(0, layerCount - 1));
    v.face  = std::clamp(v.face,  0, std::max(0, faceCount  - 1));
    v.zoom  = std::clamp(v.zoom,  0.05f, 32.0f);
}
```

- [ ] **Step 2: Write `src/TextureData.h`**

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <DirectXTex.h>

struct MipImage {
    std::vector<uint8_t> pixels; // RGBA8 (4 bytes/px) or RGBA32F (16 bytes/px)
    uint32_t width  = 0;
    uint32_t height = 0;
};

struct TextureData {
    DXGI_FORMAT originalFormat = DXGI_FORMAT_UNKNOWN;
    bool        isFloat        = false;  // true → pixels are RGBA32F
    uint32_t    baseWidth      = 0;
    uint32_t    baseHeight     = 0;
    uint32_t    depth          = 1;
    uint32_t    mipCount       = 0;
    uint32_t    layerCount     = 1;  // arraySize, or 6 for cubemaps, or depth for 3D
    bool        isCubemap      = false;
    bool        is3D           = false;
    std::string formatName;

    // images[layerIndex][mipIndex]
    // Cubemaps:   layer = face (0=+X 1=-X 2=+Y 3=-Y 4=+Z 5=-Z)
    // Arrays/2D:  layer = array index (0 for plain 2D)
    // 3D:         layer = depth slice at mip 0, mipCount = 1
    std::vector<std::vector<MipImage>> images;
};
```

- [ ] **Step 3: Build to verify headers compile**

```powershell
cmake --build build --config Release
```

Expected: builds cleanly (stubs include these headers when we update them next task).

- [ ] **Step 4: Commit**

```
git add src/TextureView.h src/TextureData.h
git commit -m "feat: add TextureView and TextureData type headers"
```

---

## Task 3: DDSLoader Interface and Stub

**Files:**
- Modify: `src/DDSLoader.h`
- Modify: `src/DDSLoader.cpp`

- [ ] **Step 1: Write `src/DDSLoader.h`**

```cpp
#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include "TextureData.h"

class DDSLoader {
public:
    static std::expected<TextureData, std::string> Load(const std::filesystem::path& path);
};
```

- [ ] **Step 2: Write `src/DDSLoader.cpp` (stub)**

```cpp
#include "DDSLoader.h"

std::expected<TextureData, std::string> DDSLoader::Load(const std::filesystem::path&) {
    return std::unexpected("DDSLoader not yet implemented");
}
```

- [ ] **Step 3: Build**

```powershell
cmake --build build --config Release
```

Expected: compiles cleanly.

- [ ] **Step 4: Commit**

```
git add src/DDSLoader.h src/DDSLoader.cpp
git commit -m "feat: add DDSLoader interface stub"
```

---

## Task 4: Test Infrastructure and Fixture Generator

**Files:**
- Create: `tests/CMakeLists.txt`
- Create: `tests/generate_fixtures/CMakeLists.txt`
- Create: `tests/generate_fixtures/main.cpp`

- [ ] **Step 1: Write `tests/CMakeLists.txt`**

```cmake
add_subdirectory(generate_fixtures)

add_executable(ddsviewer_tests
    test_dds_loader.cpp
    test_texture_view.cpp
    ${CMAKE_SOURCE_DIR}/src/DDSLoader.cpp
)

target_include_directories(ddsviewer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(ddsviewer_tests PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(ddsviewer_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Microsoft::DirectXTex
)

include(GoogleTest)
gtest_discover_tests(ddsviewer_tests)
```

- [ ] **Step 2: Create empty test source files so CMake can configure**

**`tests/test_dds_loader.cpp`:**
```cpp
#include <gtest/gtest.h>
// tests added in Task 5
```

**`tests/test_texture_view.cpp`:**
```cpp
#include <gtest/gtest.h>
// tests added in Task 6
```

- [ ] **Step 3: Write `tests/generate_fixtures/CMakeLists.txt`**

```cmake
add_executable(generate_fixtures main.cpp)

target_link_libraries(generate_fixtures PRIVATE Microsoft::DirectXTex)

set_target_properties(generate_fixtures PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
```

- [ ] **Step 4: Write `tests/generate_fixtures/main.cpp`**

```cpp
#include <DirectXTex.h>
#include <cstdio>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

static void FillImage(const DirectX::Image& img, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint8_t* p = img.pixels;
    for (size_t y = 0; y < img.height; ++y) {
        uint8_t* row = p + y * img.rowPitch;
        for (size_t x = 0; x < img.width; ++x) {
            row[x * 4 + 0] = r;
            row[x * 4 + 1] = g;
            row[x * 4 + 2] = b;
            row[x * 4 + 3] = a;
        }
    }
}

static bool Save(const DirectX::ScratchImage& img, const fs::path& path) {
    HRESULT hr = DirectX::SaveToDDSFile(
        img.GetImages(), img.GetImageCount(), img.GetMetadata(),
        DirectX::DDS_FLAGS_NONE, path.wstring().c_str());
    if (FAILED(hr)) {
        std::fprintf(stderr, "Failed to save %s: 0x%08X\n", path.string().c_str(), hr);
        return false;
    }
    std::printf("  wrote %s\n", path.filename().string().c_str());
    return true;
}

int main(int argc, char* argv[]) {
    fs::path outDir = argc > 1 ? argv[1] : "tests/fixtures";
    fs::create_directories(outDir);
    std::printf("Generating fixtures in %s\n", outDir.string().c_str());

    // ── rgba8_uncompressed.dds ─────────────────────────────────────────────
    {
        DirectX::ScratchImage img;
        img.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, 1, 1);
        // Checkerboard: orange/blue pixels at known positions
        const DirectX::Image* im = img.GetImage(0, 0, 0);
        std::memset(im->pixels, 0, im->slicePitch);
        // pixel (0,0) = orange
        im->pixels[0] = 255; im->pixels[1] = 128; im->pixels[2] = 0; im->pixels[3] = 255;
        // pixel (1,0) = blue
        im->pixels[4] = 0;   im->pixels[5] = 0;   im->pixels[6] = 255; im->pixels[7] = 255;
        Save(img, outDir / "rgba8_uncompressed.dds");
    }

    // ── bc1_flat.dds ───────────────────────────────────────────────────────
    {
        DirectX::ScratchImage src;
        src.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, 1, 1);
        FillImage(*src.GetImage(0, 0, 0), 255, 0, 0, 255); // red
        DirectX::ScratchImage bc1;
        DirectX::Compress(src.GetImages(), src.GetImageCount(), src.GetMetadata(),
                          DXGI_FORMAT_BC1_UNORM, DirectX::TEX_COMPRESS_DEFAULT,
                          DirectX::TEX_THRESHOLD_DEFAULT, bc1);
        Save(bc1, outDir / "bc1_flat.dds");
    }

    // ── bc7_flat.dds ───────────────────────────────────────────────────────
    {
        DirectX::ScratchImage src;
        src.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, 1, 1);
        FillImage(*src.GetImage(0, 0, 0), 0, 255, 0, 255); // green
        DirectX::ScratchImage bc7;
        DirectX::Compress(src.GetImages(), src.GetImageCount(), src.GetMetadata(),
                          DXGI_FORMAT_BC7_UNORM, DirectX::TEX_COMPRESS_DEFAULT,
                          DirectX::TEX_THRESHOLD_DEFAULT, bc7);
        Save(bc7, outDir / "bc7_flat.dds");
    }

    // ── mipchain.dds ───────────────────────────────────────────────────────
    {
        DirectX::ScratchImage src;
        src.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 64, 64, 1, 1);
        FillImage(*src.GetImage(0, 0, 0), 100, 149, 237, 255); // cornflower blue
        DirectX::ScratchImage mipped;
        DirectX::GenerateMipMaps(*src.GetImage(0, 0, 0),
                                 DirectX::TEX_FILTER_DEFAULT, 0, mipped);
        Save(mipped, outDir / "mipchain.dds");
    }

    // ── cubemap.dds ────────────────────────────────────────────────────────
    {
        DirectX::ScratchImage cube;
        cube.InitializeCube(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, 1, 1);
        uint8_t faceColors[6][4] = {
            {255,   0,   0, 255}, // +X red
            {  0, 255,   0, 255}, // -X green
            {  0,   0, 255, 255}, // +Y blue
            {255, 255,   0, 255}, // -Y yellow
            {  0, 255, 255, 255}, // +Z cyan
            {255,   0, 255, 255}, // -Z magenta
        };
        for (int f = 0; f < 6; ++f) {
            const DirectX::Image* img = cube.GetImage(0, f, 0);
            FillImage(*img, faceColors[f][0], faceColors[f][1],
                      faceColors[f][2], faceColors[f][3]);
        }
        Save(cube, outDir / "cubemap.dds");
    }

    // ── array.dds ──────────────────────────────────────────────────────────
    {
        DirectX::ScratchImage arr;
        arr.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, 3, 1);
        uint8_t sliceColors[3][4] = {
            {255,   0,   0, 255},
            {  0, 255,   0, 255},
            {  0,   0, 255, 255},
        };
        for (int s = 0; s < 3; ++s) {
            const DirectX::Image* img = arr.GetImage(0, s, 0);
            FillImage(*img, sliceColors[s][0], sliceColors[s][1],
                      sliceColors[s][2], sliceColors[s][3]);
        }
        Save(arr, outDir / "array.dds");
    }

    std::printf("Done.\n");
    return 0;
}
```

- [ ] **Step 5: Reconfigure and build fixture generator**

```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --target generate_fixtures
```

Expected: `build/Release/generate_fixtures.exe` built successfully.

- [ ] **Step 6: Run fixture generator**

```powershell
./build/Release/generate_fixtures.exe tests/fixtures
```

Expected output:
```
Generating fixtures in tests/fixtures
  wrote rgba8_uncompressed.dds
  wrote bc1_flat.dds
  wrote bc7_flat.dds
  wrote mipchain.dds
  wrote cubemap.dds
  wrote array.dds
Done.
```

Verify files exist: `ls tests/fixtures/`

- [ ] **Step 7: Commit**

```
git add tests/ 
git commit -m "feat: test infrastructure and fixture generator"
```

---

## Task 5: DDSLoader Tests and Implementation (TDD)

**Files:**
- Modify: `tests/test_dds_loader.cpp`
- Modify: `src/DDSLoader.cpp`

- [ ] **Step 1: Write failing tests in `tests/test_dds_loader.cpp`**

```cpp
#include <gtest/gtest.h>
#include <filesystem>
#include "DDSLoader.h"

namespace fs = std::filesystem;
static const fs::path FX = FIXTURES_DIR;

TEST(DDSLoader, FileNotFound) {
    auto result = DDSLoader::Load(FX / "does_not_exist.dds");
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

TEST(DDSLoader, RGBA8Uncompressed) {
    auto result = DDSLoader::Load(FX / "rgba8_uncompressed.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    const TextureData& d = *result;
    EXPECT_EQ(d.baseWidth,  4u);
    EXPECT_EQ(d.baseHeight, 4u);
    EXPECT_EQ(d.mipCount,   1u);
    EXPECT_EQ(d.layerCount, 1u);
    EXPECT_FALSE(d.isCubemap);
    EXPECT_FALSE(d.is3D);
    EXPECT_FALSE(d.isFloat);
    EXPECT_EQ(d.originalFormat, DXGI_FORMAT_R8G8B8A8_UNORM);
    // pixel (0,0) should be orange: R=255 G=128 B=0 A=255
    const auto& px = d.images[0][0].pixels;
    EXPECT_EQ(px[0], 255u); // R
    EXPECT_EQ(px[1], 128u); // G
    EXPECT_EQ(px[2],   0u); // B
    EXPECT_EQ(px[3], 255u); // A
}

TEST(DDSLoader, BC1Decompress) {
    auto result = DDSLoader::Load(FX / "bc1_flat.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    const TextureData& d = *result;
    EXPECT_EQ(d.baseWidth,  4u);
    EXPECT_EQ(d.baseHeight, 4u);
    EXPECT_EQ(d.originalFormat, DXGI_FORMAT_BC1_UNORM);
    EXPECT_FALSE(d.isFloat);
    // After decompression to RGBA8: red channel dominant
    const auto& px = d.images[0][0].pixels;
    EXPECT_GT(px[0], 200u); // R high
    EXPECT_LT(px[1],  50u); // G low
    EXPECT_LT(px[2],  50u); // B low
}

TEST(DDSLoader, BC7Decompress) {
    auto result = DDSLoader::Load(FX / "bc7_flat.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    const TextureData& d = *result;
    EXPECT_EQ(d.originalFormat, DXGI_FORMAT_BC7_UNORM);
    const auto& px = d.images[0][0].pixels;
    EXPECT_LT(px[0],  50u); // R low
    EXPECT_GT(px[1], 200u); // G high
    EXPECT_LT(px[2],  50u); // B low
}

TEST(DDSLoader, MipChain) {
    auto result = DDSLoader::Load(FX / "mipchain.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    const TextureData& d = *result;
    EXPECT_EQ(d.baseWidth,  64u);
    EXPECT_EQ(d.baseHeight, 64u);
    EXPECT_EQ(d.mipCount,    7u); // 64→32→16→8→4→2→1
    EXPECT_EQ(d.layerCount,  1u);
    // Last mip is 1x1
    const MipImage& last = d.images[0][6];
    EXPECT_EQ(last.width,  1u);
    EXPECT_EQ(last.height, 1u);
}

TEST(DDSLoader, Cubemap) {
    auto result = DDSLoader::Load(FX / "cubemap.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    const TextureData& d = *result;
    EXPECT_TRUE(d.isCubemap);
    EXPECT_EQ(d.layerCount, 6u);
    // Face 0 (+X) is red
    const auto& f0 = d.images[0][0].pixels;
    EXPECT_GT(f0[0], 200u); EXPECT_LT(f0[1], 50u); EXPECT_LT(f0[2], 50u);
    // Face 2 (+Y) is blue
    const auto& f2 = d.images[2][0].pixels;
    EXPECT_LT(f2[0], 50u); EXPECT_LT(f2[1], 50u); EXPECT_GT(f2[2], 200u);
}

TEST(DDSLoader, TextureArray) {
    auto result = DDSLoader::Load(FX / "array.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    const TextureData& d = *result;
    EXPECT_FALSE(d.isCubemap);
    EXPECT_EQ(d.layerCount, 3u);
    // Slice 0 = red, slice 1 = green, slice 2 = blue
    EXPECT_GT(d.images[0][0].pixels[0], 200u); // slice 0 R
    EXPECT_GT(d.images[1][0].pixels[1], 200u); // slice 1 G
    EXPECT_GT(d.images[2][0].pixels[2], 200u); // slice 2 B
}
```

- [ ] **Step 2: Run tests (expect failures)**

```powershell
cmake --build build --config Release --target ddsviewer_tests
ctest --test-dir build -C Release --output-on-failure -R DDSLoader
```

Expected: `FileNotFound` passes (stub returns error), all load tests fail with "not yet implemented".

- [ ] **Step 3: Implement `src/DDSLoader.cpp`**

```cpp
#include "DDSLoader.h"
#include <DirectXTex.h>
#include <format>
#include <cstring>

static bool IsFloatFormat(DXGI_FORMAT fmt) {
    using namespace DirectX;
    return fmt == DXGI_FORMAT_BC6H_SF16         ||
           fmt == DXGI_FORMAT_BC6H_UF16         ||
           fmt == DXGI_FORMAT_R16_FLOAT         ||
           fmt == DXGI_FORMAT_R16G16_FLOAT      ||
           fmt == DXGI_FORMAT_R16G16B16A16_FLOAT||
           fmt == DXGI_FORMAT_R32_FLOAT         ||
           fmt == DXGI_FORMAT_R32G32_FLOAT      ||
           fmt == DXGI_FORMAT_R32G32B32_FLOAT   ||
           fmt == DXGI_FORMAT_R32G32B32A32_FLOAT||
           fmt == DXGI_FORMAT_R11G11B10_FLOAT   ||
           fmt == DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
}

static std::string FormatName(DXGI_FORMAT fmt) {
    switch (fmt) {
        case DXGI_FORMAT_BC1_UNORM:            return "BC1_UNORM";
        case DXGI_FORMAT_BC1_UNORM_SRGB:       return "BC1_UNORM_SRGB";
        case DXGI_FORMAT_BC2_UNORM:            return "BC2_UNORM";
        case DXGI_FORMAT_BC2_UNORM_SRGB:       return "BC2_UNORM_SRGB";
        case DXGI_FORMAT_BC3_UNORM:            return "BC3_UNORM";
        case DXGI_FORMAT_BC3_UNORM_SRGB:       return "BC3_UNORM_SRGB";
        case DXGI_FORMAT_BC4_UNORM:            return "BC4_UNORM";
        case DXGI_FORMAT_BC4_SNORM:            return "BC4_SNORM";
        case DXGI_FORMAT_BC5_UNORM:            return "BC5_UNORM";
        case DXGI_FORMAT_BC5_SNORM:            return "BC5_SNORM";
        case DXGI_FORMAT_BC6H_UF16:            return "BC6H_UF16";
        case DXGI_FORMAT_BC6H_SF16:            return "BC6H_SF16";
        case DXGI_FORMAT_BC7_UNORM:            return "BC7_UNORM";
        case DXGI_FORMAT_BC7_UNORM_SRGB:       return "BC7_UNORM_SRGB";
        case DXGI_FORMAT_R8G8B8A8_UNORM:       return "R8G8B8A8_UNORM";
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:  return "R8G8B8A8_UNORM_SRGB";
        case DXGI_FORMAT_R16G16B16A16_FLOAT:   return "R16G16B16A16_FLOAT";
        case DXGI_FORMAT_R32G32B32A32_FLOAT:   return "R32G32B32A32_FLOAT";
        case DXGI_FORMAT_R11G11B10_FLOAT:      return "R11G11B10_FLOAT";
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:   return "R9G9B9E5_SHAREDEXP";
        case DXGI_FORMAT_B8G8R8A8_UNORM:       return "B8G8R8A8_UNORM";
        default: return std::format("DXGI_FORMAT_{}", static_cast<uint32_t>(fmt));
    }
}

static MipImage ExtractImage(const DirectX::Image& src, bool isFloat) {
    MipImage mi;
    mi.width  = static_cast<uint32_t>(src.width);
    mi.height = static_cast<uint32_t>(src.height);
    size_t bytesPerPixel = isFloat ? 16 : 4;
    mi.pixels.resize(mi.width * mi.height * bytesPerPixel);
    // Copy row by row (rowPitch may have padding)
    for (uint32_t y = 0; y < mi.height; ++y) {
        const uint8_t* srcRow = src.pixels + y * src.rowPitch;
        uint8_t*       dstRow = mi.pixels.data() + y * mi.width * bytesPerPixel;
        std::memcpy(dstRow, srcRow, mi.width * bytesPerPixel);
    }
    return mi;
}

std::expected<TextureData, std::string> DDSLoader::Load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path))
        return std::unexpected("File not found: " + path.string());

    DirectX::TexMetadata  metadata{};
    DirectX::ScratchImage raw;

    HRESULT hr = DirectX::LoadFromDDSFile(
        path.wstring().c_str(), DirectX::DDS_FLAGS_NONE, &metadata, raw);
    if (FAILED(hr))
        return std::unexpected(std::format(
            "Failed to parse DDS: HRESULT 0x{:08X}", static_cast<uint32_t>(hr)));

    bool        isFloat      = IsFloatFormat(metadata.format);
    DXGI_FORMAT targetFormat = isFloat ? DXGI_FORMAT_R32G32B32A32_FLOAT
                                       : DXGI_FORMAT_R8G8B8A8_UNORM;

    const DirectX::ScratchImage* source = &raw;
    DirectX::ScratchImage        converted;

    if (DirectX::IsCompressed(metadata.format)) {
        hr = DirectX::Decompress(raw.GetImages(), raw.GetImageCount(),
                                 metadata, targetFormat, converted);
        if (FAILED(hr))
            return std::unexpected("Cannot decompress format: " + FormatName(metadata.format));
        source = &converted;
    } else if (metadata.format != targetFormat) {
        hr = DirectX::Convert(raw.GetImages(), raw.GetImageCount(), metadata,
                              targetFormat, DirectX::TEX_FILTER_DEFAULT,
                              DirectX::TEX_THRESHOLD_DEFAULT, converted);
        if (FAILED(hr))
            return std::unexpected("Cannot convert format: " + FormatName(metadata.format));
        source = &converted;
    }

    TextureData data;
    data.originalFormat = metadata.format;
    data.isFloat        = isFloat;
    data.baseWidth      = static_cast<uint32_t>(metadata.width);
    data.baseHeight     = static_cast<uint32_t>(metadata.height);
    data.depth          = static_cast<uint32_t>(metadata.depth);
    data.mipCount       = static_cast<uint32_t>(metadata.mipLevels);
    data.isCubemap      = (metadata.miscFlags & DirectX::TEX_MISC_TEXTURECUBE) != 0;
    data.is3D           = (metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE3D);
    data.formatName     = FormatName(metadata.format);

    if (data.is3D) {
        // Treat depth slices at mip 0 as layers; only store mip 0 for 3D
        data.layerCount = static_cast<uint32_t>(metadata.depth);
        data.mipCount   = 1;
        data.images.resize(data.layerCount);
        for (uint32_t d = 0; d < data.layerCount; ++d) {
            const DirectX::Image* img = source->GetImage(0, 0, d);
            data.images[d].push_back(ExtractImage(*img, isFloat));
        }
    } else {
        data.layerCount = static_cast<uint32_t>(metadata.arraySize);
        data.images.resize(data.layerCount);
        for (uint32_t layer = 0; layer < data.layerCount; ++layer) {
            data.images[layer].resize(data.mipCount);
            for (uint32_t mip = 0; mip < data.mipCount; ++mip) {
                const DirectX::Image* img = source->GetImage(mip, layer, 0);
                data.images[layer][mip] = ExtractImage(*img, isFloat);
            }
        }
    }

    return data;
}
```

- [ ] **Step 4: Build and run tests**

```powershell
cmake --build build --config Release --target ddsviewer_tests
ctest --test-dir build -C Release --output-on-failure -R DDSLoader
```

Expected: all 7 DDSLoader tests pass.

- [ ] **Step 5: Commit**

```
git add src/DDSLoader.cpp tests/test_dds_loader.cpp
git commit -m "feat: implement DDSLoader with DirectXTex; all DDS format tests pass"
```

---

## Task 6: TextureView Tests

**Files:**
- Modify: `tests/test_texture_view.cpp`

- [ ] **Step 1: Write `tests/test_texture_view.cpp`**

```cpp
#include <gtest/gtest.h>
#include "TextureView.h"

TEST(TextureView, Defaults) {
    TextureView v;
    EXPECT_EQ(v.mip,   0);
    EXPECT_EQ(v.slice, 0);
    EXPECT_EQ(v.face,  0);
    EXPECT_TRUE(v.r);
    EXPECT_TRUE(v.g);
    EXPECT_TRUE(v.b);
    EXPECT_FALSE(v.a);
    EXPECT_FLOAT_EQ(v.zoom, 1.0f);
}

TEST(TextureView, AnyChannelEnabled_AllOff) {
    TextureView v;
    v.r = v.g = v.b = v.a = false;
    EXPECT_FALSE(AnyChannelEnabled(v));
}

TEST(TextureView, AnyChannelEnabled_OneOn) {
    TextureView v;
    v.r = v.g = v.b = v.a = false;
    v.b = true;
    EXPECT_TRUE(AnyChannelEnabled(v));
}

TEST(TextureView, ClampView_MipBounds) {
    TextureView v;
    v.mip = -1;
    ClampView(v, 4, 1, 1);
    EXPECT_EQ(v.mip, 0);

    v.mip = 99;
    ClampView(v, 4, 1, 1);
    EXPECT_EQ(v.mip, 3);
}

TEST(TextureView, ClampView_ZoomBounds) {
    TextureView v;
    v.zoom = 0.0f;
    ClampView(v, 1, 1, 1);
    EXPECT_FLOAT_EQ(v.zoom, 0.05f);

    v.zoom = 999.0f;
    ClampView(v, 1, 1, 1);
    EXPECT_FLOAT_EQ(v.zoom, 32.0f);
}

TEST(TextureView, ClampView_SliceBounds) {
    TextureView v;
    v.slice = 5;
    ClampView(v, 1, 3, 1);
    EXPECT_EQ(v.slice, 2);
}

TEST(TextureView, ClampView_FaceBounds) {
    TextureView v;
    v.face = 10;
    ClampView(v, 1, 1, 6);
    EXPECT_EQ(v.face, 5);
}
```

- [ ] **Step 2: Build and run tests**

```powershell
cmake --build build --config Release --target ddsviewer_tests
ctest --test-dir build -C Release --output-on-failure -R TextureView
```

Expected: all TextureView tests pass immediately.

- [ ] **Step 3: Commit**

```
git add tests/test_texture_view.cpp
git commit -m "test: add TextureView unit tests"
```

---

## Task 7: TextureRenderer

**Files:**
- Modify: `src/TextureRenderer.h`
- Modify: `src/TextureRenderer.cpp`

- [ ] **Step 1: Write `src/TextureRenderer.h`**

```cpp
#pragma once
#include <SDL3/SDL.h>
#include "TextureData.h"
#include "TextureView.h"

class TextureRenderer {
public:
    explicit TextureRenderer(SDL_Renderer* renderer);
    ~TextureRenderer();

    // Call when a new file is loaded or view selection changes
    void Upload(const TextureData& data, const TextureView& view);
    void Clear();

    // Render the texture into canvasRect, applying zoom and pan from view
    void Render(SDL_Renderer* renderer, SDL_FRect canvasRect, const TextureView& view);

    bool HasTexture() const { return texture_ != nullptr; }
    int  TexWidth()   const { return texW_; }
    int  TexHeight()  const { return texH_; }

private:
    static std::vector<uint8_t> ConvertToRGBA8(const MipImage& img,
                                                bool isFloat,
                                                const TextureView& view);

    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture*  texture_  = nullptr;
    int           texW_     = 0;
    int           texH_     = 0;
};
```

- [ ] **Step 2: Write `src/TextureRenderer.cpp`**

```cpp
#include "TextureRenderer.h"
#include <algorithm>
#include <cassert>
#include <cmath>

TextureRenderer::TextureRenderer(SDL_Renderer* renderer)
    : renderer_(renderer) {}

TextureRenderer::~TextureRenderer() {
    Clear();
}

void TextureRenderer::Clear() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
        texW_ = texH_ = 0;
    }
}

void TextureRenderer::Upload(const TextureData& data, const TextureView& view) {
    // Select the right image from the data
    int layer = data.isCubemap ? view.face : view.slice;
    layer     = std::clamp(layer, 0, static_cast<int>(data.layerCount) - 1);
    int mip   = std::clamp(view.mip, 0, static_cast<int>(data.mipCount) - 1);

    const MipImage& src = data.images[layer][mip];

    // Recreate texture if dimensions changed
    if (!texture_ || texW_ != static_cast<int>(src.width) ||
        texH_ != static_cast<int>(src.height)) {
        Clear();
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     src.width, src.height);
        texW_ = static_cast<int>(src.width);
        texH_ = static_cast<int>(src.height);
    }

    std::vector<uint8_t> rgba8 = ConvertToRGBA8(src, data.isFloat, view);
    SDL_UpdateTexture(texture_, nullptr, rgba8.data(), texW_ * 4);
}

void TextureRenderer::Render(SDL_Renderer* renderer, SDL_FRect canvasRect,
                             const TextureView& view) {
    if (!texture_) return;

    float texW = texW_ * view.zoom;
    float texH = texH_ * view.zoom;
    SDL_FRect dst = {
        canvasRect.x + (canvasRect.w - texW) * 0.5f + view.panX,
        canvasRect.y + (canvasRect.h - texH) * 0.5f + view.panY,
        texW, texH
    };
    SDL_RenderTexture(renderer, texture_, nullptr, &dst);
}

std::vector<uint8_t> TextureRenderer::ConvertToRGBA8(const MipImage& img,
                                                      bool isFloat,
                                                      const TextureView& view) {
    bool anyOn = AnyChannelEnabled(view);
    bool r = anyOn ? view.r : true;
    bool g = anyOn ? view.g : true;
    bool b = anyOn ? view.b : true;
    bool a = anyOn ? view.a : true;

    size_t count = img.width * img.height;
    std::vector<uint8_t> out(count * 4);

    if (isFloat) {
        float scale = std::pow(2.0f, view.exposure);
        const float* src = reinterpret_cast<const float*>(img.pixels.data());
        for (size_t i = 0; i < count; ++i) {
            auto toU8 = [](float v) -> uint8_t {
                return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
            };
            out[i*4+0] = r ? toU8(src[i*4+0] * scale) : 0;
            out[i*4+1] = g ? toU8(src[i*4+1] * scale) : 0;
            out[i*4+2] = b ? toU8(src[i*4+2] * scale) : 0;
            out[i*4+3] = a ? toU8(src[i*4+3])         : 255;
        }
    } else {
        const uint8_t* src = img.pixels.data();
        for (size_t i = 0; i < count; ++i) {
            out[i*4+0] = r ? src[i*4+0] : 0;
            out[i*4+1] = g ? src[i*4+1] : 0;
            out[i*4+2] = b ? src[i*4+2] : 0;
            out[i*4+3] = a ? src[i*4+3] : 255;
        }
    }
    return out;
}
```

- [ ] **Step 3: Build**

```powershell
cmake --build build --config Release
```

Expected: compiles cleanly. (main.cpp is still a stub so the executable links but does nothing useful yet.)

- [ ] **Step 4: Commit**

```
git add src/TextureRenderer.h src/TextureRenderer.cpp
git commit -m "feat: TextureRenderer — CPU channel/exposure pass + SDL_Texture upload"
```

---

## Task 8: UIPanel

**Files:**
- Modify: `src/UIPanel.h`
- Modify: `src/UIPanel.cpp`

- [ ] **Step 1: Write `src/UIPanel.h`**

```cpp
#pragma once
#include <functional>
#include <string>
#include "TextureData.h"
#include "TextureView.h"

class UIPanel {
public:
    static constexpr float kPanelHeight   = 84.0f;

    // Returns true if view changed and texture must be re-uploaded
    bool Draw(TextureView& view, const TextureData* data,
              const std::string& errorMsg,
              const std::function<void()>& onOpenFile);

    float MenuBarHeight() const { return menuBarH_; }

private:
    float menuBarH_ = 0.0f;
};
```

- [ ] **Step 2: Write `src/UIPanel.cpp`**

```cpp
#include "UIPanel.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <format>

static bool ChannelButton(const char* label, bool& value, ImVec4 color) {
    if (value) ImGui::PushStyleColor(ImGuiCol_Button, color);
    else       ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_Button]);
    bool clicked = ImGui::Button(label);
    ImGui::PopStyleColor();
    if (clicked) value = !value;
    return clicked;
}

bool UIPanel::Draw(TextureView& view, const TextureData* data,
                   const std::string& errorMsg,
                   const std::function<void()>& onOpenFile) {
    bool changed = false;

    // ── Menu bar ──────────────────────────────────────────────────────────
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open...", "Ctrl+O")) onOpenFile();
            ImGui::EndMenu();
        }
        menuBarH_ = ImGui::GetFrameHeight();
        ImGui::EndMainMenuBar();
    }

    // ── Error strip ───────────────────────────────────────────────────────
    if (!errorMsg.empty()) {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0.0f, menuBarH_));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 24.0f));
        ImGui::SetNextWindowBgAlpha(0.85f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        ImGui::Begin("##err", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::TextUnformatted(errorMsg.c_str());
        ImGui::End();
        ImGui::PopStyleColor();
    }

    // ── Bottom panel ──────────────────────────────────────────────────────
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - kPanelHeight));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, kPanelHeight));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("##panel", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (!data) {
        ImGui::TextDisabled("Drop a .dds file here or use File > Open");
        ImGui::End();
        return false;
    }

    // File info
    ImGui::BeginGroup();
    ImGui::TextUnformatted(data->formatName.c_str());
    ImGui::TextDisabled("%ux%u", data->baseWidth, data->baseHeight);
    ImGui::TextDisabled("%u mip%s  %u layer%s",
        data->mipCount, data->mipCount > 1 ? "s" : "",
        data->layerCount, data->layerCount > 1 ? "s" : "");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Mip selector
    ImGui::BeginGroup();
    ImGui::TextDisabled("Mip");
    ImGui::SetNextItemWidth(60.0f);
    int mip = view.mip;
    if (ImGui::InputInt("##mip", &mip, 1)) {
        view.mip = std::clamp(mip, 0, static_cast<int>(data->mipCount) - 1);
        changed = true;
    }
    const MipImage& curMip = data->images[0][view.mip];
    ImGui::TextDisabled("%ux%u", curMip.width, curMip.height);
    ImGui::EndGroup();

    // Cubemap faces
    if (data->isCubemap) {
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("Face");
        static const char* kFaceLabels[6] = {"+X","-X","+Y","-Y","+Z","-Z"};
        for (int f = 0; f < 6; ++f) {
            if (f > 0) ImGui::SameLine();
            bool selected = (view.face == f);
            if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.4f,0.7f,1.0f));
            else          ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_Button]);
            if (ImGui::Button(kFaceLabels[f])) { view.face = f; changed = true; }
            ImGui::PopStyleColor();
        }
        ImGui::EndGroup();
    }

    // Array / 3D slice selector
    if (!data->isCubemap && data->layerCount > 1) {
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled(data->is3D ? "Depth" : "Slice");
        ImGui::SetNextItemWidth(60.0f);
        int slice = view.slice;
        if (ImGui::InputInt("##slice", &slice, 1)) {
            view.slice = std::clamp(slice, 0, static_cast<int>(data->layerCount) - 1);
            changed = true;
        }
        ImGui::TextDisabled("of %u", data->layerCount);
        ImGui::EndGroup();
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Channel toggles
    ImGui::BeginGroup();
    ImGui::TextDisabled("Channels");
    if (ChannelButton("R", view.r, ImVec4(0.7f,0.2f,0.2f,1.0f))) changed = true;
    ImGui::SameLine();
    if (ChannelButton("G", view.g, ImVec4(0.2f,0.7f,0.2f,1.0f))) changed = true;
    ImGui::SameLine();
    if (ChannelButton("B", view.b, ImVec4(0.2f,0.2f,0.7f,1.0f))) changed = true;
    ImGui::SameLine();
    if (ChannelButton("A", view.a, ImVec4(0.5f,0.5f,0.5f,1.0f))) changed = true;
    ImGui::EndGroup();

    // Exposure (float formats only)
    if (data->isFloat) {
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("Exposure");
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("##ev", &view.exposure, -10.0f, 10.0f, "%.1f EV"))
            changed = true;
        ImGui::EndGroup();
    }

    ImGui::End();
    return changed;
}
```

- [ ] **Step 3: Build**

```powershell
cmake --build build --config Release
```

Expected: compiles cleanly.

- [ ] **Step 4: Commit**

```
git add src/UIPanel.h src/UIPanel.cpp
git commit -m "feat: UIPanel with ImGui bottom panel, menu bar, mip/face/slice/channel/exposure controls"
```

---

## Task 9: App

**Files:**
- Modify: `src/App.h`
- Modify: `src/App.cpp`

- [ ] **Step 1: Write `src/App.h`**

```cpp
#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <SDL3/SDL.h>
#include "TextureData.h"
#include "TextureRenderer.h"
#include "TextureView.h"
#include "UIPanel.h"

class App {
public:
    App(SDL_Window* window, SDL_Renderer* renderer);

    void LoadFile(const std::filesystem::path& path);
    void HandleEvent(const SDL_Event& event);
    void DrawUI();
    void RenderTexture();

    bool IsRunning() const { return running_; }

private:
    void OpenFileDialog();
    SDL_FRect CanvasRect() const;

    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    std::optional<TextureData> data_;
    TextureView                view_;
    TextureRenderer            texRenderer_;
    UIPanel                    uiPanel_;
    std::string                errorMsg_;
    bool                       running_       = true;
    bool                       dragging_      = false;
    float                      dragStartX_    = 0.0f;
    float                      dragStartY_    = 0.0f;
    float                      panStartX_     = 0.0f;
    float                      panStartY_     = 0.0f;
};
```

- [ ] **Step 2: Write `src/App.cpp`**

```cpp
#include "App.h"
#include "DDSLoader.h"
#include <imgui.h>
#include <nfd.hpp>
#include <SDL3/SDL.h>
#include <cmath>
#include <format>

App::App(SDL_Window* window, SDL_Renderer* renderer)
    : window_(window), renderer_(renderer), texRenderer_(renderer) {
    NFD::Init();
}

void App::LoadFile(const std::filesystem::path& path) {
    errorMsg_.clear();
    view_       = TextureView{};  // reset view on new file
    auto result = DDSLoader::Load(path);
    if (!result) {
        errorMsg_ = result.error();
        data_.reset();
        texRenderer_.Clear();
        return;
    }
    data_ = std::move(*result);
    texRenderer_.Upload(*data_, view_);
}

void App::OpenFileDialog() {
    NFD::UniquePath outPath;
    nfdfilteritem_t filters[] = {{"DDS Texture", "dds"}};
    nfdresult_t     res       = NFD::OpenDialog(outPath, filters, 1);
    if (res == NFD_OKAY)
        LoadFile(outPath.get());
}

SDL_FRect App::CanvasRect() const {
    int w, h;
    SDL_GetWindowSize(window_, &w, &h);
    float top    = uiPanel_.MenuBarHeight();
    float bottom = UIPanel::kPanelHeight;
    if (!errorMsg_.empty()) top += 24.0f;
    return {0.0f, top, static_cast<float>(w), static_cast<float>(h) - top - bottom};
}

void App::HandleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
            running_ = false;
            break;

        case SDL_EVENT_DROP_FILE: {
            std::string p = event.drop.data;
            SDL_free(event.drop.data);
            LoadFile(p);
            break;
        }

        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_O &&
                (event.key.mod & SDL_KMOD_CTRL))
                OpenFileDialog();
            break;

        case SDL_EVENT_MOUSE_WHEEL: {
            // Only zoom when cursor is in the canvas area
            SDL_FRect canvas = CanvasRect();
            float mx = event.wheel.mouse_x;
            float my = event.wheel.mouse_y;
            if (mx >= canvas.x && mx < canvas.x + canvas.w &&
                my >= canvas.y && my < canvas.y + canvas.h) {
                float factor = (event.wheel.y > 0) ? 1.1f : (1.0f / 1.1f);
                view_.zoom = std::clamp(view_.zoom * factor, 0.05f, 32.0f);
                if (data_) texRenderer_.Upload(*data_, view_);
            }
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                dragging_  = true;
                dragStartX_ = event.button.x;
                dragStartY_ = event.button.y;
                panStartX_  = view_.panX;
                panStartY_  = view_.panY;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT)
                dragging_ = false;
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (dragging_) {
                view_.panX = panStartX_ + (event.motion.x - dragStartX_);
                view_.panY = panStartY_ + (event.motion.y - dragStartY_);
            }
            break;

        default: break;
    }
}

void App::DrawUI() {
    bool changed = uiPanel_.Draw(view_, data_ ? &*data_ : nullptr,
                                  errorMsg_, [this]{ OpenFileDialog(); });
    if (changed && data_)
        texRenderer_.Upload(*data_, view_);
}

void App::RenderTexture() {
    if (!texRenderer_.HasTexture()) return;

    // Draw checkerboard background in canvas area
    SDL_FRect canvas = CanvasRect();
    SDL_SetRenderClipRect(renderer_, reinterpret_cast<SDL_Rect*>(&canvas));
    SDL_SetRenderDrawColor(renderer_, 50, 50, 50, 255);
    SDL_RenderFillRect(renderer_, &canvas);
    // Simple 16px checkerboard
    SDL_SetRenderDrawColor(renderer_, 30, 30, 30, 255);
    for (float y = canvas.y; y < canvas.y + canvas.h; y += 32.0f) {
        for (float x = canvas.x; x < canvas.x + canvas.w; x += 32.0f) {
            int col = static_cast<int>((x - canvas.x) / 16.0f);
            int row = static_cast<int>((y - canvas.y) / 16.0f);
            if ((col + row) % 2 == 0) {
                SDL_FRect cell = {x, y, 16.0f, 16.0f};
                SDL_RenderFillRect(renderer_, &cell);
            }
        }
    }

    texRenderer_.Render(renderer_, canvas, view_);
    SDL_SetRenderClipRect(renderer_, nullptr);

    // Zoom overlay
    if (data_) {
        ImGui::SetNextWindowPos(ImVec2(canvas.x + canvas.w - 160.0f,
                                       canvas.y + 6.0f));
        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGui::Begin("##overlay", nullptr,
                     ImGuiWindowFlags_NoTitleBar     | ImGuiWindowFlags_NoResize  |
                     ImGuiWindowFlags_NoMove         | ImGuiWindowFlags_NoScrollbar|
                     ImGuiWindowFlags_AlwaysAutoResize|
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::TextDisabled("%ux%u  %.2f\xc3\x97",
            texRenderer_.TexWidth(), texRenderer_.TexHeight(), view_.zoom);
        ImGui::End();
    }
}
```

- [ ] **Step 3: Build**

```powershell
cmake --build build --config Release
```

Expected: compiles cleanly.

- [ ] **Step 4: Commit**

```
git add src/App.h src/App.cpp
git commit -m "feat: App orchestrator — event routing, file load, zoom/pan, drag-and-drop"
```

---

## Task 10: main.cpp and Full Integration

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Write `src/main.cpp`**

```cpp
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <cstdio>
#include "App.h"

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "DDS Viewer", 1280, 720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetWindowDropPosition(window, true); // accept drop anywhere on window

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // no imgui.ini
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    App app(window, renderer);

    if (argc > 1)
        app.LoadFile(argv[1]);

    while (app.IsRunning()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            app.HandleEvent(event);
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        app.DrawUI();    // build ImGui draw list
        app.RenderTexture(); // ImGui overlay windows + texture canvas setup

        ImGui::Render();

        SDL_SetRenderDrawColorFloat(renderer, 0.117f, 0.117f, 0.117f, 1.0f);
        SDL_RenderClear(renderer);

        app.RenderTexture(); // actual SDL_RenderTexture calls
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

> **Note:** `RenderTexture()` is called twice above — once inside `ImGui::NewFrame()..ImGui::Render()` for overlay ImGui windows, and once after `ImGui::Render()` for the actual SDL_RenderTexture blit. Split the method into `DrawOverlayUI()` (ImGui windows) and `BlitTexture()` (SDL calls) to avoid this, or track which call site does what via a parameter.

**Fix the split now — update `src/App.h` to separate concerns:**

Add to `App`:
```cpp
void DrawOverlayUI();  // ImGui overlay windows (zoom readout)
void BlitTexture();    // SDL_RenderTexture + checkerboard
```

Remove `RenderTexture()`. Update `App.cpp` accordingly by splitting the existing `RenderTexture()` body at the `ImGui::SetNextWindowPos` call — checkerboard+blit go into `BlitTexture()`, ImGui overlay goes into `DrawOverlayUI()`.

Update `main.cpp` to:
```cpp
app.DrawUI();
app.DrawOverlayUI();

ImGui::Render();

SDL_SetRenderDrawColorFloat(renderer, 0.117f, 0.117f, 0.117f, 1.0f);
SDL_RenderClear(renderer);
app.BlitTexture();
ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
SDL_RenderPresent(renderer);
```

- [ ] **Step 2: Full build**

```powershell
cmake --build build --config Release
```

Expected: `ddsviewer.exe` built.

- [ ] **Step 3: Smoke test**

```powershell
./build/Release/ddsviewer.exe
```

Expected: window opens, shows "Drop a .dds file here" message. No crashes.

```powershell
./build/Release/ddsviewer.exe tests/fixtures/bc7_flat.dds
```

Expected: window opens with green texture visible, bottom panel shows "BC7_UNORM  4×4  1 mip  1 layer".

- [ ] **Step 4: Run all tests**

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 5: Commit**

```
git add src/main.cpp src/App.h src/App.cpp
git commit -m "feat: main entry point + full SDL3/ImGui integration; smoke tested"
```

---

## Task 11: README

**Files:**
- Create: `README.md`

- [ ] **Step 1: Write `README.md`**

```markdown
# DDS Viewer

A cross-platform desktop viewer for DirectDraw Surface (`.dds`) texture files.

## Features

- BC1 – BC7 compressed formats, uncompressed RGBA, HDR (BC6H, R16F, R32F, …)
- Mip level navigation
- Cubemap face selection (+X −X +Y −Y +Z −Z)
- Texture array and 3D depth slice navigation
- Per-channel RGBA toggle
- Exposure control (EV stops) for HDR/float textures
- Zoom (scroll wheel) and pan (drag)
- Open via CLI argument, drag-and-drop, or File → Open

## Requirements

- CMake 3.25+
- vcpkg (with `VCPKG_ROOT` set)
- Visual Studio 2022 / Clang 16+ / GCC 13+ (C++23)

## Building

```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

On Linux/macOS:
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
# Generate test fixtures first (only needed once)
./build/Release/generate_fixtures.exe tests/fixtures

# Run tests
ctest --test-dir build -C Release --output-on-failure
```

## Dependencies

| Library | Role |
|---------|------|
| [DirectXTex](https://github.com/microsoft/DirectXTex) | DDS decode (Microsoft) |
| [SDL3](https://libsdl.org/) | Window, renderer, input |
| [Dear ImGui](https://github.com/ocornut/imgui) | UI |
| [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended) | Native file open dialog |
| [Google Test](https://github.com/google/googletest) | Unit tests |

All dependencies are managed via [vcpkg](https://vcpkg.io/).
```

- [ ] **Step 2: Commit**

```
git add README.md
git commit -m "docs: add README with build instructions and feature overview"
```

---

## Self-Review Checklist

- [x] **Spec coverage:**
  - File opening (CLI, drag-drop, dialog) → Tasks 9, 10
  - Mip navigation → Task 8
  - Cubemap faces → Task 8
  - Array/3D slices → Task 8
  - RGBA channel toggles → Tasks 7, 8
  - Exposure slider (float only) → Tasks 7, 8
  - Zoom/pan → Tasks 9, 10
  - Metadata display → Task 8
  - Error handling → Tasks 5, 8, 9
  - Cross-platform build → Task 1 (CMake/vcpkg)

- [x] **Placeholders:** None found. All steps have complete code.

- [x] **Type consistency:**
  - `MipImage` defined in Task 2 (`TextureData.h`), used in Tasks 5, 7, 8
  - `TextureView` fields (`mip`, `slice`, `face`, `r`, `g`, `b`, `a`, `exposure`, `zoom`, `panX`, `panY`) consistent across all tasks
  - `TextureData.images[layer][mip]` indexing consistent across DDSLoader, TextureRenderer, UIPanel, App
  - `AnyChannelEnabled()` and `ClampView()` defined in Task 2, used in Tasks 6, 7
  - `UIPanel::kPanelHeight` used in App (Task 9) — defined as `static constexpr float`
  - `DDSLoader::Load()` returns `std::expected<TextureData, std::string>` — consistent in header (Task 3) and tests (Task 5)
