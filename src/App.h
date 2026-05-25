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
    ~App();

    void LoadFile(const std::filesystem::path& path);
    void HandleEvent(const SDL_Event& event);
    void DrawUI();        // ImGui windows (menu bar, panel, overlays)
    void BlitTexture();   // SDL_RenderTexture + checkerboard

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
    bool                       running_    = true;
    bool                       dragging_   = false;
    float                      dragStartX_ = 0.0f;
    float                      dragStartY_ = 0.0f;
    float                      panStartX_  = 0.0f;
    float                      panStartY_  = 0.0f;
};
