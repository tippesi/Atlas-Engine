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

    };

}