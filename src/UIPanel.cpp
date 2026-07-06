#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "UIPanel.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <algorithm>
#include <cctype>
#include <format>

static std::string FormatFileSize(uintmax_t bytes) {
    constexpr double kKiB = 1024.0;
    constexpr double kMiB = 1024.0 * 1024.0;

    if (bytes == 0) return "unknown size";
    if (bytes < 1024) return std::format("{} B", bytes);
    if (bytes < 1024 * 1024) return std::format("{:.1f} KiB", bytes / kKiB);
    return std::format("{:.1f} MiB", bytes / kMiB);
}

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

static std::string FormatHex(uint32_t value) {
    return std::format("0x{:08X}", value);
}

static std::string FormatFourCC(uint32_t value) {
    if (value == 0) return "0";

    char text[5] = {
        static_cast<char>(value & 0xFF),
        static_cast<char>((value >> 8) & 0xFF),
        static_cast<char>((value >> 16) & 0xFF),
        static_cast<char>((value >> 24) & 0xFF),
        '\0'
    };
    for (char& c : text) {
        if (!std::isprint(static_cast<unsigned char>(c))) c = '.';
    }
    return std::format("'{}' ({})", text, FormatHex(value));
}

static void DescRow(const char* name, uint32_t value) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(name);
    ImGui::TableNextColumn();
    ImGui::Text("%u", value);
    ImGui::TableNextColumn();
    const std::string hex = FormatHex(value);
    ImGui::TextUnformatted(hex.c_str());
}

struct FlagInfo {
    const char* name;
    uint32_t value;
    const char* meaning;
    bool legacy;
};

static void FlagRow(const FlagInfo& flag) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(flag.name);
    ImGui::TableNextColumn();
    const std::string hex = FormatHex(flag.value);
    ImGui::TextUnformatted(hex.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(flag.legacy ? "legacy" : "core");
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(flag.meaning);
}

static void FourCCRow(const char* name, uint32_t value) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(name);
    ImGui::TableNextColumn();
    const std::string text = FormatFourCC(value);
    ImGui::TextUnformatted(text.c_str());
    ImGui::TableNextColumn();
    const std::string hex = FormatHex(value);
    ImGui::TextUnformatted(hex.c_str());
}

