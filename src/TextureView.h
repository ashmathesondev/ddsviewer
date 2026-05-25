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
