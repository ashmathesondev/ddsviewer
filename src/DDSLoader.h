#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include "TextureData.h"

class DDSLoader {
public:
    static std::expected<TextureData, std::string> Load(const std::filesystem::path& path);
};
