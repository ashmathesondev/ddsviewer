#pragma once
#include <functional>
#include <string>
#include "TextureData.h"
#include "TextureView.h"

class UIPanel {
public:
    static constexpr float kPanelHeight = 84.0f;

    // Returns true if view changed and texture must be re-uploaded.
    // data may be nullptr (no file loaded yet).
    bool Draw(TextureView& view, const TextureData* data,
              const std::string& errorMsg,
              const std::function<void()>& onOpenFile);

    float MenuBarHeight() const { return menuBarH_; }

private:
    float menuBarH_ = 0.0f;
};
