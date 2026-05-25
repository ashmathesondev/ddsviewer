#include <DirectXTex.h>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

static void FillImage(const DirectX::Image& img, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (size_t y = 0; y < img.height; ++y) {
        uint8_t* row = img.pixels + y * img.rowPitch;
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
        std::fprintf(stderr, "Failed to save %s: 0x%08X\n",
                     path.string().c_str(), static_cast<unsigned>(hr));
        return false;
    }
    std::printf("  wrote %s\n", path.filename().string().c_str());
    return true;
}

int main(int argc, char* argv[]) {
    fs::path outDir = argc > 1 ? argv[1] : "tests/fixtures";
    fs::create_directories(outDir);
    std::printf("Generating fixtures in %s\n", outDir.string().c_str());

    // rgba8_uncompressed.dds — 4x4, known pixel values
    {
        DirectX::ScratchImage img;
        img.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, 1, 1);
        const DirectX::Image* im = img.GetImage(0, 0, 0);
        std::memset(im->pixels, 0, im->slicePitch);
        // pixel (0,0) = orange: R=255 G=128 B=0 A=255
        im->pixels[0] = 255; im->pixels[1] = 128; im->pixels[2] = 0; im->pixels[3] = 255;
        // pixel (1,0) = blue: R=0 G=0 B=255 A=255
        im->pixels[4] = 0;   im->pixels[5] = 0;   im->pixels[6] = 255; im->pixels[7] = 255;
        Save(img, outDir / "rgba8_uncompressed.dds");
    }

    // bc1_flat.dds — 4x4, solid red
    {
        DirectX::ScratchImage src;
        src.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, 1, 1);
        FillImage(*src.GetImage(0, 0, 0), 255, 0, 0, 255);
        DirectX::ScratchImage bc1;
        DirectX::Compress(src.GetImages(), src.GetImageCount(), src.GetMetadata(),
                          DXGI_FORMAT_BC1_UNORM, DirectX::TEX_COMPRESS_DEFAULT,
                          DirectX::TEX_THRESHOLD_DEFAULT, bc1);
        Save(bc1, outDir / "bc1_flat.dds");
    }

    // bc7_flat.dds — 4x4, solid green
    {
        DirectX::ScratchImage src;
        src.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, 1, 1);
        FillImage(*src.GetImage(0, 0, 0), 0, 255, 0, 255);
        DirectX::ScratchImage bc7;
        DirectX::Compress(src.GetImages(), src.GetImageCount(), src.GetMetadata(),
                          DXGI_FORMAT_BC7_UNORM, DirectX::TEX_COMPRESS_DEFAULT,
                          DirectX::TEX_THRESHOLD_DEFAULT, bc7);
        Save(bc7, outDir / "bc7_flat.dds");
    }

    // mipchain.dds — 64x64 RGBA8 with full mip chain
    {
        DirectX::ScratchImage src;
        src.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 64, 64, 1, 1);
        FillImage(*src.GetImage(0, 0, 0), 100, 149, 237, 255);
        DirectX::ScratchImage mipped;
        DirectX::GenerateMipMaps(*src.GetImage(0, 0, 0),
                                 DirectX::TEX_FILTER_DEFAULT, 0, mipped);
        Save(mipped, outDir / "mipchain.dds");
    }

    // cubemap.dds — 4x4 cubemap, each face a different colour
    {
        DirectX::ScratchImage cube;
        cube.InitializeCube(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, 1, 1);
        const uint8_t faceColors[6][4] = {
            {255,   0,   0, 255}, // +X red
            {  0, 255,   0, 255}, // -X green
            {  0,   0, 255, 255}, // +Y blue
            {255, 255,   0, 255}, // -Y yellow
            {  0, 255, 255, 255}, // +Z cyan
            {255,   0, 255, 255}, // -Z magenta
        };
        for (int f = 0; f < 6; ++f)
            FillImage(*cube.GetImage(0, f, 0),
                      faceColors[f][0], faceColors[f][1],
                      faceColors[f][2], faceColors[f][3]);
        Save(cube, outDir / "cubemap.dds");
    }

    // array.dds — 4x4, 3-slice texture array
    {
        DirectX::ScratchImage arr;
        arr.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, 3, 1);
        const uint8_t sliceColors[3][4] = {
            {255,   0,   0, 255},
            {  0, 255,   0, 255},
            {  0,   0, 255, 255},
        };
        for (int s = 0; s < 3; ++s)
            FillImage(*arr.GetImage(0, s, 0),
                      sliceColors[s][0], sliceColors[s][1],
                      sliceColors[s][2], sliceColors[s][3]);
        Save(arr, outDir / "array.dds");
    }

    std::printf("Done.\n");
    return 0;
}
