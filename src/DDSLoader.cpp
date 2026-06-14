#include "DDSLoader.h"
#include "Log.h"
#include <DirectXTex.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <system_error>

static std::string LowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::ranges::transform(ext, ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

static bool IsFloatFormat(DXGI_FORMAT fmt) {
    return fmt == DXGI_FORMAT_BC6H_SF16          ||
           fmt == DXGI_FORMAT_BC6H_UF16          ||
           fmt == DXGI_FORMAT_R16_FLOAT          ||
           fmt == DXGI_FORMAT_R16G16_FLOAT       ||
           fmt == DXGI_FORMAT_R16G16B16A16_FLOAT ||
           fmt == DXGI_FORMAT_R32_FLOAT          ||
           fmt == DXGI_FORMAT_R32G32_FLOAT       ||
           fmt == DXGI_FORMAT_R32G32B32_FLOAT    ||
           fmt == DXGI_FORMAT_R32G32B32A32_FLOAT ||
           fmt == DXGI_FORMAT_R11G11B10_FLOAT    ||
           fmt == DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
}

static std::string FormatName(DXGI_FORMAT fmt) {
    switch (fmt) {
        case DXGI_FORMAT_BC1_UNORM:             return "BC1_UNORM";
        case DXGI_FORMAT_BC1_UNORM_SRGB:        return "BC1_UNORM_SRGB";
        case DXGI_FORMAT_BC2_UNORM:             return "BC2_UNORM";
        case DXGI_FORMAT_BC2_UNORM_SRGB:        return "BC2_UNORM_SRGB";
        case DXGI_FORMAT_BC3_UNORM:             return "BC3_UNORM";
        case DXGI_FORMAT_BC3_UNORM_SRGB:        return "BC3_UNORM_SRGB";
        case DXGI_FORMAT_BC4_UNORM:             return "BC4_UNORM";
        case DXGI_FORMAT_BC4_SNORM:             return "BC4_SNORM";
        case DXGI_FORMAT_BC5_UNORM:             return "BC5_UNORM";
        case DXGI_FORMAT_BC5_SNORM:             return "BC5_SNORM";
        case DXGI_FORMAT_BC6H_UF16:             return "BC6H_UF16";
        case DXGI_FORMAT_BC6H_SF16:             return "BC6H_SF16";
        case DXGI_FORMAT_BC7_UNORM:             return "BC7_UNORM";
        case DXGI_FORMAT_BC7_UNORM_SRGB:        return "BC7_UNORM_SRGB";
        case DXGI_FORMAT_R8G8B8A8_UNORM:        return "R8G8B8A8_UNORM";
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return "R8G8B8A8_UNORM_SRGB";
        case DXGI_FORMAT_R16G16B16A16_FLOAT:    return "R16G16B16A16_FLOAT";
        case DXGI_FORMAT_R32G32B32A32_FLOAT:    return "R32G32B32A32_FLOAT";
        case DXGI_FORMAT_R11G11B10_FLOAT:       return "R11G11B10_FLOAT";
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:    return "R9G9B9E5_SHAREDEXP";
        case DXGI_FORMAT_B8G8R8A8_UNORM:        return "B8G8R8A8_UNORM";
        case DXGI_FORMAT_B8G8R8X8_UNORM:        return "B8G8R8X8_UNORM";
        case DXGI_FORMAT_B5G6R5_UNORM:          return "B5G6R5_UNORM";
        case DXGI_FORMAT_B5G5R5A1_UNORM:        return "B5G5R5A1_UNORM";
        default: return std::format("DXGI_FORMAT_{}", static_cast<uint32_t>(fmt));
    }
}

static bool IsWICExtension(const std::string& ext) {
    return ext == ".png"  ||
           ext == ".jpg"  ||
           ext == ".jpeg" ||
           ext == ".bmp"  ||
           ext == ".gif"  ||
           ext == ".tif"  ||
           ext == ".tiff" ||
           ext == ".wdp"  ||
           ext == ".jxr"  ||
           ext == ".hdp";
}

static std::string ContainerName(const std::string& ext) {
    if (ext == ".dds") return "DDS";
    if (ext == ".png") return "PNG";
    if (ext == ".jpg" || ext == ".jpeg") return "JPEG";
    if (ext == ".tga") return "Targa";
    if (ext == ".bmp") return "BMP";
    if (ext == ".gif") return "GIF";
    if (ext == ".tif" || ext == ".tiff") return "TIFF";
    if (ext == ".wdp" || ext == ".jxr" || ext == ".hdp") return "JPEG XR";
    return "Image";
}

static MipImage ExtractImage(const DirectX::Image& src, bool isFloat) {
    MipImage mi;
    mi.width  = static_cast<uint32_t>(src.width);
    mi.height = static_cast<uint32_t>(src.height);
    const size_t bytesPerPixel = isFloat ? 16u : 4u;
    mi.pixels.resize(mi.width * mi.height * bytesPerPixel);
    // Copy row-by-row: rowPitch may include padding
    for (uint32_t y = 0; y < mi.height; ++y) {
        const uint8_t* srcRow = src.pixels + y * src.rowPitch;
        uint8_t*       dstRow = mi.pixels.data() + y * mi.width * bytesPerPixel;
        std::memcpy(dstRow, srcRow, mi.width * bytesPerPixel);
    }
    return mi;
}

std::expected<TextureData, std::string> DDSLoader::Load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        LOG_ERROR("File not found: {}", path.string());
        return std::unexpected("File not found: " + path.string());
    }

    DirectX::TexMetadata  metadata{};
    DirectX::ScratchImage raw;

    const std::string ext = LowerExtension(path);
    HRESULT hr = S_OK;
    if (ext == ".dds") {
        LOG_TRACE("Parsing DDS: {}", path.filename().string());
        hr = DirectX::LoadFromDDSFile(
            path.wstring().c_str(), DirectX::DDS_FLAGS_NONE, &metadata, raw);
    } else if (ext == ".tga") {
        LOG_TRACE("Parsing TGA: {}", path.filename().string());
        hr = DirectX::LoadFromTGAFile(path.wstring().c_str(), &metadata, raw);
#ifdef _WIN32
    } else if (IsWICExtension(ext)) {
        LOG_TRACE("Parsing WIC image: {}", path.filename().string());
        hr = DirectX::LoadFromWICFile(
            path.wstring().c_str(), DirectX::WIC_FLAGS_NONE, &metadata, raw);
#endif
    } else {
        auto msg = "Unsupported image format: " + (ext.empty() ? path.filename().string() : ext);
        LOG_ERROR("{}", msg);
        return std::unexpected(msg);
    }

    if (FAILED(hr)) {
        auto msg = std::format("Failed to parse image: HRESULT 0x{:08X}", static_cast<uint32_t>(hr));
        LOG_ERROR("{}", msg);
        return std::unexpected(msg);
    }

    const bool        isFloat      = IsFloatFormat(metadata.format);
    const DXGI_FORMAT targetFormat = isFloat
        ? DXGI_FORMAT_R32G32B32A32_FLOAT
        : DXGI_FORMAT_R8G8B8A8_UNORM;

    const DirectX::ScratchImage* source = &raw;
    DirectX::ScratchImage        converted;

    if (DirectX::IsCompressed(metadata.format)) {
        LOG_TRACE("Decompressing {} -> target format", FormatName(metadata.format));
        hr = DirectX::Decompress(raw.GetImages(), raw.GetImageCount(),
                                 metadata, targetFormat, converted);
        if (FAILED(hr)) {
            auto msg = "Cannot decompress format: " + FormatName(metadata.format);
            LOG_ERROR("{}", msg);
            return std::unexpected(msg);
        }
        source = &converted;
    } else if (metadata.format != targetFormat) {
        LOG_TRACE("Converting {} -> target format", FormatName(metadata.format));
        hr = DirectX::Convert(raw.GetImages(), raw.GetImageCount(), metadata,
                              targetFormat, DirectX::TEX_FILTER_DEFAULT,
                              DirectX::TEX_THRESHOLD_DEFAULT, converted);
        if (FAILED(hr)) {
            auto msg = "Cannot convert format: " + FormatName(metadata.format);
            LOG_ERROR("{}", msg);
            return std::unexpected(msg);
        }
        source = &converted;
    }

    TextureData data;
    data.fileName       = path.filename().string();
    data.containerName  = ContainerName(ext);
    {
        std::error_code ec;
        data.fileSizeBytes = std::filesystem::file_size(path, ec);
        if (ec) data.fileSizeBytes = 0;
    }
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
        // 3D textures: store depth slices at mip 0 as layers; skip higher mips
        data.layerCount = static_cast<uint32_t>(metadata.depth);
        data.mipCount   = 1;
        data.images.resize(data.layerCount);
        for (uint32_t d = 0; d < data.layerCount; ++d) {
            const DirectX::Image* img = source->GetImage(0, 0, d);
            if (!img) return std::unexpected("Internal error: GetImage returned null for depth slice " + std::to_string(d));
            data.images[d].push_back(ExtractImage(*img, isFloat));
        }
    } else {
        data.layerCount = static_cast<uint32_t>(metadata.arraySize);
        data.images.resize(data.layerCount);
        for (uint32_t layer = 0; layer < data.layerCount; ++layer) {
            data.images[layer].resize(data.mipCount);
            for (uint32_t mip = 0; mip < data.mipCount; ++mip) {
                const DirectX::Image* img = source->GetImage(mip, layer, 0);
                if (!img) return std::unexpected("Internal error: GetImage returned null at layer " + std::to_string(layer) + " mip " + std::to_string(mip));
                data.images[layer][mip] = ExtractImage(*img, isFloat);
            }
        }
    }

    return data;
}
