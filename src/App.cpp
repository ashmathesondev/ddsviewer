#define NOMINMAX
#include "App.h"
#include "DDSLoader.h"
#include "Log.h"
#include <imgui.h>
#include <nfd.hpp>
#include <cmath>
#include <iterator>

App::App(SDL_Window* window, SDL_Renderer* renderer)
    : window_(window), renderer_(renderer), texRenderer_(renderer) {
    NFD::Init();
}

App::~App() {
    NFD::Quit();
}

void App::LoadFile(const std::filesystem::path& path) {
    LOG_INFO("Loading file: {}", path.string());
    errorMsg_.clear();
    view_ = TextureView{};
    auto result = DDSLoader::Load(path);
    if (!result) {
        errorMsg_ = result.error();
        LOG_ERROR("Load failed: {}", errorMsg_);
        data_.reset();
        texRenderer_.Clear();
        return;
    }
    data_ = std::move(*result);
    LOG_INFO("Loaded: {}x{} {} ({} mip{}, {} layer{})",
        data_->baseWidth, data_->baseHeight, data_->formatName,
        data_->mipCount, data_->mipCount != 1 ? "s" : "",
        data_->layerCount, data_->layerCount != 1 ? "s" : "");
    texRenderer_.Upload(*data_, view_);
}

void App::OpenFileDialog() {
    NFD::UniquePath outPath;
    nfdfilteritem_t filters[] = {
#ifdef _WIN32
        {"Supported Images", "dds,png,jpg,jpeg,tga,bmp,gif,tif,tiff,wdp,jxr,hdp"},
#else
        {"Supported Images", "dds,tga"},
#endif
        {"DDS Texture", "dds"},
#ifdef _WIN32
        {"PNG Image", "png"},
        {"JPEG Image", "jpg,jpeg"},
#endif
        {"Targa Image", "tga"}
    };
    const nfdresult_t res = NFD::OpenDialog(outPath, filters, std::size(filters));
    if (res == NFD_OKAY)
        LoadFile(outPath.get());
    else if (res == NFD_CANCEL)
        LOG_TRACE("File dialog cancelled");
    else
        LOG_ERROR("File dialog error: {}", NFD::GetError());
}

SDL_FRect App::CanvasRect() const {
    int w = 0, h = 0;
    SDL_GetWindowSize(window_, &w, &h);
    float top = uiPanel_.MenuBarHeight();
    if (!errorMsg_.empty()) top += 24.0f;
    return {0.0f, top,
            static_cast<float>(w),
            static_cast<float>(h) - top - UIPanel::kPanelHeight};
}

void App::HandleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
            running_ = false;
            break;

        case SDL_EVENT_DROP_FILE: {
            LoadFile(std::filesystem::path(event.drop.data));
            break;
        }

        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_O &&
                (event.key.mod & SDL_KMOD_CTRL))
                OpenFileDialog();
            break;

        case SDL_EVENT_MOUSE_WHEEL: {
            if (!ImGui::GetIO().WantCaptureMouse) {
                const SDL_FRect canvas = CanvasRect();
                const float mx = event.wheel.mouse_x;
                const float my = event.wheel.mouse_y;
                if (mx >= canvas.x && mx < canvas.x + canvas.w &&
                    my >= canvas.y && my < canvas.y + canvas.h) {
                    const float factor = (event.wheel.y > 0) ? 1.1f : (1.0f / 1.1f);
                    view_.zoom = std::clamp(view_.zoom * factor, 0.05f, 32.0f);
                    if (data_) texRenderer_.Upload(*data_, view_);
                }
            }
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT &&
                !ImGui::GetIO().WantCaptureMouse) {
                dragging_   = true;
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
    const bool changed = uiPanel_.Draw(view_, data_ ? &*data_ : nullptr,
                                        errorMsg_, [this]{ OpenFileDialog(); });
    if (changed && data_)
        texRenderer_.Upload(*data_, view_);

    // Zoom / coords overlay (top-right corner of canvas)
    if (data_) {
        const SDL_FRect canvas = CanvasRect();
        ImGui::SetNextWindowPos(ImVec2(canvas.x + canvas.w - 170.0f, canvas.y + 6.0f));
        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGui::Begin("##overlay", nullptr,
                     ImGuiWindowFlags_NoTitleBar      |
                     ImGuiWindowFlags_NoResize        |
                     ImGuiWindowFlags_NoMove          |
                     ImGuiWindowFlags_NoScrollbar     |
                     ImGuiWindowFlags_AlwaysAutoResize|
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::TextDisabled("%ux%u  %.2f\xc3\x97",
            texRenderer_.TexWidth(), texRenderer_.TexHeight(), view_.zoom);
        ImGui::End();
    }
}

void App::BlitTexture() {
    if (!texRenderer_.HasTexture()) return;

    const SDL_FRect canvas = CanvasRect();

    // Checkerboard background
    SDL_SetRenderClipRect(renderer_, nullptr);
    {
        SDL_FRect bg = canvas;
        SDL_SetRenderDrawColorFloat(renderer_, 0.196f, 0.196f, 0.196f, 1.0f);
        SDL_RenderFillRect(renderer_, &bg);
        SDL_SetRenderDrawColorFloat(renderer_, 0.118f, 0.118f, 0.118f, 1.0f);
        constexpr float cell = 16.0f;
        for (float cy = canvas.y; cy < canvas.y + canvas.h; cy += cell) {
            for (float cx = canvas.x; cx < canvas.x + canvas.w; cx += cell) {
                const int col = static_cast<int>((cx - canvas.x) / cell);
                const int row = static_cast<int>((cy - canvas.y) / cell);
                if ((col + row) % 2 == 0) {
                    SDL_FRect cell_rect = {cx, cy, cell, cell};
                    SDL_RenderFillRect(renderer_, &cell_rect);
                }
            }
        }
    }

    texRenderer_.Render(renderer_, canvas, view_);
}