static bool BeginDescTable(const char* id) {
    if (!ImGui::BeginTable(id, 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
        return false;
    }
    ImGui::TableSetupColumn("Field");
    ImGui::TableSetupColumn("Value");
    ImGui::TableSetupColumn("Hex");
    ImGui::TableHeadersRow();
    return true;
}

static constexpr FlagInfo kDdsHeaderFlags[] = {
    {"DDSD_CAPS",             0x00000001, "dwCaps is valid and describes surface complexity", false},
    {"DDSD_HEIGHT",           0x00000002, "dwHeight is valid", false},
    {"DDSD_WIDTH",            0x00000004, "dwWidth is valid", false},
    {"DDSD_PITCH",            0x00000008, "dwPitchOrLinearSize stores row pitch in bytes", false},
    {"DDSD_BACKBUFFERCOUNT",  0x00000020, "legacy DirectDraw back-buffer count field", true},
    {"DDSD_ZBUFFERBITDEPTH",  0x00000040, "legacy DirectDraw z-buffer bit depth field", true},
    {"DDSD_ALPHABITDEPTH",    0x00000080, "legacy DirectDraw alpha bit depth field", true},
    {"DDSD_LPSURFACE",        0x00000800, "legacy DirectDraw surface pointer field", true},
    {"DDSD_PIXELFORMAT",      0x00001000, "ddspf is valid and describes the pixel layout", false},
    {"DDSD_CKDESTOVERLAY",    0x00002000, "legacy destination overlay color key field", true},
    {"DDSD_CKDESTBLT",        0x00004000, "legacy destination blit color key field", true},
    {"DDSD_CKSRCOVERLAY",     0x00008000, "legacy source overlay color key field", true},
    {"DDSD_CKSRCBLT",         0x00010000, "legacy source blit color key field", true},
    {"DDSD_MIPMAPCOUNT",      0x00020000, "dwMipMapCount is valid", false},
    {"DDSD_REFRESHRATE",      0x00040000, "legacy refresh-rate field", true},
    {"DDSD_LINEARSIZE",       0x00080000, "dwPitchOrLinearSize stores total top-level image bytes", false},
    {"DDSD_TEXTURESTAGE",     0x00100000, "legacy texture-stage field", true},
    {"DDSD_FVF",              0x00200000, "legacy flexible-vertex-format field", true},
    {"DDSD_SRCVBHANDLE",      0x00400000, "legacy source vertex-buffer handle field", true},
    {"DDSD_DEPTH",            0x00800000, "dwDepth is valid for a volume texture", false},
};

static std::string FormatFlagsSummary(uint32_t flags) {
    std::string summary;
    uint32_t knownMask = 0;
    for (const FlagInfo& flag : kDdsHeaderFlags) {
        knownMask |= flag.value;
        if ((flags & flag.value) == 0) continue;

        if (!summary.empty()) summary += " | ";
        summary += flag.name;
    }

    const uint32_t unknownFlags = flags & ~knownMask;
    if (unknownFlags != 0) {
        if (!summary.empty()) summary += " | ";
        summary += std::format("UNKNOWN({})", FormatHex(unknownFlags));
    }
    return summary.empty() ? "none" : summary;
}

static void DrawDdsHeaderFlags(uint32_t flags) {
    if (!ImGui::CollapsingHeader("dwFlags breakdown", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const std::string summary = FormatFlagsSummary(flags);
    ImGui::TextWrapped("%s", summary.c_str());

    if (!ImGui::BeginTable("dds_header_flags", 4,
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        return;
    }
    ImGui::TableSetupColumn("Flag");
    ImGui::TableSetupColumn("Bit");
    ImGui::TableSetupColumn("Kind");
    ImGui::TableSetupColumn("Meaning");
    ImGui::TableHeadersRow();

    uint32_t knownMask = 0;
    for (const FlagInfo& flag : kDdsHeaderFlags) {
        knownMask |= flag.value;
        if ((flags & flag.value) != 0) {
            FlagRow(flag);
        }
    }

    const uint32_t unknownFlags = flags & ~knownMask;
    if (unknownFlags != 0) {
        const FlagInfo unknown{
            "Unknown bits",
            unknownFlags,
            "Bits not recognized as standard DDS_HEADER dwFlags values",
            false,
        };
        FlagRow(unknown);
    }

    ImGui::EndTable();
}

static void DrawDdsDescSection(const DdsDescInfo& desc) {
    if (ImGui::CollapsingHeader("DDS_HEADER desc", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (BeginDescTable("dds_desc")) {
            DescRow("magic", desc.magic);
            DescRow("dwSize", desc.size);
            DescRow("dwFlags", desc.flags);
            DescRow("dwHeight", desc.height);
            DescRow("dwWidth", desc.width);
            DescRow("dwPitchOrLinearSize", desc.pitchOrLinearSize);
            DescRow("dwDepth", desc.depth);
            DescRow("dwMipMapCount", desc.mipMapCount);
            DescRow("dwCaps", desc.caps);
            DescRow("dwCaps2", desc.caps2);
            DescRow("dwCaps3", desc.caps3);
            DescRow("dwCaps4", desc.caps4);
            DescRow("dwReserved2", desc.reserved2);
            ImGui::EndTable();
        }
    }
    DrawDdsHeaderFlags(desc.flags);

    if (ImGui::CollapsingHeader("ddspf", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (BeginDescTable("dds_pixel_format")) {
            DescRow("dwSize", desc.pixelFormat.size);
            DescRow("dwFlags", desc.pixelFormat.flags);
            FourCCRow("dwFourCC", desc.pixelFormat.fourCC);
            DescRow("dwRGBBitCount", desc.pixelFormat.rgbBitCount);
            DescRow("dwRBitMask", desc.pixelFormat.rBitMask);
            DescRow("dwGBitMask", desc.pixelFormat.gBitMask);
            DescRow("dwBBitMask", desc.pixelFormat.bBitMask);
            DescRow("dwABitMask", desc.pixelFormat.aBitMask);
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("dwReserved1")) {
        if (BeginDescTable("dds_reserved1")) {
            for (size_t i = 0; i < desc.reserved1.size(); ++i) {
                const std::string name = std::format("dwReserved1[{}]", i);
                DescRow(name.c_str(), desc.reserved1[i]);
            }
            ImGui::EndTable();
        }
    }

    if (desc.dx10Header && ImGui::CollapsingHeader("DDS_HEADER_DXT10", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (BeginDescTable("dds_dx10")) {
            DescRow("dxgiFormat", desc.dx10Header->dxgiFormat);
            DescRow("resourceDimension", desc.dx10Header->resourceDimension);
            DescRow("miscFlag", desc.dx10Header->miscFlag);
            DescRow("arraySize", desc.dx10Header->arraySize);
            DescRow("miscFlags2", desc.dx10Header->miscFlags2);
            ImGui::EndTable();
        }
    }
}

static void DrawDdsDescWindow(bool& open, const TextureData& data) {
    ImGui::SetNextWindowSize(ImVec2(520.0f, 580.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("DDS Desc", &open)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted(data.fileName.c_str());
    ImGui::Separator();
    if (data.ddsDesc) {
        DrawDdsDescSection(*data.ddsDesc);
    } else {
        ImGui::TextDisabled("No DDS desc data is available for this file.");
    }
    ImGui::End();
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
        ImGui::TextDisabled("Drop a .dds, .png, .jpg, .tga, .bmp, .gif, or .tiff file or use File \xe2\x86\x92 Open");
        ImGui::End();
        return false;
    }

    // File info
    ImGui::BeginGroup();
    ImGui::TextUnformatted(data->fileName.empty() ? data->containerName.c_str() : data->fileName.c_str());
    if (data->ddsDesc) {
        ImGui::SameLine();
        if (ImGui::SmallButton("DDS Desc")) {
            showDdsDesc_ = true;
        }
    }
    ImGui::TextDisabled("%s image  %ux%u",
        data->containerName.empty() ? "Loaded" : data->containerName.c_str(),
        data->baseWidth, data->baseHeight);
    const std::string fileSize = FormatFileSize(data->fileSizeBytes);
    ImGui::TextDisabled("%s  %s", data->formatName.c_str(), fileSize.c_str());
    if (data->mipCount > 1 || data->layerCount > 1 || data->isCubemap || data->is3D) {
        ImGui::TextDisabled("%u mip%s  %u layer%s%s%s",
            data->mipCount,   data->mipCount   > 1 ? "s" : "",
            data->layerCount, data->layerCount > 1 ? "s" : "",
            data->isCubemap ? "  cubemap" : "",
            data->is3D ? "  3D" : "");
    }
    ImGui::EndGroup();

    // Mip selector (hidden for 3D textures — DDSLoader stores only mip 0 for 3D)
    if (!data->is3D && data->mipCount > 1) {
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
    if (showDdsDesc_) {
        DrawDdsDescWindow(showDdsDesc_, *data);
    }
    return changed;
}
