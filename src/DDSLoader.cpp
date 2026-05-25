#include "DDSLoader.h"

std::expected<TextureData, std::string> DDSLoader::Load(const std::filesystem::path&) {
    return std::unexpected(std::string("DDSLoader not yet implemented"));
}
