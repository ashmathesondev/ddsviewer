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
#include <string_view>

static std::string FormatFileSize(uintmax_t bytes) {
    constexpr double kKiB = 1024.0;
    constexpr double kMiB = 1024.0 * 1024.0;

    if (bytes == 0) return "unknown size";
    if (bytes < 1024) return std::format("{} B", bytes);
    if (bytes < 1024 * 1024) return std::format("{:.1f} KiB", bytes / kKiB);
    return std::format("{:.1f} MiB", bytes / kMiB);
}

static std::string FormatDimensions(const TextureData& data) {
    if (data.is3D) {
        return std::format("{} x {} x {}", data.baseWidth, data.baseHeight, data.depth);
    }
    return std::format("{} x {}", data.baseWidth, data.baseHeight);
}

static std::string FormatTextureType(const TextureData& data) {
    if (data.is3D) return "3D volume";
    if (data.isCubemap) return data.layerCount > 6 ? "Cube map array" : "Cube map";
    if (data.layerCount > 1) return "2D texture array";
    return "2D texture";
}

static std::string FormatMipSummary(const TextureData& data) {
    return std::format("{} level{}", data.mipCount, data.mipCount == 1 ? "" : "s");
}

static std::string FormatSliceSummary(const TextureData& data) {
    if (data.is3D) {
        return std::format("{} depth slice{}", data.layerCount, data.layerCount == 1 ? "" : "s");
    }
    if (data.isCubemap) {
        return data.layerCount > 6
            ? std::format("{} faces/slices", data.layerCount)
            : "6 faces";
    }
    return std::format("{} layer{}", data.layerCount, data.layerCount == 1 ? "" : "s");
}

static std::string FormatPreviewStorage(const TextureData& data) {
    return data.isFloat ? "RGBA32F preview" : "RGBA8 preview";
}

static void SummaryCell(const char* label, const std::string& value) {
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", label);
    ImGui::TextWrapped("%s", value.c_str());
}

static void SummaryCell(const char* label, const char* value) {
    SummaryCell(label, std::string(value));
}

static void DetailRow(const char* label, const std::string& value) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", label);
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", value.c_str());
}

