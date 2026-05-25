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
    int layer = data.isCubemap ? view.face : view.slice;
    layer     = std::clamp(layer, 0, static_cast<int>(data.layerCount) - 1);
    int mip   = std::clamp(view.mip, 0, static_cast<int>(data.mipCount) - 1);

    const MipImage& src = data.images[layer][mip];

    if (!texture_ || texW_ != static_cast<int>(src.width) ||
        texH_ != static_cast<int>(src.height)) {
        Clear();
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     static_cast<int>(src.width),
                                     static_cast<int>(src.height));
        texW_ = static_cast<int>(src.width);
        texH_ = static_cast<int>(src.height);
    }

    std::vector<uint8_t> rgba8 = ToRGBA8(src, data.isFloat, view);
    SDL_UpdateTexture(texture_, nullptr, rgba8.data(), texW_ * 4);
}

void TextureRenderer::Render(SDL_Renderer* renderer, SDL_FRect canvasRect,
                             const TextureView& view) const {
    if (!texture_) return;

    float texW = static_cast<float>(texW_) * view.zoom;
    float texH = static_cast<float>(texH_) * view.zoom;
    SDL_FRect dst = {
        canvasRect.x + (canvasRect.w - texW) * 0.5f + view.panX,
        canvasRect.y + (canvasRect.h - texH) * 0.5f + view.panY,
        texW, texH
    };
    SDL_RenderTexture(renderer, texture_, nullptr, &dst);
}

std::vector<uint8_t> TextureRenderer::ToRGBA8(const MipImage& img,
                                               bool isFloat,
                                               const TextureView& view) {
    const bool anyOn = AnyChannelEnabled(view);
    const bool r = anyOn ? view.r : true;
    const bool g = anyOn ? view.g : true;
    const bool b = anyOn ? view.b : true;
    const bool a = anyOn ? view.a : true;

    const size_t count = static_cast<size_t>(img.width) * img.height;
    std::vector<uint8_t> out(count * 4);

    if (isFloat) {
        const float scale = std::pow(2.0f, view.exposure);
        const float* src  = reinterpret_cast<const float*>(img.pixels.data());
        auto toU8 = [](float v) -> uint8_t {
            return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
        };
        for (size_t i = 0; i < count; ++i) {
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
