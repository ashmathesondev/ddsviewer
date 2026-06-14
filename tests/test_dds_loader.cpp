#include <gtest/gtest.h>
#define NOMINMAX
#include <DirectXTex.h>
#ifdef _WIN32
#include <objbase.h>
#include <wincodec.h>
#endif
#include <chrono>
#include <cstdio>
#include <filesystem>
#include "DDSLoader.h"

namespace fs = std::filesystem;
static const fs::path FX = FIXTURES_DIR;

#ifdef _WIN32
class ComFixture : public ::testing::Test {
protected:
    void SetUp() override {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        uninitializeCom_ = SUCCEEDED(hr);
    }

    void TearDown() override {
        if (uninitializeCom_) CoUninitialize();
    }

private:
    bool uninitializeCom_ = false;
};
#endif

static fs::path MakeTempImagePath(const char* extension) {
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path dir = fs::temp_directory_path() / ("ddsviewer_tests_" + std::to_string(ticks));
    fs::create_directories(dir);
    return dir / ("image" + std::string(extension));
}

static DirectX::ScratchImage MakeRGBA8Pattern() {
    DirectX::ScratchImage img;
    img.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 2, 2, 1, 1);
    const DirectX::Image* im = img.GetImage(0, 0, 0);

    uint8_t* p = im->pixels;
    p[0] = 255; p[1] = 128; p[2] = 0;   p[3] = 255;
    p[4] = 0;   p[5] = 0;   p[6] = 255; p[7] = 255;

    uint8_t* row1 = im->pixels + im->rowPitch;
    row1[0] = 0;   row1[1] = 255; row1[2] = 0;   row1[3] = 255;
    row1[4] = 255; row1[5] = 0;   row1[6] = 255; row1[7] = 255;

    return img;
}

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
    ASSERT_FALSE(d.images.empty());
    ASSERT_FALSE(d.images[0].empty());
    const auto& px = d.images[0][0].pixels;
    ASSERT_GE(px.size(), 8u);
    // pixel (0,0) = orange: R=255 G=128 B=0 A=255
    EXPECT_EQ(px[0], 255u);
    EXPECT_EQ(px[1], 128u);
    EXPECT_EQ(px[2],   0u);
    EXPECT_EQ(px[3], 255u);
    // pixel (1,0) = blue: R=0 G=0 B=255 A=255
    EXPECT_EQ(px[4],   0u);
    EXPECT_EQ(px[5],   0u);
    EXPECT_EQ(px[6], 255u);
    EXPECT_EQ(px[7], 255u);
}