static void DetailRow(const char* label, const char* value) {
    DetailRow(label, std::string(value));
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

static void FieldNameWithTooltip(const char* name, const char* description) {
    ImGui::TextUnformatted(name);
    if (description && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0f);
        ImGui::TextUnformatted(description);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static const char* DescribeDdsHeaderField(const char* name) {
    const std::string_view field{name};
    if (field == "magic") return "DDS file signature. Valid DDS files start with the four bytes 'DDS '.";
    if (field == "dwSize") return "Size of the DDS_HEADER structure in bytes. The standard value is 124.";
    if (field == "dwFlags") return "Bitmask that says which DDS_HEADER fields contain valid data.";
    if (field == "dwHeight") return "Texture height in pixels at mip level 0.";
    if (field == "dwWidth") return "Texture width in pixels at mip level 0.";
    if (field == "dwPitchOrLinearSize") return "For uncompressed textures, row pitch in bytes. For compressed textures, total bytes for the top mip level.";
    if (field == "dwDepth") return "Depth in slices for a volume texture. Usually 0 for 1D, 2D, arrays, and cube maps.";
    if (field == "dwMipMapCount") return "Number of mip levels stored in the file when DDSD_MIPMAPCOUNT is set.";
    if (field == "dwCaps") return "Surface capability flags, such as texture, mipmap, or complex surface.";
    if (field == "dwCaps2") return "Additional capability flags, including cube-map faces and volume texture markers.";
    if (field == "dwCaps3") return "Legacy DirectDraw capability field. Modern DDS files usually leave this as 0.";
    if (field == "dwCaps4") return "Legacy DirectDraw capability field. Modern DDS files usually leave this as 0.";
    if (field == "dwReserved2") return "Reserved header field. DDS writers should set this to 0.";
    return nullptr;
}

static const char* DescribeDdsPixelFormatField(const char* name) {
    const std::string_view field{name};
    if (field == "dwSize") return "Size of the DDS_PIXELFORMAT structure in bytes. The standard value is 32.";
    if (field == "dwFlags") return "Bitmask that describes how to interpret the pixel format fields.";
    if (field == "dwFourCC") return "Four-character code for compressed formats or the DX10 extended header marker.";
    if (field == "dwRGBBitCount") return "Bits per pixel for uncompressed RGB, RGBA, luminance, or alpha formats.";
    if (field == "dwRBitMask") return "Bit mask for the red channel in uncompressed formats.";
    if (field == "dwGBitMask") return "Bit mask for the green channel in uncompressed formats.";
    if (field == "dwBBitMask") return "Bit mask for the blue channel in uncompressed formats.";
    if (field == "dwABitMask") return "Bit mask for the alpha channel in uncompressed formats.";
    return nullptr;
}

static const char* DescribeDdsDx10Field(const char* name) {
    const std::string_view field{name};
    if (field == "dxgiFormat") return "DXGI_FORMAT enum value that identifies the texture's exact pixel format.";
    if (field == "resourceDimension") return "Resource shape: 1D, 2D, 3D, buffer, or unknown.";
    if (field == "miscFlag") return "Additional resource flags. In DDS files this commonly marks cube textures.";
    if (field == "arraySize") return "Number of texture array elements. Cube maps use one array element per cube.";
    if (field == "miscFlags2") return "Additional DX10 flags. The low bits encode alpha mode.";
    return nullptr;
}

static std::string FormatResourceDimension(uint32_t value) {
    switch (value) {
        case 0: return "Unknown";
        case 1: return "Buffer";
        case 2: return "1D texture";
        case 3: return "2D texture";
        case 4: return "3D texture";
        default: return std::format("Unknown ({})", value);
    }
}

static std::string FormatAlphaMode(uint32_t miscFlags2) {
    switch (miscFlags2 & 0x7u) {
        case 0: return "Unknown";
        case 1: return "Straight";
        case 2: return "Premultiplied";
        case 3: return "Opaque";
        case 4: return "Custom";
        default: return std::format("Reserved ({})", miscFlags2 & 0x7u);
    }
}

static std::string FormatPixelFlagsSummary(uint32_t flags) {
    struct PixelFlag {
        const char* name;
        uint32_t value;
    };
    static constexpr PixelFlag kPixelFlags[] = {
        {"Alpha pixels", 0x00000001},
        {"Alpha only", 0x00000002},
        {"FourCC", 0x00000004},
        {"RGB", 0x00000040},
        {"YUV", 0x00000200},
        {"Luminance", 0x00020000},
    };

    std::string summary;
    uint32_t knownMask = 0;
    for (const PixelFlag& flag : kPixelFlags) {
        knownMask |= flag.value;
        if ((flags & flag.value) == 0) continue;

        if (!summary.empty()) summary += ", ";
        summary += flag.name;
    }

    const uint32_t unknownFlags = flags & ~knownMask;
    if (unknownFlags != 0) {
        if (!summary.empty()) summary += ", ";
        summary += std::format("Unknown {}", FormatHex(unknownFlags));
    }
    return summary.empty() ? "None" : summary;
}

static void DescRow(const char* name, uint32_t value, const char* description = nullptr) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    FieldNameWithTooltip(name, description);
    ImGui::TableNextColumn();
    ImGui::Text("%u", value);
    ImGui::TableNextColumn();
    const std::string hex = FormatHex(value);
    ImGui::TextUnformatted(hex.c_str());
}

static void DescRow(const char* name, const std::string& valueText, uint32_t value, const char* description = nullptr) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    FieldNameWithTooltip(name, description);
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", valueText.c_str());
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

static std::string FormatFlagsSummary(uint32_t flags);

template <size_t N>
static std::string FormatFlagNames(uint32_t flags, const FlagInfo (&knownFlags)[N]) {
    std::string summary;
    uint32_t knownMask = 0;
    for (const FlagInfo& flag : knownFlags) {
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

static void FourCCRow(const char* name, uint32_t value, const char* description = nullptr) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    FieldNameWithTooltip(name, description);
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

static void DrawDdsSummary(const TextureData& data, const DdsDescInfo& desc) {
    if (!ImGui::CollapsingHeader("Summary", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (!ImGui::BeginTable("dds_summary", 2,
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    DetailRow("Texture", FormatTextureType(data));
    DetailRow("Dimensions", FormatDimensions(data));
    DetailRow("Format", data.formatName.empty() ? "Unknown" : data.formatName);
    DetailRow("Container", data.containerName.empty() ? "Image" : data.containerName);
    DetailRow("File size", FormatFileSize(data.fileSizeBytes));
    DetailRow("Mip levels", FormatMipSummary(data));
    DetailRow(data.is3D ? "Depth" : data.isCubemap ? "Faces" : "Layers", FormatSliceSummary(data));
    DetailRow("Preview storage", FormatPreviewStorage(data));
    DetailRow("Header flags", FormatFlagsSummary(desc.flags));
    DetailRow("Pixel flags", FormatPixelFlagsSummary(desc.pixelFormat.flags));
    DetailRow("FourCC", FormatFourCC(desc.pixelFormat.fourCC));
    if (desc.dx10Header) {
        DetailRow("DX10 dimension", FormatResourceDimension(desc.dx10Header->resourceDimension));
        DetailRow("DX10 array size", std::to_string(desc.dx10Header->arraySize));
        DetailRow("Alpha mode", FormatAlphaMode(desc.dx10Header->miscFlags2));
    }

    ImGui::EndTable();
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

static constexpr FlagInfo kDdsCapsFlags[] = {
    {"DDSCAPS_ALPHA",          0x00000002, "Legacy alpha-only surface", true},
    {"DDSCAPS_BACKBUFFER",     0x00000004, "Legacy back buffer surface", true},
    {"DDSCAPS_COMPLEX",        0x00000008, "Surface has related surfaces, such as mipmaps or cube-map faces", false},
    {"DDSCAPS_FLIP",           0x00000010, "Legacy flipping surface chain", true},
    {"DDSCAPS_FRONTBUFFER",    0x00000020, "Legacy front buffer surface", true},
    {"DDSCAPS_OFFSCREENPLAIN", 0x00000040, "Legacy offscreen plain surface", true},
    {"DDSCAPS_OVERLAY",        0x00000080, "Legacy overlay surface", true},
    {"DDSCAPS_PALETTE",        0x00000100, "Legacy palette surface", true},
    {"DDSCAPS_PRIMARYSURFACE", 0x00000200, "Legacy primary display surface", true},
    {"DDSCAPS_SYSTEMMEMORY",   0x00000800, "Legacy system-memory surface", true},
    {"DDSCAPS_TEXTURE",        0x00001000, "Surface is a texture", false},
    {"DDSCAPS_VIDEOMEMORY",    0x00004000, "Legacy video-memory surface", true},
    {"DDSCAPS_VISIBLE",        0x00008000, "Legacy visible surface", true},
    {"DDSCAPS_WRITEONLY",      0x00010000, "Legacy write-only surface", true},
    {"DDSCAPS_ZBUFFER",        0x00020000, "Legacy z-buffer surface", true},
    {"DDSCAPS_OWNDC",          0x00040000, "Legacy surface owns a device context", true},
    {"DDSCAPS_LIVEVIDEO",      0x00080000, "Legacy live video surface", true},
    {"DDSCAPS_HWCODEC",        0x00100000, "Legacy hardware codec surface", true},
    {"DDSCAPS_MODEX",          0x00200000, "Legacy Mode X surface", true},
    {"DDSCAPS_MIPMAP",         0x00400000, "Surface is one level in a mipmap chain", false},
};

static constexpr FlagInfo kDdsCaps2Flags[] = {
    {"DDSCAPS2_CUBEMAP",           0x00000200, "Texture is a cube map", false},
    {"DDSCAPS2_CUBEMAP_POSITIVEX", 0x00000400, "Cube map contains the +X face", false},
    {"DDSCAPS2_CUBEMAP_NEGATIVEX", 0x00000800, "Cube map contains the -X face", false},
    {"DDSCAPS2_CUBEMAP_POSITIVEY", 0x00001000, "Cube map contains the +Y face", false},
    {"DDSCAPS2_CUBEMAP_NEGATIVEY", 0x00002000, "Cube map contains the -Y face", false},
    {"DDSCAPS2_CUBEMAP_POSITIVEZ", 0x00004000, "Cube map contains the +Z face", false},
    {"DDSCAPS2_CUBEMAP_NEGATIVEZ", 0x00008000, "Cube map contains the -Z face", false},
    {"DDSCAPS2_VOLUME",            0x00200000, "Texture is a 3D volume texture", false},
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

static void DrawDdsDescSection(const TextureData& data, const DdsDescInfo& desc) {
    DrawDdsSummary(data, desc);

    if (ImGui::CollapsingHeader("DDS_HEADER desc", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (BeginDescTable("dds_desc")) {
            FourCCRow("magic", desc.magic, DescribeDdsHeaderField("magic"));
            DescRow("dwSize", desc.size, DescribeDdsHeaderField("dwSize"));
            DescRow("dwFlags", desc.flags, DescribeDdsHeaderField("dwFlags"));
            DescRow("dwHeight", desc.height, DescribeDdsHeaderField("dwHeight"));
            DescRow("dwWidth", desc.width, DescribeDdsHeaderField("dwWidth"));
            DescRow("dwPitchOrLinearSize", desc.pitchOrLinearSize, DescribeDdsHeaderField("dwPitchOrLinearSize"));
            DescRow("dwDepth", desc.depth, DescribeDdsHeaderField("dwDepth"));
            DescRow("dwMipMapCount", desc.mipMapCount, DescribeDdsHeaderField("dwMipMapCount"));
            DescRow("dwCaps", FormatFlagNames(desc.caps, kDdsCapsFlags), desc.caps, DescribeDdsHeaderField("dwCaps"));
            DescRow("dwCaps2", FormatFlagNames(desc.caps2, kDdsCaps2Flags), desc.caps2, DescribeDdsHeaderField("dwCaps2"));
            DescRow("dwCaps3", desc.caps3, DescribeDdsHeaderField("dwCaps3"));
            DescRow("dwCaps4", desc.caps4, DescribeDdsHeaderField("dwCaps4"));
            DescRow("dwReserved2", desc.reserved2, DescribeDdsHeaderField("dwReserved2"));
            ImGui::EndTable();
        }
    }
    DrawDdsHeaderFlags(desc.flags);

    if (ImGui::CollapsingHeader("ddspf", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (BeginDescTable("dds_pixel_format")) {
            DescRow("dwSize", desc.pixelFormat.size, DescribeDdsPixelFormatField("dwSize"));
            DescRow("dwFlags", desc.pixelFormat.flags, DescribeDdsPixelFormatField("dwFlags"));
            FourCCRow("dwFourCC", desc.pixelFormat.fourCC, DescribeDdsPixelFormatField("dwFourCC"));
            DescRow("dwRGBBitCount", desc.pixelFormat.rgbBitCount, DescribeDdsPixelFormatField("dwRGBBitCount"));
            DescRow("dwRBitMask", desc.pixelFormat.rBitMask, DescribeDdsPixelFormatField("dwRBitMask"));
            DescRow("dwGBitMask", desc.pixelFormat.gBitMask, DescribeDdsPixelFormatField("dwGBitMask"));
            DescRow("dwBBitMask", desc.pixelFormat.bBitMask, DescribeDdsPixelFormatField("dwBBitMask"));
            DescRow("dwABitMask", desc.pixelFormat.aBitMask, DescribeDdsPixelFormatField("dwABitMask"));
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("dwReserved1")) {
        if (BeginDescTable("dds_reserved1")) {
            for (size_t i = 0; i < desc.reserved1.size(); ++i) {
                const std::string name = std::format("dwReserved1[{}]", i);
                DescRow(name.c_str(), desc.reserved1[i], "Reserved DDS_HEADER field. DDS writers should set this to 0.");
            }
            ImGui::EndTable();
        }
    }

    if (desc.dx10Header && ImGui::CollapsingHeader("DDS_HEADER_DXT10", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (BeginDescTable("dds_dx10")) {
            DescRow("dxgiFormat", desc.dx10Header->dxgiFormat, DescribeDdsDx10Field("dxgiFormat"));
            DescRow("resourceDimension", desc.dx10Header->resourceDimension, DescribeDdsDx10Field("resourceDimension"));
            DescRow("miscFlag", desc.dx10Header->miscFlag, DescribeDdsDx10Field("miscFlag"));
            DescRow("arraySize", desc.dx10Header->arraySize, DescribeDdsDx10Field("arraySize"));
            DescRow("miscFlags2", desc.dx10Header->miscFlags2, DescribeDdsDx10Field("miscFlags2"));
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
        DrawDdsDescSection(data, *data.ddsDesc);
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

    const float infoWidth = std::min(540.0f, std::max(320.0f, io.DisplaySize.x * 0.42f));
    if (ImGui::BeginTable("file_summary", 4,
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX,
        ImVec2(infoWidth, 0.0f))) {
        SummaryCell("Type", FormatTextureType(*data));
        SummaryCell("Size", FormatDimensions(*data));
        SummaryCell("Format", data->formatName.empty() ? "Unknown" : data->formatName);
        SummaryCell("File", FormatFileSize(data->fileSizeBytes));
        SummaryCell("Mips", FormatMipSummary(*data));
        SummaryCell(data->is3D ? "Depth" : data->isCubemap ? "Faces" : "Layers", FormatSliceSummary(*data));
        SummaryCell("Container", data->containerName.empty() ? "Image" : data->containerName);
        SummaryCell("Pixels", FormatPreviewStorage(*data));
        ImGui::EndTable();
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
