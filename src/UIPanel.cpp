#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "UIPanel.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <algorithm>

static void VerticalSeparator() {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float h = ImGui::GetFrameHeight() * 2.0f;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x, pos.y + h),
        ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
    ImGui::Dummy(ImVec2(1.0f, h));
}

static bool ChannelButton(const char* label, bool& value, ImVec4 activeColor) {
    ImGui::PushStyleColor(ImGuiCol_Button,
        value ? activeColor : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    const bool clicked = ImGui::Button(label);
    ImGui::PopStyleColor();
    if (clicked) value = !value;
    return clicked;
}

bool UIPanel::Draw(TextureView& view, const TextureData* data,
                   const std::string& errorMsg,
                   const std::function<void()>& onOpenFile) {
    bool changed = false;

    // ── Menu bar ──────────────────────────────────────────────────────────
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open...", "Ctrl+O")) onOpenFile();
            ImGui::EndMenu();
        }
        menuBarH_ = ImGui::GetFrameHeight();
        ImGui::EndMainMenuBar();
    }

    // ── Error strip (shown only when errorMsg is non-empty) ───────────────
    if (!errorMsg.empty()) {
        const ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0.0f, menuBarH_));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 24.0f));
        ImGui::SetNextWindowBgAlpha(0.9f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        ImGui::Begin("##err", nullptr,
                     ImGuiWindowFlags_NoTitleBar       |
                     ImGuiWindowFlags_NoResize         |
                     ImGuiWindowFlags_NoMove           |
                     ImGuiWindowFlags_NoScrollbar      |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::TextUnformatted(errorMsg.c_str());
        ImGui::End();
        ImGui::PopStyleColor();
    }

    // ── Bottom panel ──────────────────────────────────────────────────────
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - kPanelHeight));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, kPanelHeight));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("##panel", nullptr,
                 ImGuiWindowFlags_NoTitleBar       |
                 ImGuiWindowFlags_NoResize         |
                 ImGuiWindowFlags_NoMove           |
                 ImGuiWindowFlags_NoScrollbar      |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (!data) {
        ImGui::TextDisabled("Drop a .dds file or use File \xe2\x86\x92 Open");
        ImGui::End();
        return false;
    }

    // File info
    ImGui::BeginGroup();
    ImGui::TextUnformatted(data->formatName.c_str());
    ImGui::TextDisabled("%ux%u", data->baseWidth, data->baseHeight);
    ImGui::TextDisabled("%u mip%s  %u layer%s",
        data->mipCount,   data->mipCount   > 1 ? "s" : "",
        data->layerCount, data->layerCount > 1 ? "s" : "");
    ImGui::EndGroup();

    // Mip selector (hidden for 3D textures — DDSLoader stores only mip 0 for 3D)
    if (!data->is3D) {
        ImGui::SameLine();
        VerticalSeparator();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("Mip");
        ImGui::SetNextItemWidth(60.0f);
        int mip = view.mip;
        if (ImGui::InputInt("##mip", &mip, 1)) {
            view.mip = std::clamp(mip, 0, static_cast<int>(data->mipCount) - 1);
            changed = true;
        }
        if (!data->images.empty() && !data->images[0].empty()) {
            const MipImage& curMip = data->images[0][
                std::clamp(view.mip, 0, static_cast<int>(data->images[0].size()) - 1)];
            ImGui::TextDisabled("%ux%u", curMip.width, curMip.height);
        }
        ImGui::EndGroup();
    }

    // Cubemap face buttons
    if (data->isCubemap) {
        ImGui::SameLine();
        VerticalSeparator();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("Face");
        static const char* kFaceLabels[6] = {"+X","-X","+Y","-Y","+Z","-Z"};
        for (int f = 0; f < 6; ++f) {
            if (f > 0) ImGui::SameLine();
            const bool selected = (view.face == f);
            ImGui::PushStyleColor(ImGuiCol_Button,
                selected ? ImVec4(0.2f, 0.4f, 0.7f, 1.0f)
                         : ImGui::GetStyle().Colors[ImGuiCol_Button]);
            if (ImGui::Button(kFaceLabels[f])) { view.face = f; changed = true; }
            ImGui::PopStyleColor();
        }
        ImGui::EndGroup();
    }

    // Array / 3D slice selector (hidden for plain 2D and cubemaps)
    if (!data->isCubemap && data->layerCount > 1) {
        ImGui::SameLine();
        VerticalSeparator();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled(data->is3D ? "Depth" : "Slice");
        ImGui::SetNextItemWidth(60.0f);
        int slice = view.slice;
        if (ImGui::InputInt("##slice", &slice, 1)) {
            view.slice = std::clamp(slice, 0, static_cast<int>(data->layerCount) - 1);
            changed = true;
        }
        ImGui::TextDisabled("of %u", data->layerCount);
        ImGui::EndGroup();
    }

    ImGui::SameLine();
    VerticalSeparator();
    ImGui::SameLine();

    // RGBA channel toggles
    ImGui::BeginGroup();
    ImGui::TextDisabled("Channels");
    if (ChannelButton("R", view.r, ImVec4(0.7f, 0.2f, 0.2f, 1.0f))) changed = true;
    ImGui::SameLine();
    if (ChannelButton("G", view.g, ImVec4(0.2f, 0.7f, 0.2f, 1.0f))) changed = true;
    ImGui::SameLine();
    if (ChannelButton("B", view.b, ImVec4(0.2f, 0.2f, 0.7f, 1.0f))) changed = true;
    ImGui::SameLine();
    if (ChannelButton("A", view.a, ImVec4(0.5f, 0.5f, 0.5f, 1.0f))) changed = true;
    ImGui::EndGroup();

    // Exposure slider (float formats only)
    if (data->isFloat) {
        ImGui::SameLine();
        VerticalSeparator();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("Exposure");
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderFloat("##ev", &view.exposure, -10.0f, 10.0f, "%.1f EV"))
            changed = true;
        ImGui::EndGroup();
    }

    ImGui::End();
    return changed;
}
