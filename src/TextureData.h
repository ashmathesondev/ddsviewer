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
    uint32_t    layerCount     = 1;  // arraySize, 6 for cubemaps, or depth for 3D
    bool        isCubemap      = false;
    bool        is3D           = false;
    std::string formatName;

    // images[layerIndex][mipIndex]
    // Cubemaps:   layer = face (0=+X 1=-X 2=+Y 3=-Y 4=+Z 5=-Z)
    // Arrays/2D:  layer = array index (0 for plain 2D)
    // 3D:         layer = depth slice at mip 0, mipCount = 1
    std::vector<std::vector<MipImage>> images;
};
