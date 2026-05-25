#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdint>
#include <vector>
#include <SDL3/SDL.h>
#include "TextureData.h"
#include "TextureView.h"

class TextureRenderer {
public:
    explicit TextureRenderer(SDL_Renderer* renderer);
    ~TextureRenderer();

    // Upload the currently selected slice/face/mip to GPU. Call when file loads or view changes.
    void Upload(const TextureData& data, const TextureView& view);
    void Clear();

    // Blit texture into canvasRect with zoom and pan from view.
    void Render(SDL_Renderer* renderer, SDL_FRect canvasRect, const TextureView& view) const;

    bool HasTexture() const { return texture_ != nullptr; }
    int  TexWidth()   const { return texW_; }
    int  TexHeight()  const { return texH_; }

private:
    static std::vector<uint8_t> ToRGBA8(const MipImage& img,
                                         bool isFloat,
                                         const TextureView& view);

    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture*  texture_  = nullptr;
    int           texW_     = 0;
    int           texH_     = 0;
};