TEST(DDSLoader, BC1Decompress) {
    auto result = DDSLoader::Load(FX / "bc1_flat.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    const TextureData& d = *result;
    EXPECT_EQ(d.baseWidth,  4u);
    EXPECT_EQ(d.baseHeight, 4u);
    EXPECT_EQ(d.originalFormat, DXGI_FORMAT_BC1_UNORM);
    EXPECT_FALSE(d.isFloat);
    ASSERT_FALSE(d.images[0][0].pixels.empty());
    const auto& px = d.images[0][0].pixels;
    // After decompression: red channel dominant
    EXPECT_GT(px[0], 200u);
    EXPECT_LT(px[1],  50u);
    EXPECT_LT(px[2],  50u);
}

TEST(DDSLoader, BC7Decompress) {
    auto result = DDSLoader::Load(FX / "bc7_flat.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    const TextureData& d = *result;
    EXPECT_EQ(d.originalFormat, DXGI_FORMAT_BC7_UNORM);
    ASSERT_FALSE(d.images[0][0].pixels.empty());
    const auto& px = d.images[0][0].pixels;
    // After decompression: green channel dominant
    EXPECT_LT(px[0],  50u);
    EXPECT_GT(px[1], 200u);
    EXPECT_LT(px[2],  50u);
}

TEST(DDSLoader, MipChain) {
    auto result = DDSLoader::Load(FX / "mipchain.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    const TextureData& d = *result;
    EXPECT_EQ(d.baseWidth,  64u);
    EXPECT_EQ(d.baseHeight, 64u);
    EXPECT_EQ(d.mipCount,    7u);
    EXPECT_EQ(d.layerCount,  1u);
    ASSERT_EQ(d.images.size(),    1u);
    ASSERT_EQ(d.images[0].size(), 7u);
    // Last mip is 1x1
    EXPECT_EQ(d.images[0][6].width,  1u);
    EXPECT_EQ(d.images[0][6].height, 1u);
}

TEST(DDSLoader, Cubemap) {
    auto result = DDSLoader::Load(FX / "cubemap.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    const TextureData& d = *result;
    EXPECT_TRUE(d.isCubemap);
    EXPECT_EQ(d.layerCount, 6u);
    ASSERT_EQ(d.images.size(), 6u);
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
    ASSERT_EQ(d.images.size(), 3u);
    // Slice 0 = red, slice 1 = green, slice 2 = blue
    EXPECT_GT(d.images[0][0].pixels[0], 200u); // slice 0 R
    EXPECT_GT(d.images[1][0].pixels[1], 200u); // slice 1 G
    EXPECT_GT(d.images[2][0].pixels[2], 200u); // slice 2 B
}

TEST(DDSLoader, FormatNameNotEmpty) {
    auto result = DDSLoader::Load(FX / "rgba8_uncompressed.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_FALSE(result->formatName.empty());
}

TEST(DDSLoader, FileMetadata) {
    auto result = DDSLoader::Load(FX / "rgba8_uncompressed.dds");
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->fileName, "rgba8_uncompressed.dds");
    EXPECT_EQ(result->containerName, "DDS");
    EXPECT_GT(result->fileSizeBytes, 0u);
}

#ifdef _WIN32
TEST_F(ComFixture, PNG) {
    const fs::path path = MakeTempImagePath(".png");
    DirectX::ScratchImage source = MakeRGBA8Pattern();
    const HRESULT hr = DirectX::SaveToWICFile(
        *source.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE,
        GUID_ContainerFormatPng, path.wstring().c_str());
    ASSERT_TRUE(SUCCEEDED(hr));

    auto result = DDSLoader::Load(path);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->baseWidth, 2u);
    EXPECT_EQ(result->baseHeight, 2u);
    EXPECT_EQ(result->mipCount, 1u);
    EXPECT_EQ(result->layerCount, 1u);
    EXPECT_EQ(result->fileName, "image.png");
    EXPECT_EQ(result->containerName, "PNG");
    EXPECT_GT(result->fileSizeBytes, 0u);
    EXPECT_FALSE(result->isFloat);
    ASSERT_GE(result->images[0][0].pixels.size(), 8u);
    EXPECT_EQ(result->images[0][0].pixels[0], 255u);
    EXPECT_EQ(result->images[0][0].pixels[1], 128u);
    EXPECT_EQ(result->images[0][0].pixels[2], 0u);
    EXPECT_EQ(result->images[0][0].pixels[3], 255u);
}

TEST_F(ComFixture, JPEG) {
    const fs::path path = MakeTempImagePath(".jpg");
    DirectX::ScratchImage source = MakeRGBA8Pattern();
    const HRESULT hr = DirectX::SaveToWICFile(
        *source.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE,
        GUID_ContainerFormatJpeg, path.wstring().c_str());
    ASSERT_TRUE(SUCCEEDED(hr));

    auto result = DDSLoader::Load(path);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->baseWidth, 2u);
    EXPECT_EQ(result->baseHeight, 2u);
    EXPECT_EQ(result->mipCount, 1u);
    EXPECT_EQ(result->layerCount, 1u);
    EXPECT_EQ(result->fileName, "image.jpg");
    EXPECT_EQ(result->containerName, "JPEG");
    EXPECT_GT(result->fileSizeBytes, 0u);
    EXPECT_FALSE(result->isFloat);
    EXPECT_EQ(result->images[0][0].pixels.size(), 16u);
}
#endif

TEST(DDSLoader, TGA) {
    const fs::path path = MakeTempImagePath(".tga");
    DirectX::ScratchImage source = MakeRGBA8Pattern();
    const HRESULT hr = DirectX::SaveToTGAFile(
        *source.GetImage(0, 0, 0), path.wstring().c_str(), &source.GetMetadata());
    ASSERT_TRUE(SUCCEEDED(hr));

    auto result = DDSLoader::Load(path);
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->baseWidth, 2u);
    EXPECT_EQ(result->baseHeight, 2u);
    EXPECT_EQ(result->mipCount, 1u);
    EXPECT_EQ(result->layerCount, 1u);
    EXPECT_EQ(result->fileName, "image.tga");
    EXPECT_EQ(result->containerName, "Targa");
    EXPECT_GT(result->fileSizeBytes, 0u);
    EXPECT_FALSE(result->isFloat);
    ASSERT_GE(result->images[0][0].pixels.size(), 8u);
    EXPECT_EQ(result->images[0][0].pixels[0], 255u);
    EXPECT_EQ(result->images[0][0].pixels[1], 128u);
    EXPECT_EQ(result->images[0][0].pixels[2], 0u);
    EXPECT_EQ(result->images[0][0].pixels[3], 255u);
}

TEST(DDSLoader, UnsupportedExtension) {
    const fs::path path = MakeTempImagePath(".txt");
    {
        FILE* file = nullptr;
        ASSERT_EQ(fopen_s(&file, path.string().c_str(), "wb"), 0);
        ASSERT_NE(file, nullptr);
        fputs("not an image", file);
        fclose(file);
    }

    auto result = DDSLoader::Load(path);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Unsupported image format"), std::string::npos);
}
