#pragma once

#include <string>
#include <imgui.h>

#include "System.h"
#include "ImguiWrapper.h"
#include "texture/Texture2D.h"

namespace Atlas::ImguiExtension {

    class UIElements {

    public:
        static void TexturePreview(Ref<ImguiWrapper>& wrapper, const Texture::Texture* texture);

        static void TextureView(Ref<ImguiWrapper>& wrapper, const Texture::Texture* texture, float maxTextureSize = 0.0f);

        static void Tooltip(const char* text, float size = 35.0f);

        // Properties with a muted label on the left and the value filling the rest of the row. Call PropertyLabel
        // before each value, then EndProperties. Same layout as the property tables of the editor
        static bool BeginProperties(const char* id);

        // Starts a row, the next item is the value and fills its column
        static void PropertyLabel(const char* label, const char* tooltip = nullptr);

        static void EndProperties();

    };

}