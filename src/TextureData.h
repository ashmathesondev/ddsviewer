#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <DirectXTex.h>

struct MipImage {
    std::vector<uint8_t> pixels; // RGBA8 (4 bytes/px) or RGBA32F (16 bytes/px)
    uint32_t width  = 0;
    uint32_t height = 0;
};

struct DdsPixelFormatInfo {
    uint32_t size        = 0;
    uint32_t flags       = 0;
    uint32_t fourCC      = 0;
    uint32_t rgbBitCount = 0;
    uint32_t rBitMask    = 0;
    uint32_t gBitMask    = 0;
    uint32_t bBitMask    = 0;
    uint32_t aBitMask    = 0;
};

struct DdsDx10HeaderInfo {
    uint32_t dxgiFormat        = 0;
    uint32_t resourceDimension = 0;
    uint32_t miscFlag          = 0;
    uint32_t arraySize         = 0;
    uint32_t miscFlags2        = 0;
};

struct DdsDescInfo {
    uint32_t magic             = 0;
    uint32_t size              = 0;
    uint32_t flags             = 0;
    uint32_t height            = 0;
    uint32_t width             = 0;
    uint32_t pitchOrLinearSize = 0;
    uint32_t depth             = 0;
    uint32_t mipMapCount       = 0;
    std::array<uint32_t, 11> reserved1{};
    DdsPixelFormatInfo pixelFormat;
    uint32_t caps      = 0;
    uint32_t caps2     = 0;
    uint32_t caps3     = 0;
    uint32_t caps4     = 0;
    uint32_t reserved2 = 0;
    std::optional<DdsDx10HeaderInfo> dx10Header;
};

struct TextureData {
    std::string fileName;
    std::string containerName;
    uintmax_t   fileSizeBytes  = 0;
    DXGI_FORMAT originalFormat = DXGI_FORMAT_UNKNOWN;
    bool        isFloat        = false;  // true → pixels are RGBA32F
    uint32_t    baseWidth      = 0;
    uint32_t    baseHeight     = 0;
    uint32_t    depth          = 1;
    uint32_t    mipCount       = 0;
    uint32_t    layerCount     = 1;  // arraySize, 6 for cubemaps, or depth for 3D
    bool        isCubemap      = false;
    bool        is3D           = false;
    std::string formatName;
    std::optional<DdsDescInfo> ddsDesc;

    // images[layerIndex][mipIndex]
    // Cubemaps:   layer = face (0=+X 1=-X 2=+Y 3=-Y 4=+Z 5=-Z)
    // Arrays/2D:  layer = array index (0 for plain 2D)
    // 3D:         layer = depth slice at mip 0, mipCount = 1
    std::vector<std::vector<MipImage>> images;
};
