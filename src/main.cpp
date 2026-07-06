#define NOMINMAX
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include "App.h"
#include "Log.h"
#include "version.h"

int main(int argc, char* argv[]) {
    Log::Init();
    LOG_INFO("DDS Viewer {} starting ({})", DDSVIEWER_VERSION_STRING, DDSVIEWER_GIT_DESCRIBE);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        return 1;
    }
    LOG_TRACE("SDL initialized");

    SDL_Window* window = SDL_CreateWindow(
        "DDS Viewer " DDSVIEWER_VERSION_STRING, 1280, 720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        LOG_ERROR("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        LOG_ERROR("SDL_CreateRenderer failed: {}", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    LOG_TRACE("SDL window and renderer created");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    App app(window, renderer);

    if (argc > 1)
        app.LoadFile(argv[1]);

    while (app.IsRunning()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            app.HandleEvent(event);
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        app.DrawUI();

        ImGui::Render();

        SDL_SetRenderDrawColorFloat(renderer, 0.117f, 0.117f, 0.117f, 1.0f);
        SDL_RenderClear(renderer);

        app.BlitTexture();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    LOG_INFO("DDS Viewer shutdown");
    spdlog::shutdown();
    return 0;
}
